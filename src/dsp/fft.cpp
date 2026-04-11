#include "src/dsp/fft.hpp"

#include <cmath>
#include <stdexcept>

namespace sdr_analyzer::dsp {

bool is_power_of_two(std::size_t value) {
  return value > 0 && (value & (value - 1)) == 0;
}

std::vector<float> hann_window(const std::size_t size) {
  // A Hann window is a reasonable default for this analyzer because it reduces
  // spectral leakage without widening narrow peaks as aggressively as some
  // heavier windows used for measurement-oriented tooling.
  std::vector<float> window(size, 1.0f);
  if (size <= 1) {
    return window;
  }
  constexpr double kTwoPi = 6.28318530717958647692;
  for (std::size_t index = 0; index < size; ++index) {
    window[index] = static_cast<float>(
        0.5 - 0.5 * std::cos(kTwoPi * index / static_cast<double>(size - 1)));
  }
  return window;
}

void compute_fft(std::vector<std::complex<float>> &data) {
  const std::size_t count = data.size();
  if (!is_power_of_two(count)) {
    throw std::invalid_argument("FFT size must be a power of two.");
  }

  // Reorder the input into bit-reversed order before the iterative butterfly
  // stages of the radix-2 Cooley-Tukey decimation-in-time FFT.
  std::size_t j = 0;
  for (std::size_t i = 1; i < count; ++i) {
    std::size_t bit = count >> 1;
    while ((j & bit) != 0U) {
      j &= ~bit;
      bit >>= 1;
    }
    j |= bit;
    if (i < j) {
      std::swap(data[i], data[j]);
    }
  }

  constexpr double kTwoPi = 6.28318530717958647692;
  // Each stage doubles the butterfly width until the full transform is built.
  for (std::size_t len = 2; len <= count; len <<= 1U) {
    const double angle = -kTwoPi / static_cast<double>(len);
    const std::complex<float> step(static_cast<float>(std::cos(angle)),
                                   static_cast<float>(std::sin(angle)));
    for (std::size_t offset = 0; offset < count; offset += len) {
      std::complex<float> omega(1.0f, 0.0f);
      const std::size_t half = len >> 1U;
      for (std::size_t k = 0; k < half; ++k) {
        // Standard butterfly: combine the even bin with the twiddle-rotated odd
        // bin to produce the lower and upper halves of the current stage.
        const auto even = data[offset + k];
        const auto odd = omega * data[offset + k + half];
        data[offset + k] = even + odd;
        data[offset + k + half] = even - odd;
        omega *= step;
      }
    }
  }
}

} // namespace sdr_analyzer::dsp

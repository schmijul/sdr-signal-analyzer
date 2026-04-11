#pragma once

#include <complex>
#include <vector>

namespace sdr_analyzer::dsp {

bool is_power_of_two(std::size_t value);

// Computes an in-place radix-2 FFT. The input size must be a power of two.
void compute_fft(std::vector<std::complex<float>> &data);

// Builds a Hann window for leakage reduction before spectrum estimation.
std::vector<float> hann_window(std::size_t size);

} // namespace sdr_analyzer::dsp

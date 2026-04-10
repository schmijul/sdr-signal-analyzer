#pragma once

#include <complex>
#include <vector>

namespace sdr_analyzer::dsp {

bool IsPowerOfTwo(std::size_t value);

// Computes an in-place radix-2 FFT. The input size must be a power of two.
void ComputeFft(std::vector<std::complex<float>> &data);

// Builds a Hann window for leakage reduction before spectrum estimation.
std::vector<float> HannWindow(std::size_t size);

} // namespace sdr_analyzer::dsp

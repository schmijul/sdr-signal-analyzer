#pragma once

#include <complex>
#include <vector>

namespace sdr_analyzer::dsp {

bool IsPowerOfTwo(std::size_t value);
void ComputeFft(std::vector<std::complex<float>>& data);
std::vector<float> HannWindow(std::size_t size);

}  // namespace sdr_analyzer::dsp

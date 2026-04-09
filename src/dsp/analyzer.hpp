#pragma once

#include <complex>
#include <vector>

#include "sdr_analyzer/config.hpp"
#include "sdr_analyzer/results.hpp"

namespace sdr_analyzer::dsp {

class Analyzer {
 public:
  explicit Analyzer(ProcessingConfig config = {});

  void UpdateConfig(const ProcessingConfig& config);
  AnalyzerSnapshot Process(
      std::uint64_t sequence,
      double center_frequency_hz,
      double sample_rate_hz,
      const std::vector<std::complex<float>>& iq_samples,
      const std::vector<Marker>& markers);

 private:
  ProcessingConfig config_;
  std::vector<float> window_;
  std::vector<double> averaged_power_;
  std::vector<double> peak_hold_power_;

  void EnsureState();
};

}  // namespace sdr_analyzer::dsp

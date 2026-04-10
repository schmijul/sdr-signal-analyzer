#pragma once

#include <cstdint>
#include <random>

#include "src/sdr/source.hpp"

namespace sdr_analyzer::sdr {

class SimulatorSource final : public ISampleSource {
public:
  SimulatorSource();

  bool Configure(const SourceConfig &config, std::string &error) override;
  bool Start(std::string &error) override;
  void Stop() override;
  std::size_t ReadSamples(std::vector<std::complex<float>> &output,
                          std::size_t max_samples, std::string &error) override;
  std::string Description() const override;

private:
  SourceConfig config_;
  bool running_ = false;
  double phase_a_ = 0.0;
  double phase_b_ = 0.0;
  double burst_phase_ = 0.0;
  std::uint64_t sample_cursor_ = 0;
  std::mt19937 rng_;
  std::normal_distribution<float> noise_{0.0f, 0.03f};
};

} // namespace sdr_analyzer::sdr

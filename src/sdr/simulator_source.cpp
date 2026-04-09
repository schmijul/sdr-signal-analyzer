#include "src/sdr/simulator_source.hpp"

#include <cmath>

namespace sdr_analyzer::sdr {
namespace {

constexpr double kTwoPi = 6.28318530717958647692;

}  // namespace

SimulatorSource::SimulatorSource() : rng_(0x534452u) {}

bool SimulatorSource::Configure(const SourceConfig& config, std::string& error) {
  if (config.sample_rate_hz <= 0.0) {
    error = "Sample rate must be positive.";
    return false;
  }
  if (config.frame_samples == 0) {
    error = "Frame size must be positive.";
    return false;
  }
  config_ = config;
  return true;
}

bool SimulatorSource::Start(std::string& /*error*/) {
  running_ = true;
  return true;
}

void SimulatorSource::Stop() { running_ = false; }

std::size_t SimulatorSource::ReadSamples(
    std::vector<std::complex<float>>& output,
    const std::size_t max_samples,
    std::string& error) {
  if (!running_) {
    error = "Simulator source is not running.";
    return 0;
  }

  output.resize(max_samples);
  const double delta_a = kTwoPi * 120000.0 / config_.sample_rate_hz;
  const double delta_b = kTwoPi * -380000.0 / config_.sample_rate_hz;
  const double delta_burst = kTwoPi * 450000.0 / config_.sample_rate_hz;

  for (std::size_t index = 0; index < max_samples; ++index) {
    const bool burst_on = ((sample_cursor_ / 4096U) % 10U) < 2U;
    const float fm_like =
        0.45f * std::sin(static_cast<float>(phase_a_ + 1.8 * std::sin(sample_cursor_ * 0.0009)));
    const std::complex<float> tone_b(
        0.22f * std::cos(static_cast<float>(phase_b_)),
        0.22f * std::sin(static_cast<float>(phase_b_)));
    const std::complex<float> burst(
        burst_on ? 0.38f * std::cos(static_cast<float>(burst_phase_)) : 0.0f,
        burst_on ? 0.38f * std::sin(static_cast<float>(burst_phase_)) : 0.0f);
    output[index] =
        std::complex<float>(fm_like, 0.32f * std::cos(static_cast<float>(phase_a_ * 0.6))) + tone_b + burst +
        std::complex<float>(noise_(rng_), noise_(rng_));

    phase_a_ = std::fmod(phase_a_ + delta_a, kTwoPi);
    phase_b_ = std::fmod(phase_b_ + delta_b, kTwoPi);
    burst_phase_ = std::fmod(burst_phase_ + delta_burst, kTwoPi);
    ++sample_cursor_;
  }

  return output.size();
}

std::string SimulatorSource::Description() const { return "Simulator source"; }

}  // namespace sdr_analyzer::sdr

#include <cmath>
#include <complex>
#include <iostream>
#include <stdexcept>
#include <vector>

#include "src/dsp/analyzer.hpp"

namespace {

using sdr_analyzer::Marker;
using sdr_analyzer::ProcessingConfig;
using sdr_analyzer::dsp::Analyzer;

void Require(const bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::vector<std::complex<float>> GenerateTone(
    const std::size_t sample_count,
    const double sample_rate_hz,
    const double tone_offset_hz,
    const float amplitude) {
  constexpr double kTwoPi = 6.28318530717958647692;
  std::vector<std::complex<float>> samples(sample_count);
  double phase = 0.0;
  const double delta = kTwoPi * tone_offset_hz / sample_rate_hz;
  for (std::size_t index = 0; index < sample_count; ++index) {
    samples[index] = std::complex<float>(
        amplitude * std::cos(static_cast<float>(phase)),
        amplitude * std::sin(static_cast<float>(phase)));
    phase += delta;
  }
  return samples;
}

std::vector<std::complex<float>> GenerateBurstTone(
    const std::size_t sample_count,
    const double sample_rate_hz,
    const double tone_offset_hz) {
  auto samples = GenerateTone(sample_count, sample_rate_hz, tone_offset_hz, 0.7f);
  for (std::size_t index = 0; index < sample_count; ++index) {
    const bool enabled = (index / 256U) % 4U == 0U;
    if (!enabled) {
      samples[index] = {};
    }
  }
  return samples;
}

void TestPeakDetection() {
  ProcessingConfig config;
  config.fft_size = 2048;
  Analyzer analyzer(config);

  const auto samples = GenerateTone(2048, 2.4e6, 200000.0, 0.8f);
  const auto snapshot = analyzer.Process(1, 100e6, 2.4e6, samples, {});

  Require(!snapshot.analysis.detections.empty(), "Expected at least one detection.");

  bool matched_frequency = false;
  for (const auto& detection : snapshot.analysis.detections) {
    if (std::abs(detection.center_frequency_hz - 100200000.0) < 4000.0) {
      matched_frequency = true;
      Require(detection.bandwidth_hz > 0.0, "Bandwidth estimate should be positive.");
      break;
    }
  }
  Require(matched_frequency, "Tone center frequency estimate is off.");
}

void TestBurstClassification() {
  ProcessingConfig config;
  config.fft_size = 1024;
  Analyzer analyzer(config);

  const auto samples = GenerateBurstTone(1024, 2.4e6, -250000.0);
  const auto snapshot = analyzer.Process(2, 433.92e6, 2.4e6, samples, {});

  Require(!snapshot.analysis.detections.empty(), "Burst test should detect the active tone.");

  bool saw_burst_like = false;
  for (const auto& detection : snapshot.analysis.detections) {
    for (const auto& label : detection.labels) {
      if (label == "burst-like") {
        saw_burst_like = true;
        break;
      }
    }
    if (saw_burst_like) {
      break;
    }
  }
  Require(saw_burst_like, "Expected burst-like classification for gated signal.");
}

}  // namespace

int main() {
  try {
    TestPeakDetection();
    TestBurstClassification();
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
  return 0;
}

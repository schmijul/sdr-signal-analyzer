#include <algorithm>
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

void Require(const bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::vector<std::complex<float>> GenerateTone(const std::size_t sample_count,
                                              const double sample_rate_hz,
                                              const double tone_offset_hz,
                                              const float amplitude) {
  constexpr double kTwoPi = 6.28318530717958647692;
  std::vector<std::complex<float>> samples(sample_count);
  double phase = 0.0;
  const double delta = kTwoPi * tone_offset_hz / sample_rate_hz;
  for (std::size_t index = 0; index < sample_count; ++index) {
    samples[index] =
        std::complex<float>(amplitude * std::cos(static_cast<float>(phase)),
                            amplitude * std::sin(static_cast<float>(phase)));
    phase += delta;
  }
  return samples;
}

std::vector<std::complex<float>>
GenerateBurstTone(const std::size_t sample_count, const double sample_rate_hz,
                  const double tone_offset_hz) {
  auto samples =
      GenerateTone(sample_count, sample_rate_hz, tone_offset_hz, 0.7f);
  for (std::size_t index = 0; index < sample_count; ++index) {
    const bool enabled = (index / 256U) % 4U == 0U;
    if (!enabled) {
      samples[index] = {};
    }
  }
  return samples;
}

std::vector<std::complex<float>> GenerateSilence(const std::size_t sample_count) {
  return std::vector<std::complex<float>>(sample_count);
}

const sdr_analyzer::DetectionResult &
StrongestDetection(const sdr_analyzer::AnalyzerSnapshot &snapshot) {
  Require(!snapshot.analysis.detections.empty(), "Expected a detection.");
  return snapshot.analysis.detections.front();
}

void TestPeakDetection() {
  ProcessingConfig config;
  config.fft_size = 2048;
  Analyzer analyzer(config);

  const auto samples = GenerateTone(2048, 2.4e6, 200000.0, 0.8f);
  const auto snapshot = analyzer.Process(1, 100e6, 2.4e6, samples, {});

  Require(!snapshot.analysis.detections.empty(),
          "Expected at least one detection.");

  bool matched_frequency = false;
  for (const auto &detection : snapshot.analysis.detections) {
    if (std::abs(detection.center_frequency_hz - 100200000.0) < 4000.0) {
      matched_frequency = true;
      Require(detection.bandwidth_hz > 0.0,
              "Bandwidth estimate should be positive.");
      break;
    }
  }
  Require(matched_frequency, "Tone center frequency estimate is off.");
}

void TestFrequencyAccuracyAcrossOffsets() {
  ProcessingConfig config;
  config.fft_size = 4096;
  Analyzer lower_offset_analyzer(config);
  Analyzer upper_offset_analyzer(config);

  const auto lower_offset = lower_offset_analyzer.Process(
      10, 144e6, 2.4e6, GenerateTone(4096, 2.4e6, 125000.0, 0.9f), {});
  const auto upper_offset = upper_offset_analyzer.Process(
      11, 144e6, 2.4e6, GenerateTone(4096, 2.4e6, 325000.0, 0.9f), {});

  Require(std::abs(StrongestDetection(lower_offset).center_frequency_hz - 144125000.0) <
              1500.0,
          "Lower-offset tone frequency estimate drifted too far.");
  Require(std::abs(StrongestDetection(upper_offset).center_frequency_hz - 144325000.0) <
              1500.0,
          "Upper-offset tone frequency estimate drifted too far.");
}

void TestToneBandwidthAndNoiseMargin() {
  ProcessingConfig config;
  config.fft_size = 2048;
  Analyzer analyzer(config);

  const auto snapshot = analyzer.Process(
      12, 100e6, 2.4e6, GenerateTone(2048, 2.4e6, 150000.0, 0.85f), {});
  const auto &detection = StrongestDetection(snapshot);
  const double bin_resolution = snapshot.spectrum.bin_resolution_hz;

  Require(detection.bandwidth_hz >= bin_resolution,
          "Narrowband tone bandwidth must cover at least one FFT bin.");
  Require(detection.bandwidth_hz <= bin_resolution * 8.0,
          "Pure tone bandwidth estimate should remain narrow.");
  Require(snapshot.analysis.strongest_peak_dbfs - snapshot.analysis.noise_floor_dbfs >
              25.0,
          "Tone should stand clearly above the estimated noise floor.");
}

void TestBurstClassification() {
  ProcessingConfig config;
  config.fft_size = 1024;
  Analyzer analyzer(config);

  const auto samples = GenerateBurstTone(1024, 2.4e6, -250000.0);
  const auto snapshot = analyzer.Process(2, 433.92e6, 2.4e6, samples, {});

  Require(!snapshot.analysis.detections.empty(),
          "Burst test should detect the active tone.");

  bool saw_burst_like = false;
  for (const auto &detection : snapshot.analysis.detections) {
    for (const auto &label : detection.labels) {
      if (label == "burst-like") {
        saw_burst_like = true;
        break;
      }
    }
    if (saw_burst_like) {
      break;
    }
  }
  Require(saw_burst_like,
          "Expected burst-like classification for gated signal.");
}

void TestMarkerMeasurement() {
  ProcessingConfig config;
  config.fft_size = 2048;
  Analyzer analyzer(config);

  const auto samples = GenerateTone(2048, 2.4e6, 200000.0, 0.8f);
  Marker marker;
  marker.center_frequency_hz = 100200000.0;
  marker.bandwidth_hz = 100000.0;

  const auto snapshot = analyzer.Process(3, 100e6, 2.4e6, samples, {marker});
  Require(snapshot.analysis.marker_measurements.size() == 1,
          "Expected one marker measurement.");

  const auto &measurement = snapshot.analysis.marker_measurements.front();
  Require(measurement.center_frequency_hz == marker.center_frequency_hz,
          "Marker center frequency should round-trip.");
  Require(measurement.bandwidth_hz == marker.bandwidth_hz,
          "Marker bandwidth should round-trip.");
  Require(measurement.peak_power_dbfs > measurement.average_power_dbfs,
          "Peak power should exceed average power.");
  Require(measurement.peak_power_dbfs > -30.0,
          "Expected a strong in-band marker reading.");
}

void TestPeakHoldPreservesRecentPeak() {
  ProcessingConfig config;
  config.fft_size = 2048;
  config.peak_hold_enabled = true;
  Analyzer analyzer(config);

  const auto active = analyzer.Process(
      20, 100e6, 2.4e6, GenerateTone(2048, 2.4e6, 250000.0, 0.9f), {});
  const auto quiet =
      analyzer.Process(21, 100e6, 2.4e6, GenerateSilence(2048), {});

  const double peak_hold_max =
      *std::max_element(quiet.spectrum.peak_hold_dbfs.begin(),
                        quiet.spectrum.peak_hold_dbfs.end());
  const double live_power_max =
      *std::max_element(quiet.spectrum.power_dbfs.begin(),
                        quiet.spectrum.power_dbfs.end());

  Require(peak_hold_max >= active.analysis.strongest_peak_dbfs - 0.5,
          "Peak hold should preserve the recent strong tone.");
  Require(live_power_max < -100.0,
          "Silence frame should not retain a strong live-spectrum peak.");
}

} // namespace

int main() {
  try {
    TestPeakDetection();
    TestFrequencyAccuracyAcrossOffsets();
    TestToneBandwidthAndNoiseMargin();
    TestBurstClassification();
    TestMarkerMeasurement();
    TestPeakHoldPreservesRecentPeak();
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
  return 0;
}

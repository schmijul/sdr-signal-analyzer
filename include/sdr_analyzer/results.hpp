#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace sdr_analyzer {

struct Marker {
  double center_frequency_hz = 0.0;
  double bandwidth_hz = 0.0;
};

struct DetectionResult {
  double center_frequency_hz = 0.0;
  double offset_hz = 0.0;
  double peak_power_dbfs = 0.0;
  double bandwidth_hz = 0.0;
  std::vector<std::string> labels;
};

struct MarkerMeasurement {
  double center_frequency_hz = 0.0;
  double bandwidth_hz = 0.0;
  double peak_power_dbfs = 0.0;
  double average_power_dbfs = 0.0;
};

struct AnalysisReport {
  double noise_floor_dbfs = 0.0;
  double strongest_peak_dbfs = 0.0;
  double burst_score = 0.0;
  std::vector<DetectionResult> detections;
  std::vector<MarkerMeasurement> marker_measurements;
};

struct SpectrumFrame {
  double center_frequency_hz = 0.0;
  double bin_resolution_hz = 0.0;
  std::vector<double> power_dbfs;
  std::vector<double> average_dbfs;
  std::vector<double> peak_hold_dbfs;
};

struct TimeDomainFrame {
  std::vector<float> i;
  std::vector<float> q;
  std::vector<float> magnitude;
};

struct AnalyzerSnapshot {
  std::uint64_t sequence = 0;
  SpectrumFrame spectrum;
  TimeDomainFrame time_domain;
  AnalysisReport analysis;
};

struct RecordingStatus {
  bool active = false;
  std::string base_path;
  std::uint64_t samples_written = 0;
};

struct SessionStatus {
  bool running = false;
  std::string source_description;
  std::optional<RecordingStatus> recording;
};

} // namespace sdr_analyzer

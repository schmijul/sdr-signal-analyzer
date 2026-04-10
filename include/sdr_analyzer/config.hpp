#pragma once

#include <cstddef>
#include <string>

namespace sdr_analyzer {

enum class SourceKind {
  kSimulator,
  kReplay,
  kRtlTcp,
  kUhd,
  kSoapy,
};

enum class SampleFormat {
  kComplexFloat32,
  kComplexInt16,
};

enum class RecordingFormat {
  kRawBin,
  kSigMF,
};

struct SourceConfig {
  SourceKind kind = SourceKind::kSimulator;
  std::string device_string;
  std::string device_args;
  std::string input_path;
  std::string metadata_path;
  std::string network_host = "127.0.0.1";
  int network_port = 1234;
  std::size_t channel = 0;
  std::string antenna;
  double bandwidth_hz = 0.0;
  std::string clock_source;
  std::string time_source;
  double center_frequency_hz = 100e6;
  double sample_rate_hz = 2.4e6;
  double gain_db = 10.0;
  std::size_t frame_samples = 4096;
  bool loop_playback = false;
  SampleFormat sample_format = SampleFormat::kComplexFloat32;
};

struct ProcessingConfig {
  std::size_t fft_size = 2048;
  std::size_t display_samples = 2048;
  double averaging_factor = 4.0;
  bool peak_hold_enabled = true;
  double detection_threshold_db = 8.0;
  std::size_t max_peaks = 16;
  std::size_t minimum_peak_spacing_bins = 8;
  double bandwidth_threshold_db = 6.0;
};

struct RecordingConfig {
  RecordingFormat format = RecordingFormat::kRawBin;
  std::string base_path;
};

struct ReplayConfig {
  std::string data_path;
  std::string metadata_path;
  bool loop = false;
};

} // namespace sdr_analyzer

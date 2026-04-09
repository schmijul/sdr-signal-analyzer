#include "src/io/recorder.hpp"

#include <filesystem>

#include "src/io/metadata.hpp"

namespace sdr_analyzer::io {
namespace {

std::string DataPath(const RecordingConfig& config) {
  return config.format == RecordingFormat::kSigMF ? config.base_path + ".sigmf-data" : config.base_path + ".bin";
}

std::string MetadataPath(const RecordingConfig& config) {
  return config.format == RecordingFormat::kSigMF ? config.base_path + ".sigmf-meta" : config.base_path + ".bin.json";
}

}  // namespace

bool Recorder::Start(const RecordingConfig& config, const SourceConfig& source_config, std::string& error) {
  Stop();
  if (config.base_path.empty()) {
    error = "Recording base path must not be empty.";
    return false;
  }

  config_ = config;
  source_config_ = source_config;
  stream_.open(DataPath(config_), std::ios::binary);
  if (!stream_) {
    error = "Failed to open recording file for writing.";
    return false;
  }

  bool metadata_ok = false;
  if (config_.format == RecordingFormat::kSigMF) {
    metadata_ok = WriteSigmfMetadata(MetadataPath(config_), source_config_, error);
  } else {
    metadata_ok = WriteRawMetadata(MetadataPath(config_), source_config_, error);
  }
  if (!metadata_ok) {
    Stop();
    return false;
  }

  samples_written_ = 0;
  active_ = true;
  return true;
}

void Recorder::Stop() {
  active_ = false;
  if (stream_.is_open()) {
    stream_.close();
  }
}

bool Recorder::Write(const std::vector<std::complex<float>>& samples, std::string& error) {
  if (!active_) {
    return true;
  }
  if (!stream_) {
    error = "Recording stream is not writable.";
    active_ = false;
    return false;
  }
  stream_.write(reinterpret_cast<const char*>(samples.data()), static_cast<std::streamsize>(samples.size() * sizeof(std::complex<float>)));
  if (!stream_) {
    error = "Failed to write recording samples.";
    active_ = false;
    return false;
  }
  samples_written_ += samples.size();
  return true;
}

RecordingStatus Recorder::status() const {
  RecordingStatus status;
  status.active = active_;
  status.base_path = config_.base_path;
  status.samples_written = samples_written_;
  return status;
}

}  // namespace sdr_analyzer::io

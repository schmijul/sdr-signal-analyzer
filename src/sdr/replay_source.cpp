#include "src/sdr/replay_source.hpp"

#include <array>
#include <filesystem>

#include "src/io/metadata.hpp"

namespace sdr_analyzer::sdr {
namespace {

std::string DefaultMetadataPath(const SourceConfig& config) {
  const std::filesystem::path input(config.input_path);
  if (!config.metadata_path.empty()) {
    return config.metadata_path;
  }
  if (input.extension() == ".bin") {
    return input.string() + ".json";
  }
  if (input.extension() == ".sigmf-data") {
    auto meta = input;
    meta.replace_extension(".sigmf-meta");
    return meta.string();
  }
  return {};
}

}  // namespace

ReplaySource::ReplaySource() = default;

bool ReplaySource::Configure(const SourceConfig& config, std::string& error) {
  if (config.input_path.empty()) {
    error = "Replay source requires an input file.";
    return false;
  }
  config_ = config;

  SourceConfig metadata_config = config_;
  const std::string metadata_path = DefaultMetadataPath(config_);
  if (!metadata_path.empty() && std::filesystem::exists(metadata_path)) {
    bool parsed = false;
    if (metadata_path.ends_with(".sigmf-meta")) {
      parsed = io::ReadSigmfMetadata(metadata_path, metadata_config, error);
    } else {
      parsed = io::ReadRawMetadata(metadata_path, metadata_config, error);
    }
    if (parsed) {
      metadata_config.input_path = config_.input_path;
      metadata_config.metadata_path = metadata_path;
      metadata_config.loop_playback = config_.loop_playback;
      config_ = metadata_config;
    }
  }
  return true;
}

bool ReplaySource::Start(std::string& error) {
  stream_.open(config_.input_path, std::ios::binary);
  if (!stream_) {
    error = "Failed to open replay file: " + config_.input_path;
    return false;
  }
  return true;
}

void ReplaySource::Stop() {
  if (stream_.is_open()) {
    stream_.close();
  }
}

std::size_t ReplaySource::ReadSamples(
    std::vector<std::complex<float>>& output,
    const std::size_t max_samples,
    std::string& error) {
  if (!stream_.is_open()) {
    error = "Replay file is not open.";
    return 0;
  }
  if (config_.sample_format == SampleFormat::kComplexInt16) {
    return ReadComplexInt16(output, max_samples, error);
  }
  return ReadComplexFloat32(output, max_samples, error);
}

std::string ReplaySource::Description() const {
  return "Replay source: " + config_.input_path;
}

bool ReplaySource::Rewind() {
  stream_.clear();
  stream_.seekg(0, std::ios::beg);
  return static_cast<bool>(stream_);
}

std::size_t ReplaySource::ReadComplexFloat32(
    std::vector<std::complex<float>>& output,
    const std::size_t max_samples,
    std::string& error) {
  std::vector<float> raw(max_samples * 2U, 0.0f);
  stream_.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size() * sizeof(float)));
  const auto values_read = static_cast<std::size_t>(stream_.gcount()) / sizeof(float);
  if (values_read == 0 && config_.loop_playback && Rewind()) {
    return ReadComplexFloat32(output, max_samples, error);
  }
  if (values_read < 2U) {
    error = "Replay file reached end of stream.";
    return 0;
  }
  const std::size_t sample_count = values_read / 2U;
  output.resize(sample_count);
  for (std::size_t index = 0; index < sample_count; ++index) {
    output[index] = std::complex<float>(raw[index * 2U], raw[index * 2U + 1U]);
  }
  return output.size();
}

std::size_t ReplaySource::ReadComplexInt16(
    std::vector<std::complex<float>>& output,
    const std::size_t max_samples,
    std::string& error) {
  std::vector<std::int16_t> raw(max_samples * 2U, 0);
  stream_.read(reinterpret_cast<char*>(raw.data()), static_cast<std::streamsize>(raw.size() * sizeof(std::int16_t)));
  const auto values_read = static_cast<std::size_t>(stream_.gcount()) / sizeof(std::int16_t);
  if (values_read == 0 && config_.loop_playback && Rewind()) {
    return ReadComplexInt16(output, max_samples, error);
  }
  if (values_read < 2U) {
    error = "Replay file reached end of stream.";
    return 0;
  }
  const std::size_t sample_count = values_read / 2U;
  output.resize(sample_count);
  for (std::size_t index = 0; index < sample_count; ++index) {
    output[index] = std::complex<float>(
        static_cast<float>(raw[index * 2U]) / 32768.0f,
        static_cast<float>(raw[index * 2U + 1U]) / 32768.0f);
  }
  return output.size();
}

}  // namespace sdr_analyzer::sdr

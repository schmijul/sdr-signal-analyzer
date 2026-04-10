#include "src/io/metadata.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <optional>
#include <regex>
#include <sstream>

namespace sdr_analyzer::io {
namespace {

std::optional<double> ExtractNumber(const std::string &content,
                                    const std::string &key) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*([-0-9.eE+]+)");
  std::smatch match;
  if (!std::regex_search(content, match, pattern)) {
    return std::nullopt;
  }
  return std::stod(match[1].str());
}

std::optional<std::string> ExtractString(const std::string &content,
                                         const std::string &key) {
  const std::regex pattern("\"" + key + "\"\\s*:\\s*\"([^\"]+)\"");
  std::smatch match;
  if (!std::regex_search(content, match, pattern)) {
    return std::nullopt;
  }
  return match[1].str();
}

bool ReadFile(const std::string &path, std::string &content,
              std::string &error) {
  std::ifstream file(path);
  if (!file) {
    error = "Failed to open metadata file: " + path;
    return false;
  }
  std::ostringstream stream;
  stream << file.rdbuf();
  content = stream.str();
  return true;
}

bool WriteText(const std::string &path, const std::string &content,
               std::string &error) {
  std::ofstream file(path);
  if (!file) {
    error = "Failed to write file: " + path;
    return false;
  }
  file << content;
  return static_cast<bool>(file);
}

} // namespace

std::string SampleFormatToString(const SampleFormat format) {
  switch (format) {
  case SampleFormat::kComplexFloat32:
    return "cf32_le";
  case SampleFormat::kComplexInt16:
    return "ci16_le";
  }
  return "cf32_le";
}

std::optional<SampleFormat> SampleFormatFromString(const std::string &text) {
  if (text == "cf32_le") {
    return SampleFormat::kComplexFloat32;
  }
  if (text == "ci16_le") {
    return SampleFormat::kComplexInt16;
  }
  return std::nullopt;
}

std::string CurrentUtcTimestamp() {
  const auto now = std::chrono::system_clock::now();
  const std::time_t as_time_t = std::chrono::system_clock::to_time_t(now);
  std::tm utc_tm{};
#if defined(_WIN32)
  gmtime_s(&utc_tm, &as_time_t);
#else
  gmtime_r(&as_time_t, &utc_tm);
#endif
  std::ostringstream stream;
  stream << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

bool WriteRawMetadata(const std::string &path,
                      const SourceConfig &source_config, std::string &error) {
  std::ostringstream content;
  content << "{\n"
          << "  \"sample_rate_hz\": " << source_config.sample_rate_hz << ",\n"
          << "  \"center_frequency_hz\": " << source_config.center_frequency_hz
          << ",\n"
          << "  \"gain_db\": " << source_config.gain_db << ",\n"
          << "  \"frame_samples\": " << source_config.frame_samples << ",\n"
          << "  \"sample_format\": \""
          << SampleFormatToString(source_config.sample_format) << "\",\n"
          << "  \"capture_start_utc\": \"" << CurrentUtcTimestamp() << "\"\n"
          << "}\n";
  return WriteText(path, content.str(), error);
}

bool ReadRawMetadata(const std::string &path, SourceConfig &source_config,
                     std::string &error) {
  std::string content;
  if (!ReadFile(path, content, error)) {
    return false;
  }
  if (const auto value = ExtractNumber(content, "sample_rate_hz")) {
    source_config.sample_rate_hz = *value;
  }
  if (const auto value = ExtractNumber(content, "center_frequency_hz")) {
    source_config.center_frequency_hz = *value;
  }
  if (const auto value = ExtractNumber(content, "gain_db")) {
    source_config.gain_db = *value;
  }
  if (const auto value = ExtractNumber(content, "frame_samples")) {
    source_config.frame_samples = static_cast<std::size_t>(*value);
  }
  if (const auto value = ExtractString(content, "sample_format")) {
    const auto format = SampleFormatFromString(*value);
    if (format) {
      source_config.sample_format = *format;
    }
  }
  return true;
}

bool WriteSigmfMetadata(const std::string &path,
                        const SourceConfig &source_config, std::string &error) {
  std::ostringstream content;
  content << "{\n"
          << "  \"global\": {\n"
          << "    \"core:datatype\": \""
          << SampleFormatToString(source_config.sample_format) << "\",\n"
          << "    \"core:sample_rate\": " << source_config.sample_rate_hz
          << "\n"
          << "  },\n"
          << "  \"captures\": [\n"
          << "    {\n"
          << "      \"core:sample_start\": 0,\n"
          << "      \"core:frequency\": " << source_config.center_frequency_hz
          << ",\n"
          << "      \"core:datetime\": \"" << CurrentUtcTimestamp() << "\",\n"
          << "      \"sdr_analyzer:gain_db\": " << source_config.gain_db << "\n"
          << "    }\n"
          << "  ],\n"
          << "  \"annotations\": []\n"
          << "}\n";
  return WriteText(path, content.str(), error);
}

bool ReadSigmfMetadata(const std::string &path, SourceConfig &source_config,
                       std::string &error) {
  std::string content;
  if (!ReadFile(path, content, error)) {
    return false;
  }
  if (const auto value = ExtractNumber(content, "core:sample_rate")) {
    source_config.sample_rate_hz = *value;
  }
  if (const auto value = ExtractNumber(content, "core:frequency")) {
    source_config.center_frequency_hz = *value;
  }
  if (const auto value = ExtractNumber(content, "sdr_analyzer:gain_db")) {
    source_config.gain_db = *value;
  }
  if (const auto value = ExtractString(content, "core:datatype")) {
    const auto format = SampleFormatFromString(*value);
    if (format) {
      source_config.sample_format = *format;
    }
  }
  return true;
}

} // namespace sdr_analyzer::io

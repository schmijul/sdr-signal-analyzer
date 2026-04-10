#pragma once

#include <optional>
#include <string>

#include "sdr_analyzer/config.hpp"

namespace sdr_analyzer::io {

std::string SampleFormatToString(SampleFormat format);
std::optional<SampleFormat> SampleFormatFromString(const std::string &text);

bool WriteRawMetadata(const std::string &path,
                      const SourceConfig &source_config, std::string &error);
bool ReadRawMetadata(const std::string &path, SourceConfig &source_config,
                     std::string &error);

bool WriteSigmfMetadata(const std::string &path,
                        const SourceConfig &source_config, std::string &error);
bool ReadSigmfMetadata(const std::string &path, SourceConfig &source_config,
                       std::string &error);

std::string CurrentUtcTimestamp();

} // namespace sdr_analyzer::io

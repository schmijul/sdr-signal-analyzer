#pragma once

#include <complex>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#include "sdr_analyzer/config.hpp"
#include "sdr_analyzer/results.hpp"

namespace sdr_analyzer::io {

class Recorder {
public:
  bool Start(const RecordingConfig &config, const SourceConfig &source_config,
             std::string &error);
  void Stop();
  bool Write(const std::vector<std::complex<float>> &samples,
             std::string &error);

  [[nodiscard]] bool active() const { return active_; }
  [[nodiscard]] RecordingStatus status() const;

private:
  bool active_ = false;
  RecordingConfig config_;
  SourceConfig source_config_;
  std::ofstream stream_;
  std::uint64_t samples_written_ = 0;
};

} // namespace sdr_analyzer::io

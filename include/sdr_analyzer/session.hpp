#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "sdr_analyzer/config.hpp"
#include "sdr_analyzer/results.hpp"

namespace sdr_analyzer {

class AnalyzerSession {
public:
  AnalyzerSession(SourceConfig source_config = {},
                  ProcessingConfig processing_config = {});
  ~AnalyzerSession();

  AnalyzerSession(const AnalyzerSession &) = delete;
  AnalyzerSession &operator=(const AnalyzerSession &) = delete;

  bool start();
  void stop();

  bool is_running() const;
  SessionStatus status() const;

  bool update_source_config(const SourceConfig &config);
  void update_processing_config(const ProcessingConfig &config);
  bool start_recording(const RecordingConfig &config);
  void stop_recording();

  std::optional<AnalyzerSnapshot> poll_snapshot();
  void set_markers(const std::vector<Marker> &markers);

  SourceConfig source_config() const;
  ProcessingConfig processing_config() const;
  std::vector<DiagnosticEntry> diagnostics() const;
  std::vector<DiagnosticEntry> drain_diagnostics();
  std::string last_error() const;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

} // namespace sdr_analyzer

#include "sdr_analyzer/session.hpp"

#include <atomic>
#include <chrono>
#include <ctime>
#include <deque>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <thread>

#include "src/dsp/analyzer.hpp"
#include "src/io/recorder.hpp"
#include "src/sdr/source_factory.hpp"

namespace sdr_analyzer {
namespace {

constexpr std::size_t kMaxSnapshots = 8;
constexpr std::size_t kMaxDiagnostics = 128;
std::atomic<std::uint64_t> g_next_session_id{1};

std::string SourceKindToString(const SourceKind kind) {
  switch (kind) {
  case SourceKind::kSimulator:
    return "simulator";
  case SourceKind::kReplay:
    return "replay";
  case SourceKind::kRtlTcp:
    return "rtl_tcp";
  case SourceKind::kUhd:
    return "uhd";
  case SourceKind::kSoapy:
    return "soapy";
  }
  return "unknown";
}

std::string FormatUtcTimestamp(
    const std::chrono::system_clock::time_point timestamp) {
  const auto seconds =
      std::chrono::time_point_cast<std::chrono::seconds>(timestamp);
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(timestamp - seconds)
          .count();
  const std::time_t unix_seconds = std::chrono::system_clock::to_time_t(seconds);
  const std::tm utc_tm = *std::gmtime(&unix_seconds);
  std::ostringstream formatted;
  formatted << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%S") << '.'
            << std::setw(3) << std::setfill('0') << milliseconds << 'Z';
  return formatted.str();
}

std::string BuildContextError(const std::string &stage,
                              const SourceConfig &config,
                              const std::string &error) {
  return stage + " for source '" + SourceKindToString(config.kind) +
         "' failed: " + error;
}

} // namespace

class AnalyzerSession::Impl {
public:
  Impl(SourceConfig source_config, ProcessingConfig processing_config)
      : source_config_(std::move(source_config)), analyzer_(processing_config),
        processing_config_(processing_config),
        session_id_(g_next_session_id.fetch_add(1)) {}

  ~Impl() { Stop(); }

  bool Start() {
    {
      std::scoped_lock lock(mutex_);
      if (running_) {
        return true;
      }
    }

    if (worker_.joinable()) {
      worker_.join();
    }

    std::scoped_lock lock(mutex_);
    if (running_) {
      return true;
    }

    snapshots_.clear();
    last_error_.clear();
    AppendDiagnosticLocked("info", "session", "session.start_requested",
                           "Starting analyzer session.");

    std::string error;
    auto source = sdr::CreateSource(source_config_, error);
    if (!source) {
      last_error_ = BuildContextError("Source creation", source_config_, error);
      AppendDiagnosticLocked("error", "source_factory", "source.create_failed",
                             last_error_);
      return false;
    }
    if (!source->Configure(source_config_, error)) {
      last_error_ =
          BuildContextError("Source configuration", source_config_, error);
      AppendDiagnosticLocked("error", "source", "source.configure_failed",
                             last_error_, source->Description());
      return false;
    }
    if (!source->Start(error)) {
      last_error_ = BuildContextError("Source start", source_config_, error);
      AppendDiagnosticLocked("error", "source", "source.start_failed",
                             last_error_, source->Description());
      return false;
    }

    source_ = std::move(source);
    running_ = true;
    source_config_dirty_ = false;
    AppendDiagnosticLocked("info", "session", "session.started",
                           "Analyzer session started successfully.",
                           source_->Description());
    worker_ = std::thread([this]() { Run(); });
    return true;
  }

  void Stop() {
    bool should_join = false;
    {
      std::scoped_lock lock(mutex_);
      if (running_) {
        AppendDiagnosticLocked("info", "session", "session.stop_requested",
                               "Stopping analyzer session.");
      }
      running_ = false;
      should_join = worker_.joinable();
    }

    if (should_join) {
      worker_.join();
    }

    std::unique_ptr<sdr::ISampleSource> source_to_stop;
    {
      std::scoped_lock lock(mutex_);
      source_to_stop = std::move(source_);
      source_config_dirty_ = false;
    }
    if (source_to_stop) {
      source_to_stop->Stop();
    }

    std::scoped_lock recorder_lock(recorder_mutex_);
    recorder_.Stop();
  }

  bool IsRunning() const {
    std::scoped_lock lock(mutex_);
    return running_;
  }

  SessionStatus Status() const {
    SessionStatus status;
    {
      std::scoped_lock lock(mutex_);
      status.running = running_;
      status.source_description =
          running_ && source_ ? source_->Description() : "stopped";
    }
    {
      std::scoped_lock recorder_lock(recorder_mutex_);
      if (recorder_.active()) {
        status.recording = recorder_.status();
      }
    }
    return status;
  }

  bool UpdateSourceConfig(const SourceConfig &config) {
    std::scoped_lock lock(mutex_);
    source_config_ = config;
    source_config_dirty_ = source_ != nullptr;
    AppendDiagnosticLocked(
        "info", "session", "source_config.updated",
        source_config_dirty_
            ? "Source configuration updated and scheduled for live reconfigure."
            : "Source configuration updated.");
    return true;
  }

  void UpdateProcessingConfig(const ProcessingConfig &config) {
    {
      std::scoped_lock lock(mutex_);
      processing_config_ = config;
      AppendDiagnosticLocked("info", "session", "processing_config.updated",
                             "Processing configuration updated.");
    }
    std::scoped_lock analysis_lock(analysis_mutex_);
    analyzer_.UpdateConfig(config);
  }

  void ResetPeakHold() {
    std::scoped_lock analysis_lock(analysis_mutex_);
    analyzer_.ResetPeakHold();
  }

  bool StartRecording(const RecordingConfig &config) {
    SourceConfig source_config_copy;
    {
      std::scoped_lock lock(mutex_);
      source_config_copy = source_config_;
    }

    std::scoped_lock recorder_lock(recorder_mutex_);
    std::string error;
    if (!recorder_.Start(config, source_config_copy, error)) {
      std::scoped_lock lock(mutex_);
      last_error_ = error;
      AppendDiagnosticLocked("error", "recorder", "recording.start_failed",
                             error);
      return false;
    }
    {
      std::scoped_lock lock(mutex_);
      AppendDiagnosticLocked("info", "recorder", "recording.started",
                             "Recording started.");
    }
    return true;
  }

  void StopRecording() {
    std::scoped_lock recorder_lock(recorder_mutex_);
    recorder_.Stop();
    std::scoped_lock lock(mutex_);
    AppendDiagnosticLocked("info", "recorder", "recording.stopped",
                           "Recording stopped.");
  }

  std::optional<AnalyzerSnapshot> PollSnapshot() {
    std::scoped_lock lock(mutex_);
    if (snapshots_.empty()) {
      return std::nullopt;
    }
    AnalyzerSnapshot snapshot = std::move(snapshots_.front());
    snapshots_.pop_front();
    return snapshot;
  }

  void SetMarkers(const std::vector<Marker> &markers) {
    std::scoped_lock lock(mutex_);
    markers_ = markers;
  }

  SourceConfig SourceConfiguration() const {
    std::scoped_lock lock(mutex_);
    return source_config_;
  }

  ProcessingConfig ProcessingConfiguration() const {
    std::scoped_lock lock(mutex_);
    return processing_config_;
  }

  std::vector<DiagnosticEntry> Diagnostics() const {
    std::scoped_lock lock(mutex_);
    return {diagnostics_.begin(), diagnostics_.end()};
  }

  std::vector<DiagnosticEntry> DrainDiagnostics() {
    std::scoped_lock lock(mutex_);
    std::vector<DiagnosticEntry> drained(diagnostics_.begin(), diagnostics_.end());
    diagnostics_.clear();
    return drained;
  }

  std::string LastError() const {
    std::scoped_lock lock(mutex_);
    return last_error_;
  }

private:
  void AppendDiagnosticLocked(const std::string &level,
                              const std::string &component,
                              const std::string &code,
                              const std::string &message,
                              const std::string &source_description = "") {
    DiagnosticEntry entry;
    entry.timestamp_utc = FormatUtcTimestamp(std::chrono::system_clock::now());
    entry.level = level;
    entry.component = component;
    entry.code = code;
    entry.message = message;
    entry.session_id = session_id_;
    entry.source_kind = SourceKindToString(source_config_.kind);
    entry.source_description =
        source_description.empty()
            ? (source_ ? source_->Description() : std::string("stopped"))
            : source_description;
    diagnostics_.push_back(std::move(entry));
    while (diagnostics_.size() > kMaxDiagnostics) {
      diagnostics_.pop_front();
    }
  }

  void Run() {
    std::vector<std::complex<float>> iq_samples;
    std::uint64_t sequence = 0;

    while (true) {
      SourceConfig source_config_copy;
      std::vector<Marker> markers_copy;
      sdr::ISampleSource *source = nullptr;
      bool needs_reconfigure = false;
      {
        std::scoped_lock lock(mutex_);
        if (!running_ || !source_) {
          break;
        }
        source = source_.get();
        source_config_copy = source_config_;
        markers_copy = markers_;
        needs_reconfigure = source_config_dirty_;
      }

      if (needs_reconfigure) {
        std::string config_error;
        if (!source->Configure(source_config_copy, config_error)) {
          std::scoped_lock lock(mutex_);
          last_error_ = BuildContextError("Live source reconfiguration",
                                          source_config_copy, config_error);
          AppendDiagnosticLocked("error", "source", "source.reconfigure_failed",
                                 last_error_, source->Description());
          running_ = false;
          break;
        }
        std::scoped_lock lock(mutex_);
        if (source_ && source_.get() == source) {
          source_config_dirty_ = false;
          AppendDiagnosticLocked("info", "source", "source.reconfigured",
                                 "Applied updated source configuration.",
                                 source->Description());
        }
      }

      {
        std::scoped_lock lock(mutex_);
        if (!running_ || !source_ || source_.get() != source) {
          break;
        }
      }

      std::string error;
      const std::size_t samples_read = source->ReadSamples(
          iq_samples, source_config_copy.frame_samples, error);
      if (samples_read == 0) {
        std::scoped_lock lock(mutex_);
        if (!running_) {
          break;
        }
        last_error_ = error.empty()
                          ? BuildContextError("Source read", source_config_copy,
                                              "Source returned no samples.")
                          : BuildContextError("Source read", source_config_copy,
                                              error);
        AppendDiagnosticLocked("error", "source", "source.read_failed",
                               last_error_, source->Description());
        running_ = false;
        break;
      }

      {
        std::scoped_lock recorder_lock(recorder_mutex_);
        if (recorder_.active()) {
          std::string record_error;
          if (!recorder_.Write(iq_samples, record_error)) {
            std::scoped_lock lock(mutex_);
            last_error_ = record_error;
            AppendDiagnosticLocked("error", "recorder",
                                   "recording.write_failed", record_error);
            recorder_.Stop();
          }
        }
      }

      AnalyzerSnapshot snapshot;
      {
        std::scoped_lock analysis_lock(analysis_mutex_);
        snapshot =
            analyzer_.Process(sequence++, source_config_copy.center_frequency_hz,
                              source_config_copy.sample_rate_hz, iq_samples,
                              markers_copy);
      }

      {
        std::scoped_lock lock(mutex_);
        if (!running_) {
          break;
        }
        snapshots_.push_back(std::move(snapshot));
        while (snapshots_.size() > kMaxSnapshots) {
          snapshots_.pop_front();
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    std::unique_ptr<sdr::ISampleSource> source_to_stop;
    {
      std::scoped_lock lock(mutex_);
      source_to_stop = std::move(source_);
      source_config_dirty_ = false;
      AppendDiagnosticLocked("info", "session", "session.worker_stopped",
                             "Analyzer session worker stopped.");
    }
    if (source_to_stop) {
      source_to_stop->Stop();
    }
  }

  mutable std::mutex mutex_;
  mutable std::mutex recorder_mutex_;
  mutable std::mutex analysis_mutex_;
  SourceConfig source_config_;
  dsp::Analyzer analyzer_;
  ProcessingConfig processing_config_;
  std::unique_ptr<sdr::ISampleSource> source_;
  io::Recorder recorder_;
  std::deque<AnalyzerSnapshot> snapshots_;
  std::deque<DiagnosticEntry> diagnostics_;
  std::thread worker_;
  std::vector<Marker> markers_;
  bool running_ = false;
  bool source_config_dirty_ = false;
  std::string last_error_;
  std::uint64_t session_id_ = 0;
};

AnalyzerSession::AnalyzerSession(SourceConfig source_config,
                                 ProcessingConfig processing_config)
    : impl_(std::make_unique<Impl>(std::move(source_config),
                                   std::move(processing_config))) {}

AnalyzerSession::~AnalyzerSession() = default;

bool AnalyzerSession::start() { return impl_->Start(); }
void AnalyzerSession::stop() { impl_->Stop(); }
bool AnalyzerSession::is_running() const { return impl_->IsRunning(); }
SessionStatus AnalyzerSession::status() const { return impl_->Status(); }
bool AnalyzerSession::update_source_config(const SourceConfig &config) {
  return impl_->UpdateSourceConfig(config);
}
void AnalyzerSession::update_processing_config(const ProcessingConfig &config) {
  impl_->UpdateProcessingConfig(config);
}
void AnalyzerSession::reset_peak_hold() { impl_->ResetPeakHold(); }
bool AnalyzerSession::start_recording(const RecordingConfig &config) {
  return impl_->StartRecording(config);
}
void AnalyzerSession::stop_recording() { impl_->StopRecording(); }
std::optional<AnalyzerSnapshot> AnalyzerSession::poll_snapshot() {
  return impl_->PollSnapshot();
}
void AnalyzerSession::set_markers(const std::vector<Marker> &markers) {
  impl_->SetMarkers(markers);
}
SourceConfig AnalyzerSession::source_config() const {
  return impl_->SourceConfiguration();
}
ProcessingConfig AnalyzerSession::processing_config() const {
  return impl_->ProcessingConfiguration();
}
std::vector<DiagnosticEntry> AnalyzerSession::diagnostics() const {
  return impl_->Diagnostics();
}
std::vector<DiagnosticEntry> AnalyzerSession::drain_diagnostics() {
  return impl_->DrainDiagnostics();
}
std::string AnalyzerSession::last_error() const { return impl_->LastError(); }

} // namespace sdr_analyzer

#include "sdr_analyzer/session.hpp"

#include <chrono>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

#include "src/dsp/analyzer.hpp"
#include "src/io/recorder.hpp"
#include "src/sdr/source_factory.hpp"

namespace sdr_analyzer {
namespace {

constexpr std::size_t kMaxSnapshots = 8;

} // namespace

class AnalyzerSession::Impl {
public:
  Impl(SourceConfig source_config, ProcessingConfig processing_config)
      : source_config_(std::move(source_config)), analyzer_(processing_config),
        processing_config_(processing_config) {}

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

    std::string error;
    auto source = sdr::CreateSource(source_config_, error);
    if (!source) {
      last_error_ = error;
      return false;
    }
    if (!source->Configure(source_config_, error) || !source->Start(error)) {
      last_error_ = error;
      return false;
    }

    source_ = std::move(source);
    running_ = true;
    source_config_dirty_ = false;
    worker_ = std::thread([this]() { Run(); });
    return true;
  }

  void Stop() {
    bool should_join = false;
    {
      std::scoped_lock lock(mutex_);
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
    return true;
  }

  void UpdateProcessingConfig(const ProcessingConfig &config) {
    {
      std::scoped_lock lock(mutex_);
      processing_config_ = config;
    }
    std::scoped_lock analysis_lock(analysis_mutex_);
    analyzer_.UpdateConfig(config);
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
      return false;
    }
    return true;
  }

  void StopRecording() {
    std::scoped_lock recorder_lock(recorder_mutex_);
    recorder_.Stop();
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

  std::string LastError() const {
    std::scoped_lock lock(mutex_);
    return last_error_;
  }

private:
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
          last_error_ = config_error;
          running_ = false;
          break;
        }
        std::scoped_lock lock(mutex_);
        if (source_ && source_.get() == source) {
          source_config_dirty_ = false;
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
        last_error_ = error.empty() ? "Source returned no samples." : error;
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
  std::thread worker_;
  std::vector<Marker> markers_;
  bool running_ = false;
  bool source_config_dirty_ = false;
  std::string last_error_;
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
std::string AnalyzerSession::last_error() const { return impl_->LastError(); }

} // namespace sdr_analyzer

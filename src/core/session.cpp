#include "sdr_analyzer/session.hpp"

#include <atomic>
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

}  // namespace

class AnalyzerSession::Impl {
 public:
  Impl(SourceConfig source_config, ProcessingConfig processing_config)
      : source_config_(std::move(source_config)), analyzer_(processing_config), processing_config_(processing_config) {}

  ~Impl() { Stop(); }

  bool Start() {
    std::scoped_lock lock(mutex_);
    if (running_) {
      return true;
    }

    std::string error;
    source_ = sdr::CreateSource(source_config_, error);
    if (!source_) {
      last_error_ = error;
      return false;
    }
    if (!source_->Configure(source_config_, error) || !source_->Start(error)) {
      last_error_ = error;
      source_.reset();
      return false;
    }

    running_ = true;
    worker_ = std::thread([this]() { Run(); });
    return true;
  }

  void Stop() {
    bool should_join = false;
    {
      std::scoped_lock lock(mutex_);
      should_join = worker_.joinable();
      if (running_) {
        running_ = false;
      }
    }

    if (should_join) {
      worker_.join();
    }

    std::scoped_lock lock(mutex_);
    if (source_) {
      source_->Stop();
      source_.reset();
    }
    recorder_.Stop();
  }

  bool IsRunning() const {
    std::scoped_lock lock(mutex_);
    return running_;
  }

  SessionStatus Status() const {
    std::scoped_lock lock(mutex_);
    SessionStatus status;
    status.running = running_;
    status.source_description = source_ ? source_->Description() : "stopped";
    if (recorder_.active()) {
      status.recording = recorder_.status();
    }
    return status;
  }

  bool UpdateSourceConfig(const SourceConfig& config) {
    std::scoped_lock lock(mutex_);
    source_config_ = config;
    if (source_) {
      std::string error;
      if (!source_->Configure(source_config_, error)) {
        last_error_ = error;
        return false;
      }
    }
    return true;
  }

  void UpdateProcessingConfig(const ProcessingConfig& config) {
    std::scoped_lock lock(mutex_);
    processing_config_ = config;
    analyzer_.UpdateConfig(config);
  }

  bool StartRecording(const RecordingConfig& config) {
    std::scoped_lock lock(mutex_);
    std::string error;
    if (!recorder_.Start(config, source_config_, error)) {
      last_error_ = error;
      return false;
    }
    return true;
  }

  void StopRecording() {
    std::scoped_lock lock(mutex_);
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

  void SetMarkers(const std::vector<Marker>& markers) {
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
      {
        std::scoped_lock lock(mutex_);
        if (!running_) {
          break;
        }
        source_config_copy = source_config_;
        markers_copy = markers_;
        std::string error;
        if (!source_) {
          break;
        }

        const std::size_t samples_read =
            source_->ReadSamples(iq_samples, source_config_copy.frame_samples, error);
        if (samples_read == 0) {
          last_error_ = error.empty() ? "Source returned no samples." : error;
          running_ = false;
          break;
        }

        if (recorder_.active()) {
          std::string record_error;
          if (!recorder_.Write(iq_samples, record_error)) {
            last_error_ = record_error;
            recorder_.Stop();
          }
        }
        auto snapshot = analyzer_.Process(
            sequence++,
            source_config_copy.center_frequency_hz,
            source_config_copy.sample_rate_hz,
            iq_samples,
            markers_copy);

        snapshots_.push_back(std::move(snapshot));
        while (snapshots_.size() > kMaxSnapshots) {
          snapshots_.pop_front();
        }
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    if (source_) {
      source_->Stop();
    }
  }

  mutable std::mutex mutex_;
  SourceConfig source_config_;
  dsp::Analyzer analyzer_;
  ProcessingConfig processing_config_;
  std::unique_ptr<sdr::ISampleSource> source_;
  io::Recorder recorder_;
  std::deque<AnalyzerSnapshot> snapshots_;
  std::thread worker_;
  std::vector<Marker> markers_;
  bool running_ = false;
  std::string last_error_;
};

AnalyzerSession::AnalyzerSession(SourceConfig source_config, ProcessingConfig processing_config)
    : impl_(new Impl(std::move(source_config), std::move(processing_config))) {}

AnalyzerSession::~AnalyzerSession() { delete impl_; }

bool AnalyzerSession::start() { return impl_->Start(); }
void AnalyzerSession::stop() { impl_->Stop(); }
bool AnalyzerSession::is_running() const { return impl_->IsRunning(); }
SessionStatus AnalyzerSession::status() const { return impl_->Status(); }
bool AnalyzerSession::update_source_config(const SourceConfig& config) { return impl_->UpdateSourceConfig(config); }
void AnalyzerSession::update_processing_config(const ProcessingConfig& config) { impl_->UpdateProcessingConfig(config); }
bool AnalyzerSession::start_recording(const RecordingConfig& config) { return impl_->StartRecording(config); }
void AnalyzerSession::stop_recording() { impl_->StopRecording(); }
std::optional<AnalyzerSnapshot> AnalyzerSession::poll_snapshot() { return impl_->PollSnapshot(); }
void AnalyzerSession::set_markers(const std::vector<Marker>& markers) { impl_->SetMarkers(markers); }
SourceConfig AnalyzerSession::source_config() const { return impl_->SourceConfiguration(); }
ProcessingConfig AnalyzerSession::processing_config() const { return impl_->ProcessingConfiguration(); }
std::string AnalyzerSession::last_error() const { return impl_->LastError(); }

}  // namespace sdr_analyzer

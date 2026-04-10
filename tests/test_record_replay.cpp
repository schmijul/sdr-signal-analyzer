#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "sdr_analyzer/session.hpp"

namespace {

using sdr_analyzer::AnalyzerSession;
using sdr_analyzer::ProcessingConfig;
using sdr_analyzer::RecordingConfig;
using sdr_analyzer::RecordingFormat;
using sdr_analyzer::SourceConfig;
using sdr_analyzer::SourceKind;

void Require(const bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

template <typename Predicate>
auto WaitForSnapshot(AnalyzerSession& session, Predicate predicate) -> sdr_analyzer::AnalyzerSnapshot {
  for (int attempt = 0; attempt < 80; ++attempt) {
    if (auto snapshot = session.poll_snapshot()) {
      if (predicate(*snapshot)) {
        return *snapshot;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }
  throw std::runtime_error("Timed out waiting for expected snapshot.");
}

void TestRawRecordingReplay() {
  const auto temp_root = std::filesystem::temp_directory_path() / "sdr_analyzer_record_replay_test";
  std::filesystem::create_directories(temp_root);
  const auto base_path = (temp_root / "sim_capture").string();

  SourceConfig live_source;
  live_source.kind = SourceKind::kSimulator;
  live_source.frame_samples = 2048;
  live_source.sample_rate_hz = 2.4e6;
  live_source.center_frequency_hz = 100e6;

  ProcessingConfig processing;
  processing.fft_size = 2048;
  processing.display_samples = 1024;

  AnalyzerSession live_session(live_source, processing);
  Require(live_session.start(), "Simulator session failed to start: " + live_session.last_error());

  RecordingConfig recording;
  recording.format = RecordingFormat::kRawBin;
  recording.base_path = base_path;
  Require(live_session.start_recording(recording), "Failed to start raw recording: " + live_session.last_error());

  const auto recorded_snapshot = WaitForSnapshot(
      live_session,
      [](const sdr_analyzer::AnalyzerSnapshot& snapshot) { return !snapshot.analysis.detections.empty(); });
  live_session.stop_recording();
  live_session.stop();

  Require(std::filesystem::exists(base_path + ".bin"), "Raw recording file was not created.");
  Require(std::filesystem::exists(base_path + ".bin.json"), "Raw metadata file was not created.");

  SourceConfig replay_source;
  replay_source.kind = SourceKind::kReplay;
  replay_source.input_path = base_path + ".bin";
  replay_source.metadata_path = base_path + ".bin.json";
  replay_source.frame_samples = 2048;

  AnalyzerSession replay_session(replay_source, processing);
  Require(replay_session.start(), "Replay session failed to start: " + replay_session.last_error());

  const auto replay_snapshot = WaitForSnapshot(
      replay_session,
      [](const sdr_analyzer::AnalyzerSnapshot& snapshot) { return !snapshot.analysis.detections.empty(); });
  replay_session.stop();

  Require(
      !recorded_snapshot.analysis.detections.empty() && !replay_snapshot.analysis.detections.empty(),
      "Expected detections from live and replay sessions.");

  const auto& live_detection = recorded_snapshot.analysis.detections.front();
  const auto& replay_detection = replay_snapshot.analysis.detections.front();

  Require(
      std::abs(live_detection.center_frequency_hz - replay_detection.center_frequency_hz) < 2000.0,
      "Replay center frequency drifted too far from live capture.");
  Require(
      std::abs(live_detection.peak_power_dbfs - replay_detection.peak_power_dbfs) < 1.0,
      "Replay peak power does not match live capture closely enough.");

  std::filesystem::remove(base_path + ".bin");
  std::filesystem::remove(base_path + ".bin.json");
  std::filesystem::remove_all(temp_root);
}

}  // namespace

int main() {
  try {
    TestRawRecordingReplay();
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
  return 0;
}

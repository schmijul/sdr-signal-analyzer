#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "src/io/metadata.hpp"
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

void WriteTruncatedFloatCapture(const std::string& path) {
  std::ofstream file(path, std::ios::binary);
  const float sample = 0.5f;
  file.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
}

template <typename CharT>
bool Contains(const std::basic_string<CharT>& text, const std::string& needle) {
  return text.find(needle) != std::basic_string<CharT>::npos;
}

void TestRecordingReplay(const RecordingFormat format, const std::string& suffix) {
  const auto temp_root = std::filesystem::temp_directory_path() / "sdr_analyzer_record_replay_test";
  std::filesystem::create_directories(temp_root);
  const auto base_path = (temp_root / ("sim_capture_" + suffix)).string();

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
  recording.format = format;
  recording.base_path = base_path;
  Require(live_session.start_recording(recording), "Failed to start recording: " + live_session.last_error());

  const auto recorded_snapshot = WaitForSnapshot(
      live_session,
      [](const sdr_analyzer::AnalyzerSnapshot& snapshot) { return !snapshot.analysis.detections.empty(); });
  live_session.stop_recording();
  live_session.stop();

  const std::string data_path = format == RecordingFormat::kSigMF ? base_path + ".sigmf-data" : base_path + ".bin";
  const std::string meta_path = format == RecordingFormat::kSigMF ? base_path + ".sigmf-meta" : base_path + ".bin.json";

  Require(std::filesystem::exists(data_path), "Recording data file was not created.");
  Require(std::filesystem::exists(meta_path), "Recording metadata file was not created.");

  SourceConfig replay_source;
  replay_source.kind = SourceKind::kReplay;
  replay_source.input_path = data_path;
  replay_source.metadata_path = meta_path;
  replay_source.frame_samples = 2048;
  replay_source.sample_format = sdr_analyzer::SampleFormat::kComplexFloat32;

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

  std::filesystem::remove(data_path);
  std::filesystem::remove(meta_path);
  std::filesystem::remove_all(temp_root);
}

void TestMetadataFailurePaths() {
  const auto temp_root = std::filesystem::temp_directory_path() / "sdr_analyzer_metadata_failure_test";
  std::filesystem::create_directories(temp_root);
  const auto missing_path = (temp_root / "missing.sigmf-meta").string();

  sdr_analyzer::SourceConfig config;
  std::string error;
  Require(!sdr_analyzer::io::ReadRawMetadata(missing_path, config, error), "ReadRawMetadata should fail for a missing file.");
  Require(!sdr_analyzer::io::ReadSigmfMetadata(missing_path, config, error), "ReadSigmfMetadata should fail for a missing file.");
  Require(!error.empty(), "Metadata failure should return an error string.");

  std::filesystem::remove_all(temp_root);
}

void TestMalformedReplayDataFailsGracefully() {
  const auto temp_root = std::filesystem::temp_directory_path() / "sdr_analyzer_malformed_replay_test";
  std::filesystem::create_directories(temp_root);
  const auto data_path = (temp_root / "truncated.bin").string();
  const auto meta_path = (temp_root / "truncated.bin.json").string();

  WriteTruncatedFloatCapture(data_path);

  sdr_analyzer::SourceConfig source;
  source.kind = SourceKind::kReplay;
  source.input_path = data_path;
  source.metadata_path = meta_path;
  source.sample_format = sdr_analyzer::SampleFormat::kComplexFloat32;
  source.frame_samples = 1024;

  ProcessingConfig processing;
  processing.fft_size = 1024;

  AnalyzerSession session(source, processing);
  Require(session.start(), "Malformed replay should still start the reader.");

  for (int attempt = 0; attempt < 20 && session.is_running(); ++attempt) {
    std::this_thread::sleep_for(std::chrono::milliseconds(25));
  }

  Require(!session.is_running(), "Malformed replay should stop after reading truncated data.");
  Require(Contains(session.last_error(), "end of stream"), "Malformed replay should report end-of-stream.");

  session.stop();
  std::filesystem::remove_all(temp_root);
}

}  // namespace

int main() {
  try {
    TestRecordingReplay(RecordingFormat::kRawBin, "raw");
    TestRecordingReplay(RecordingFormat::kSigMF, "sigmf");
    TestMetadataFailurePaths();
    TestMalformedReplayDataFailsGracefully();
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
  return 0;
}

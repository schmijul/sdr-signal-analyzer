#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "sdr_analyzer/session.hpp"

namespace {

using sdr_analyzer::AnalyzerSession;
using sdr_analyzer::AnalyzerSnapshot;
using sdr_analyzer::ProcessingConfig;
using sdr_analyzer::SourceConfig;
using sdr_analyzer::SourceKind;

const auto kFixtureRoot =
    std::filesystem::path(__FILE__).parent_path() / "fixtures";

void Require(const bool condition, const std::string &message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

template <typename Predicate>
AnalyzerSnapshot WaitForSnapshot(AnalyzerSession &session, Predicate predicate,
                                 const std::chrono::milliseconds timeout =
                                     std::chrono::milliseconds(1200)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (std::chrono::steady_clock::now() < deadline) {
    if (auto snapshot = session.poll_snapshot()) {
      if (predicate(*snapshot)) {
        return *snapshot;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  throw std::runtime_error("Timed out waiting for expected snapshot.");
}

void TestStartStopSmoke() {
  SourceConfig source;
  source.kind = SourceKind::kSimulator;
  source.frame_samples = 1024;

  ProcessingConfig processing;
  processing.fft_size = 1024;

  AnalyzerSession session(source, processing);
  Require(session.start(), "Simulator session failed to start.");
  Require(session.is_running(), "Session should report running after start.");
  session.stop();
  Require(!session.is_running(), "Session should stop cleanly.");
}

void TestInvalidSimulatorConfigFailsFast() {
  SourceConfig source;
  source.kind = SourceKind::kSimulator;
  source.sample_rate_hz = 0.0;

  ProcessingConfig processing;
  processing.fft_size = 1024;

  AnalyzerSession session(source, processing);
  Require(!session.start(),
          "Simulator session should reject zero sample rate.");
  Require(session.last_error().find("Sample rate must be positive.") !=
              std::string::npos,
          "Expected a sample-rate validation error.");
  const auto diagnostics = session.diagnostics();
  Require(!diagnostics.empty(),
          "Failed start should leave behind diagnostics for inspection.");
  Require(diagnostics.back().code == "source.configure_failed" ||
              diagnostics.back().code == "source.start_failed" ||
              diagnostics.back().code == "source.create_failed",
          "Expected a start failure diagnostic code.");
  Require(diagnostics.back().source_kind == "simulator",
          "Expected simulator source kind in diagnostics.");
}

void TestSourceUpdateIsFastAndEventuallyApplied() {
  SourceConfig source;
  source.kind = SourceKind::kSimulator;
  source.frame_samples = 1024;
  source.center_frequency_hz = 100e6;

  ProcessingConfig processing;
  processing.fft_size = 1024;
  processing.display_samples = 1024;

  AnalyzerSession session(source, processing);
  Require(session.start(),
          "Simulator session failed to start: " + session.last_error());
  WaitForSnapshot(session, [](const AnalyzerSnapshot &) { return true; });

  SourceConfig updated = session.source_config();
  updated.center_frequency_hz = 101e6;
  updated.gain_db += 3.0;

  const auto update_start = std::chrono::steady_clock::now();
  Require(session.update_source_config(updated),
          "update_source_config should succeed while running.");
  const auto update_elapsed = std::chrono::steady_clock::now() - update_start;
  Require(update_elapsed < std::chrono::milliseconds(50),
          "update_source_config should return promptly.");

  const auto updated_snapshot = WaitForSnapshot(
      session, [](const AnalyzerSnapshot &snapshot) {
        return std::abs(snapshot.spectrum.center_frequency_hz - 101e6) < 1.0;
      });
  Require(std::abs(updated_snapshot.spectrum.center_frequency_hz - 101e6) < 1.0,
          "Updated source configuration was not applied to later snapshots.");

  session.stop();
  Require(!session.is_running(), "Session should stop after config updates.");
}

void TestInvalidRuntimeReconfigureStopsSession() {
  SourceConfig source;
  source.kind = SourceKind::kSimulator;
  source.frame_samples = 1024;

  ProcessingConfig processing;
  processing.fft_size = 1024;
  processing.display_samples = 1024;

  AnalyzerSession session(source, processing);
  Require(session.start(), "Simulator session failed to start.");
  WaitForSnapshot(session, [](const AnalyzerSnapshot &) { return true; });

  SourceConfig updated = session.source_config();
  updated.sample_rate_hz = 0.0;

  Require(session.update_source_config(updated),
          "update_source_config should accept new values for revalidation.");

  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (session.is_running() && std::chrono::steady_clock::now() < deadline) {
    session.poll_snapshot();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }

  Require(!session.is_running(),
          "Session should stop after invalid reconfigure is applied.");
  Require(session.last_error().find("Sample rate must be positive.") !=
              std::string::npos,
          "Expected runtime reconfigure failure to report sample-rate error.");
  session.stop();
}

void TestReplayEofStopsSessionAndAllowsRestart() {
  SourceConfig source;
  source.kind = SourceKind::kReplay;
  source.input_path = (kFixtureRoot / "tone_cf32.sigmf-data").string();
  source.metadata_path = (kFixtureRoot / "tone_cf32.sigmf-meta").string();
  source.frame_samples = 2048;

  ProcessingConfig processing;
  processing.fft_size = 2048;
  processing.display_samples = 1024;

  AnalyzerSession session(source, processing);
  Require(session.start(),
          "Replay session failed to start: " + session.last_error());
  WaitForSnapshot(session, [](const AnalyzerSnapshot &) { return true; });

  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (session.is_running() && std::chrono::steady_clock::now() < deadline) {
    session.poll_snapshot();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  Require(!session.is_running(), "Replay session should stop after EOF.");
  Require(session.last_error().find("end of stream") != std::string::npos,
          "Replay EOF should be reported to callers.");

  Require(session.start(),
          "Session should be restartable after replay EOF: " +
              session.last_error());
  WaitForSnapshot(session, [](const AnalyzerSnapshot &) { return true; });
  session.stop();
}

void TestDiagnosticsCanBeDrained() {
  SourceConfig source;
  source.kind = SourceKind::kReplay;
  source.input_path = (kFixtureRoot / "tone_cf32.sigmf-data").string();
  source.metadata_path = (kFixtureRoot / "tone_cf32.sigmf-meta").string();
  source.frame_samples = 2048;

  ProcessingConfig processing;
  processing.fft_size = 2048;
  processing.display_samples = 1024;

  AnalyzerSession session(source, processing);
  Require(session.start(),
          "Replay session failed to start: " + session.last_error());
  auto startup_diagnostics = session.drain_diagnostics();
  Require(!startup_diagnostics.empty(),
          "Starting a session should emit diagnostics.");
  Require(session.diagnostics().empty(),
          "drain_diagnostics should clear the buffered diagnostics.");

  WaitForSnapshot(session, [](const AnalyzerSnapshot &) { return true; });
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (session.is_running() && std::chrono::steady_clock::now() < deadline) {
    session.poll_snapshot();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  auto stop_diagnostics = session.drain_diagnostics();
  Require(!stop_diagnostics.empty(),
          "Runtime stop should emit diagnostics for EOF visibility.");
  Require(stop_diagnostics.back().code == "session.worker_stopped" ||
              stop_diagnostics.front().code == "source.read_failed",
          "Expected source/session stop diagnostics after replay EOF.");
  session.stop();
}

void TestMissingOptionalBackendProducesDiagnostics() {
  SourceConfig source;
  source.kind = SourceKind::kUhd;

  ProcessingConfig processing;
  processing.fft_size = 1024;

  AnalyzerSession session(source, processing);
  Require(!session.start(),
          "UHD session should fail when the backend is unavailable.");
  Require(session.last_error().find("Source creation for source 'uhd' failed") !=
              std::string::npos,
          "Expected contextual startup failure for a missing backend.");

  const auto diagnostics = session.diagnostics();
  Require(!diagnostics.empty(), "Expected buffered diagnostics on startup failure.");
  const auto &entry = diagnostics.back();
  Require(entry.code == "source.create_failed",
          "Expected source.create_failed diagnostic code.");
  Require(entry.source_kind == "uhd",
          "Expected diagnostic to preserve the requested source kind.");
  Require(entry.session_id > 0, "Expected a non-zero diagnostic session id.");
}

} // namespace

int main() {
  try {
    TestInvalidSimulatorConfigFailsFast();
    TestStartStopSmoke();
    TestSourceUpdateIsFastAndEventuallyApplied();
    TestInvalidRuntimeReconfigureStopsSession();
    TestReplayEofStopsSessionAndAllowsRestart();
    TestDiagnosticsCanBeDrained();
    TestMissingOptionalBackendProducesDiagnostics();
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
  return 0;
}

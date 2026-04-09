#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#include "sdr_analyzer/session.hpp"

namespace {

using sdr_analyzer::AnalyzerSession;
using sdr_analyzer::ProcessingConfig;
using sdr_analyzer::RecordingConfig;
using sdr_analyzer::RecordingFormat;
using sdr_analyzer::SourceConfig;
using sdr_analyzer::SourceKind;

void PrintUsage() {
  std::cout
      << "Usage: sdr-analyzer-cli [--source simulator|replay|soapy] [--input FILE] [--meta FILE]\n"
      << "                        [--center-hz HZ] [--sample-rate HZ] [--gain-db DB]\n"
      << "                        [--fft-size N] [--frames N] [--record-base PATH]\n"
      << "                        [--record-format raw|sigmf] [--loop]\n";
}

SourceKind ParseSourceKind(const std::string& value) {
  if (value == "replay") {
    return SourceKind::kReplay;
  }
  if (value == "soapy") {
    return SourceKind::kSoapy;
  }
  return SourceKind::kSimulator;
}

RecordingFormat ParseRecordingFormat(const std::string& value) {
  return value == "sigmf" ? RecordingFormat::kSigMF : RecordingFormat::kRawBin;
}

}  // namespace

int main(int argc, char** argv) {
  SourceConfig source_config;
  ProcessingConfig processing_config;
  RecordingConfig recording_config;
  bool enable_recording = false;
  int max_frames = 200;

  for (int index = 1; index < argc; ++index) {
    const std::string arg = argv[index];
    const auto next_value = [&](const char* name) -> std::string {
      if (index + 1 >= argc) {
        throw std::runtime_error(std::string("Missing value for ") + name);
      }
      return argv[++index];
    };

    if (arg == "--source") {
      source_config.kind = ParseSourceKind(next_value("--source"));
    } else if (arg == "--input") {
      source_config.input_path = next_value("--input");
    } else if (arg == "--meta") {
      source_config.metadata_path = next_value("--meta");
    } else if (arg == "--center-hz") {
      source_config.center_frequency_hz = std::stod(next_value("--center-hz"));
    } else if (arg == "--sample-rate") {
      source_config.sample_rate_hz = std::stod(next_value("--sample-rate"));
    } else if (arg == "--gain-db") {
      source_config.gain_db = std::stod(next_value("--gain-db"));
    } else if (arg == "--fft-size") {
      processing_config.fft_size = static_cast<std::size_t>(std::stoull(next_value("--fft-size")));
      processing_config.display_samples = processing_config.fft_size;
    } else if (arg == "--frames") {
      max_frames = std::stoi(next_value("--frames"));
    } else if (arg == "--record-base") {
      recording_config.base_path = next_value("--record-base");
      enable_recording = true;
    } else if (arg == "--record-format") {
      recording_config.format = ParseRecordingFormat(next_value("--record-format"));
    } else if (arg == "--loop") {
      source_config.loop_playback = true;
    } else if (arg == "--help") {
      PrintUsage();
      return 0;
    } else {
      std::cerr << "Unknown argument: " << arg << "\n";
      PrintUsage();
      return 1;
    }
  }

  AnalyzerSession session(source_config, processing_config);
  if (!session.start()) {
    std::cerr << "Failed to start session: " << session.last_error() << "\n";
    return 1;
  }

  if (enable_recording && !session.start_recording(recording_config)) {
    std::cerr << "Failed to start recording: " << session.last_error() << "\n";
    return 1;
  }

  int frames_seen = 0;
  while (frames_seen < max_frames) {
    if (auto snapshot = session.poll_snapshot()) {
      ++frames_seen;
      std::cout << "frame=" << snapshot->sequence << " noise=" << snapshot->analysis.noise_floor_dbfs
                << "dBFS strongest=" << snapshot->analysis.strongest_peak_dbfs << "dBFS detections="
                << snapshot->analysis.detections.size() << "\n";
      for (const auto& detection : snapshot->analysis.detections) {
        std::cout << "  peak=" << detection.center_frequency_hz << "Hz"
                  << " bw=" << detection.bandwidth_hz << "Hz"
                  << " power=" << detection.peak_power_dbfs << "dBFS"
                  << " label=" << detection.labels.front() << "\n";
      }
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
  }

  session.stop();
  return 0;
}

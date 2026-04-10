#include <chrono>
#include <cstring>
#include <exception>
#include <iostream>
#include <limits>
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

void PrintUsage() {
  std::cout
      << "Usage: sdr-analyzer-cli [--source "
         "simulator|replay|rtl_tcp|uhd|soapy] [--input FILE] [--meta FILE]\n"
      << "                        [--host HOST] [--port PORT] [--device-string "
         "TEXT] [--device-args TEXT]\n"
      << "                        [--channel N] [--antenna NAME] "
         "[--bandwidth-hz HZ]\n"
      << "                        [--clock-source NAME] [--time-source NAME]\n"
      << "                        [--center-hz HZ] [--sample-rate HZ] "
         "[--gain-db DB]\n"
      << "                        [--fft-size N] [--frames N] [--record-base "
         "PATH]\n"
      << "                        [--record-format raw|sigmf] [--loop]\n";
}

SourceKind ParseSourceKind(const std::string &value) {
  if (value == "replay") {
    return SourceKind::kReplay;
  }
  if (value == "rtl_tcp") {
    return SourceKind::kRtlTcp;
  }
  if (value == "uhd") {
    return SourceKind::kUhd;
  }
  if (value == "soapy") {
    return SourceKind::kSoapy;
  }
  return SourceKind::kSimulator;
}

RecordingFormat ParseRecordingFormat(const std::string &value) {
  return value == "sigmf" ? RecordingFormat::kSigMF : RecordingFormat::kRawBin;
}

double ParseDoubleOption(const char *name, const std::string &value) {
  std::size_t consumed = 0;
  try {
    const double parsed = std::stod(value, &consumed);
    if (consumed != value.size()) {
      throw std::invalid_argument("trailing characters");
    }
    return parsed;
  } catch (const std::exception &) {
    throw std::runtime_error(std::string("Invalid value for ") + name + ": " +
                             value);
  }
}

std::size_t ParseSizeOption(const char *name, const std::string &value) {
  std::size_t consumed = 0;
  try {
    const unsigned long long parsed = std::stoull(value, &consumed);
    if (consumed != value.size()) {
      throw std::invalid_argument("trailing characters");
    }
    return static_cast<std::size_t>(parsed);
  } catch (const std::exception &) {
    throw std::runtime_error(std::string("Invalid value for ") + name + ": " +
                             value);
  }
}

int ParseIntOption(const char *name, const std::string &value) {
  std::size_t consumed = 0;
  try {
    const long long parsed = std::stoll(value, &consumed);
    if (consumed != value.size()) {
      throw std::invalid_argument("trailing characters");
    }
    if (parsed < std::numeric_limits<int>::min() ||
        parsed > std::numeric_limits<int>::max()) {
      throw std::out_of_range("value out of range");
    }
    return static_cast<int>(parsed);
  } catch (const std::exception &) {
    throw std::runtime_error(std::string("Invalid value for ") + name + ": " +
                             value);
  }
}

} // namespace

int main(int argc, char **argv) {
  try {
    SourceConfig source_config;
    ProcessingConfig processing_config;
    RecordingConfig recording_config;
    bool enable_recording = false;
    int max_frames = 200;

    for (int index = 1; index < argc; ++index) {
      const std::string arg = argv[index];
      const auto next_value = [&](const char *name) -> std::string {
        if (index + 1 >= argc) {
          throw std::runtime_error(std::string("Missing value for ") + name);
        }
        return argv[++index];
      };

      if (arg == "--source") {
        source_config.kind = ParseSourceKind(next_value("--source"));
      } else if (arg == "--device-string") {
        source_config.device_string = next_value("--device-string");
      } else if (arg == "--device-args") {
        source_config.device_args = next_value("--device-args");
      } else if (arg == "--input") {
        source_config.input_path = next_value("--input");
      } else if (arg == "--meta") {
        source_config.metadata_path = next_value("--meta");
      } else if (arg == "--host") {
        source_config.network_host = next_value("--host");
      } else if (arg == "--port") {
        source_config.network_port =
            ParseIntOption("--port", next_value("--port"));
      } else if (arg == "--channel") {
        source_config.channel =
            ParseSizeOption("--channel", next_value("--channel"));
      } else if (arg == "--antenna") {
        source_config.antenna = next_value("--antenna");
      } else if (arg == "--bandwidth-hz") {
        source_config.bandwidth_hz =
            ParseDoubleOption("--bandwidth-hz", next_value("--bandwidth-hz"));
      } else if (arg == "--clock-source") {
        source_config.clock_source = next_value("--clock-source");
      } else if (arg == "--time-source") {
        source_config.time_source = next_value("--time-source");
      } else if (arg == "--center-hz") {
        source_config.center_frequency_hz =
            ParseDoubleOption("--center-hz", next_value("--center-hz"));
      } else if (arg == "--sample-rate") {
        source_config.sample_rate_hz =
            ParseDoubleOption("--sample-rate", next_value("--sample-rate"));
      } else if (arg == "--gain-db") {
        source_config.gain_db =
            ParseDoubleOption("--gain-db", next_value("--gain-db"));
      } else if (arg == "--fft-size") {
        processing_config.fft_size =
            ParseSizeOption("--fft-size", next_value("--fft-size"));
        processing_config.display_samples = processing_config.fft_size;
      } else if (arg == "--frames") {
        max_frames = ParseIntOption("--frames", next_value("--frames"));
      } else if (arg == "--record-base") {
        recording_config.base_path = next_value("--record-base");
        enable_recording = true;
      } else if (arg == "--record-format") {
        recording_config.format =
            ParseRecordingFormat(next_value("--record-format"));
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
      std::cerr << "Failed to start recording: " << session.last_error()
                << "\n";
      session.stop();
      return 1;
    }

    int frames_seen = 0;
    while (frames_seen < max_frames) {
      if (auto snapshot = session.poll_snapshot()) {
        ++frames_seen;
        std::cout << "frame=" << snapshot->sequence
                  << " noise=" << snapshot->analysis.noise_floor_dbfs
                  << "dBFS strongest=" << snapshot->analysis.strongest_peak_dbfs
                  << "dBFS detections=" << snapshot->analysis.detections.size()
                  << "\n";
        for (const auto &detection : snapshot->analysis.detections) {
          std::cout << "  peak=" << detection.center_frequency_hz << "Hz"
                    << " bw=" << detection.bandwidth_hz << "Hz"
                    << " power=" << detection.peak_power_dbfs << "dBFS"
                    << " label=" << detection.labels.front() << "\n";
        }
        continue;
      }

      if (!session.is_running()) {
        const std::string error = session.last_error();
        if (frames_seen == 0) {
          std::cerr << "Session ended before producing a frame";
          if (!error.empty()) {
            std::cerr << ": " << error;
          }
          std::cerr << "\n";
          session.stop();
          return 1;
        }
        if (!error.empty()) {
          std::cerr << "Session stopped after " << frames_seen
                    << " frame(s): " << error << "\n";
        }
        break;
      }

      std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    session.stop();
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}

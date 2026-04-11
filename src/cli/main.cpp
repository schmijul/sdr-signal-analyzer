#include <chrono>
#include <cstring>
#include <exception>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>

#include "sdr_analyzer/session.hpp"

namespace {

using sdr_analyzer::AnalysisReport;
using sdr_analyzer::AnalyzerSession;
using sdr_analyzer::AnalyzerSnapshot;
using sdr_analyzer::DiagnosticEntry;
using sdr_analyzer::DetectionResult;
using sdr_analyzer::MarkerMeasurement;
using sdr_analyzer::ProcessingConfig;
using sdr_analyzer::RecordingConfig;
using sdr_analyzer::RecordingFormat;
using sdr_analyzer::SampleFormat;
using sdr_analyzer::SourceConfig;
using sdr_analyzer::SourceKind;

constexpr std::string_view kExportFormatVersion =
    "sdr_signal_analyzer.measurements.v1";

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
      << "                        [--record-format raw|sigmf] [--loop]\n"
      << "                        [--log-level error|warn|info|debug] "
         "[--log-file PATH] [--log-json]\n"
      << "                        [--export-jsonl PATH] "
         "[--export-interval N]\n";
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

enum class LogLevel {
  kError = 0,
  kWarn = 1,
  kInfo = 2,
  kDebug = 3,
};

LogLevel ParseLogLevel(const std::string &value) {
  if (value == "error") {
    return LogLevel::kError;
  }
  if (value == "warn") {
    return LogLevel::kWarn;
  }
  if (value == "info") {
    return LogLevel::kInfo;
  }
  if (value == "debug") {
    return LogLevel::kDebug;
  }
  throw std::runtime_error("Invalid value for --log-level: " + value);
}

LogLevel DiagnosticLevel(const std::string &value) {
  if (value == "error") {
    return LogLevel::kError;
  }
  if (value == "warn") {
    return LogLevel::kWarn;
  }
  if (value == "debug") {
    return LogLevel::kDebug;
  }
  return LogLevel::kInfo;
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

std::string SampleFormatToString(const SampleFormat format) {
  switch (format) {
  case SampleFormat::kComplexFloat32:
    return "complex_float32";
  case SampleFormat::kComplexInt16:
    return "complex_int16";
  }
  return "unknown";
}

std::string RecordingFormatToString(const RecordingFormat format) {
  switch (format) {
  case RecordingFormat::kRawBin:
    return "raw";
  case RecordingFormat::kSigMF:
    return "sigmf";
  }
  return "unknown";
}

std::string JsonEscape(const std::string_view value) {
  std::string escaped;
  escaped.reserve(value.size() + 8);
  for (const char character : value) {
    switch (character) {
    case '\\':
      escaped += "\\\\";
      break;
    case '"':
      escaped += "\\\"";
      break;
    case '\b':
      escaped += "\\b";
      break;
    case '\f':
      escaped += "\\f";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      if (static_cast<unsigned char>(character) < 0x20) {
        std::ostringstream hex;
        hex << "\\u" << std::hex << std::setw(4) << std::setfill('0')
            << static_cast<int>(static_cast<unsigned char>(character));
        escaped += hex.str();
      } else {
        escaped += character;
      }
      break;
    }
  }
  return escaped;
}

std::string JsonQuote(const std::string_view value) {
  return "\"" + JsonEscape(value) + "\"";
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

void AppendStringField(std::ostream &output, const std::string_view name,
                       const std::string_view value, const bool trailing_comma) {
  output << JsonQuote(name) << ':' << JsonQuote(value);
  if (trailing_comma) {
    output << ',';
  }
}

void AppendDoubleField(std::ostream &output, const std::string_view name,
                       const double value, const bool trailing_comma) {
  output << JsonQuote(name) << ':' << std::setprecision(15) << value;
  if (trailing_comma) {
    output << ',';
  }
}

void AppendIntegerField(std::ostream &output, const std::string_view name,
                        const std::uint64_t value, const bool trailing_comma) {
  output << JsonQuote(name) << ':' << value;
  if (trailing_comma) {
    output << ',';
  }
}

void AppendSizeField(std::ostream &output, const std::string_view name,
                     const std::size_t value, const bool trailing_comma) {
  output << JsonQuote(name) << ':' << value;
  if (trailing_comma) {
    output << ',';
  }
}

void AppendBoolField(std::ostream &output, const std::string_view name,
                     const bool value, const bool trailing_comma) {
  output << JsonQuote(name) << ':' << (value ? "true" : "false");
  if (trailing_comma) {
    output << ',';
  }
}

void AppendStringArray(std::ostream &output,
                       const std::vector<std::string> &values) {
  output << '[';
  for (std::size_t index = 0; index < values.size(); ++index) {
    if (index > 0) {
      output << ',';
    }
    output << JsonQuote(values[index]);
  }
  output << ']';
}

void AppendDetectionsArray(std::ostream &output,
                           const std::vector<DetectionResult> &detections) {
  output << '[';
  for (std::size_t index = 0; index < detections.size(); ++index) {
    if (index > 0) {
      output << ',';
    }
    const auto &detection = detections[index];
    output << '{';
    AppendDoubleField(output, "center_frequency_hz",
                      detection.center_frequency_hz, true);
    AppendDoubleField(output, "offset_hz", detection.offset_hz, true);
    AppendDoubleField(output, "bandwidth_hz", detection.bandwidth_hz, true);
    AppendDoubleField(output, "peak_power_dbfs", detection.peak_power_dbfs, true);
    output << JsonQuote("labels") << ':';
    AppendStringArray(output, detection.labels);
    output << '}';
  }
  output << ']';
}

void AppendMarkerMeasurementsArray(
    std::ostream &output,
    const std::vector<MarkerMeasurement> &marker_measurements) {
  output << '[';
  for (std::size_t index = 0; index < marker_measurements.size(); ++index) {
    if (index > 0) {
      output << ',';
    }
    const auto &marker = marker_measurements[index];
    output << '{';
    AppendDoubleField(output, "center_frequency_hz", marker.center_frequency_hz,
                      true);
    AppendDoubleField(output, "bandwidth_hz", marker.bandwidth_hz, true);
    AppendDoubleField(output, "peak_power_dbfs", marker.peak_power_dbfs, true);
    AppendDoubleField(output, "average_power_dbfs", marker.average_power_dbfs,
                      false);
    output << '}';
  }
  output << ']';
}

void AppendSourceConfigObject(std::ostream &output, const SourceConfig &config) {
  output << '{';
  AppendStringField(output, "kind", SourceKindToString(config.kind), true);
  AppendStringField(output, "device_string", config.device_string, true);
  AppendStringField(output, "device_args", config.device_args, true);
  AppendStringField(output, "input_path", config.input_path, true);
  AppendStringField(output, "metadata_path", config.metadata_path, true);
  AppendStringField(output, "network_host", config.network_host, true);
  AppendIntegerField(output, "network_port",
                     static_cast<std::uint64_t>(config.network_port), true);
  AppendSizeField(output, "channel", config.channel, true);
  AppendStringField(output, "antenna", config.antenna, true);
  AppendDoubleField(output, "bandwidth_hz", config.bandwidth_hz, true);
  AppendStringField(output, "clock_source", config.clock_source, true);
  AppendStringField(output, "time_source", config.time_source, true);
  AppendDoubleField(output, "center_frequency_hz", config.center_frequency_hz,
                    true);
  AppendDoubleField(output, "sample_rate_hz", config.sample_rate_hz, true);
  AppendDoubleField(output, "gain_db", config.gain_db, true);
  AppendSizeField(output, "frame_samples", config.frame_samples, true);
  AppendBoolField(output, "loop_playback", config.loop_playback, true);
  AppendStringField(output, "sample_format",
                    SampleFormatToString(config.sample_format), false);
  output << '}';
}

void AppendProcessingConfigObject(std::ostream &output,
                                  const ProcessingConfig &config) {
  output << '{';
  AppendSizeField(output, "fft_size", config.fft_size, true);
  AppendSizeField(output, "display_samples", config.display_samples, true);
  AppendDoubleField(output, "averaging_factor", config.averaging_factor, true);
  AppendBoolField(output, "peak_hold_enabled", config.peak_hold_enabled, true);
  AppendDoubleField(output, "detection_threshold_db",
                    config.detection_threshold_db, true);
  AppendSizeField(output, "max_peaks", config.max_peaks, true);
  AppendSizeField(output, "minimum_peak_spacing_bins",
                  config.minimum_peak_spacing_bins, true);
  AppendDoubleField(output, "bandwidth_threshold_db",
                    config.bandwidth_threshold_db, false);
  output << '}';
}

std::string BuildMetadataRecord(
    const SourceConfig &source_config, const ProcessingConfig &processing_config,
    const std::chrono::system_clock::time_point export_started_at) {
  std::ostringstream output;
  output << '{';
  AppendStringField(output, "record_type", "metadata", true);
  AppendStringField(output, "format_version", kExportFormatVersion, true);
  AppendStringField(output, "export_started_at_utc",
                    FormatUtcTimestamp(export_started_at), true);
  output << JsonQuote("source_config") << ':';
  AppendSourceConfigObject(output, source_config);
  output << ',';
  output << JsonQuote("processing_config") << ':';
  AppendProcessingConfigObject(output, processing_config);
  output << ',';
  output << JsonQuote("marker_definitions") << ":[]";
  output << '}';
  return output.str();
}

std::string BuildFrameRecord(
    const AnalyzerSnapshot &snapshot, const SourceConfig &source_config,
    const std::chrono::system_clock::time_point frame_timestamp,
    const std::chrono::steady_clock::duration elapsed) {
  const AnalysisReport &analysis = snapshot.analysis;

  std::ostringstream output;
  output << '{';
  AppendStringField(output, "record_type", "frame", true);
  AppendStringField(output, "timestamp_utc", FormatUtcTimestamp(frame_timestamp),
                    true);
  AppendDoubleField(
      output, "elapsed_seconds",
      std::chrono::duration<double>(elapsed).count(), true);
  AppendIntegerField(output, "sequence", snapshot.sequence, true);
  AppendDoubleField(output, "center_frequency_hz",
                    snapshot.spectrum.center_frequency_hz, true);
  AppendDoubleField(output, "sample_rate_hz", source_config.sample_rate_hz,
                    true);
  AppendDoubleField(output, "noise_floor_dbfs", analysis.noise_floor_dbfs, true);
  AppendDoubleField(output, "strongest_peak_dbfs",
                    analysis.strongest_peak_dbfs, true);
  AppendDoubleField(output, "burst_score", analysis.burst_score, true);
  AppendSizeField(output, "detection_count", analysis.detections.size(), true);
  output << JsonQuote("detections") << ':';
  AppendDetectionsArray(output, analysis.detections);
  output << ',';
  output << JsonQuote("marker_measurements") << ':';
  AppendMarkerMeasurementsArray(output, analysis.marker_measurements);
  output << '}';
  return output.str();
}

std::string BuildDiagnosticRecord(const DiagnosticEntry &entry) {
  std::ostringstream output;
  output << '{';
  AppendStringField(output, "record_type", "diagnostic", true);
  AppendStringField(output, "timestamp_utc", entry.timestamp_utc, true);
  AppendStringField(output, "level", entry.level, true);
  AppendStringField(output, "component", entry.component, true);
  AppendStringField(output, "code", entry.code, true);
  AppendStringField(output, "message", entry.message, true);
  AppendIntegerField(output, "session_id", entry.session_id, true);
  AppendStringField(output, "source_kind", entry.source_kind, true);
  AppendStringField(output, "source_description", entry.source_description,
                    false);
  output << '}';
  return output.str();
}

class DiagnosticLogWriter {
public:
  DiagnosticLogWriter(LogLevel minimum_level, bool json_output,
                      std::optional<std::string> file_path)
      : minimum_level_(minimum_level), json_output_(json_output),
        file_path_(std::move(file_path)) {}

  bool Open() {
    if (!file_path_) {
      return true;
    }
    stream_.open(*file_path_, std::ios::out | std::ios::trunc);
    if (!stream_.is_open()) {
      last_error_ = "Failed to open log file: " + *file_path_;
      return false;
    }
    return true;
  }

  void Write(const DiagnosticEntry &entry) {
    if (DiagnosticLevel(entry.level) > minimum_level_) {
      return;
    }
    const std::string line =
        json_output_ ? BuildDiagnosticRecord(entry) : FormatHumanLine(entry);
    std::cerr << line << '\n';
    if (stream_.is_open()) {
      stream_ << line << '\n';
      stream_.flush();
    }
  }

  void DrainFromSession(AnalyzerSession &session) {
    for (const auto &entry : session.drain_diagnostics()) {
      Write(entry);
    }
  }

  const std::string &last_error() const { return last_error_; }

private:
  static std::string FormatHumanLine(const DiagnosticEntry &entry) {
    std::ostringstream output;
    output << '[' << entry.timestamp_utc << "] " << entry.level << ' '
           << entry.component << ' ' << entry.code << " session="
           << entry.session_id << " source="
           << entry.source_kind;
    if (!entry.source_description.empty()) {
      output << " desc=" << entry.source_description;
    }
    output << " message=" << entry.message;
    return output.str();
  }

  LogLevel minimum_level_;
  bool json_output_ = false;
  std::optional<std::string> file_path_;
  std::ofstream stream_;
  std::string last_error_;
};

class JsonlExportWriter {
public:
  JsonlExportWriter(std::string path, std::size_t interval,
                    SourceConfig source_config,
                    ProcessingConfig processing_config)
      : path_(std::move(path)), interval_(interval),
        source_config_(std::move(source_config)),
        processing_config_(std::move(processing_config)) {}

  bool Open() {
    stream_.open(path_, std::ios::out | std::ios::trunc);
    if (!stream_.is_open()) {
      last_error_ = "Failed to open export file: " + path_;
      return false;
    }

    export_started_at_system_ = std::chrono::system_clock::now();
    export_started_at_steady_ = std::chrono::steady_clock::now();
    stream_ << BuildMetadataRecord(source_config_, processing_config_,
                                   export_started_at_system_)
            << '\n';
    if (!stream_) {
      last_error_ = "Failed to write export metadata to: " + path_;
      return false;
    }
    return true;
  }

  bool MaybeWriteFrame(const AnalyzerSnapshot &snapshot) {
    if (snapshot.sequence % interval_ != 0) {
      return true;
    }

    const auto frame_timestamp = std::chrono::system_clock::now();
    const auto steady_now = std::chrono::steady_clock::now();
    stream_ << BuildFrameRecord(snapshot, source_config_, frame_timestamp,
                                steady_now - export_started_at_steady_)
            << '\n';
    if (!stream_) {
      last_error_ = "Failed to write export frame to: " + path_;
      return false;
    }
    return true;
  }

  const std::string &last_error() const { return last_error_; }

private:
  std::string path_;
  std::size_t interval_ = 1;
  SourceConfig source_config_;
  ProcessingConfig processing_config_;
  std::ofstream stream_;
  std::chrono::system_clock::time_point export_started_at_system_;
  std::chrono::steady_clock::time_point export_started_at_steady_;
  std::string last_error_;
};

} // namespace

int main(int argc, char **argv) {
  try {
    SourceConfig source_config;
    ProcessingConfig processing_config;
    RecordingConfig recording_config;
    bool enable_recording = false;
    int max_frames = 200;
    LogLevel log_level = LogLevel::kInfo;
    std::optional<std::string> log_file;
    bool log_json = false;
    std::optional<std::string> export_path;
    std::size_t export_interval = 1;

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
      } else if (arg == "--log-level") {
        log_level = ParseLogLevel(next_value("--log-level"));
      } else if (arg == "--log-file") {
        log_file = next_value("--log-file");
      } else if (arg == "--log-json") {
        log_json = true;
      } else if (arg == "--export-jsonl") {
        export_path = next_value("--export-jsonl");
      } else if (arg == "--export-interval") {
        export_interval =
            ParseSizeOption("--export-interval", next_value("--export-interval"));
      } else if (arg == "--help") {
        PrintUsage();
        return 0;
      } else {
        std::cerr << "Unknown argument: " << arg << "\n";
        PrintUsage();
        return 1;
      }
    }

    if (export_interval == 0) {
      std::cerr << "--export-interval must be at least 1\n";
      return 1;
    }

    DiagnosticLogWriter diagnostics(log_level, log_json, log_file);
    if (!diagnostics.Open()) {
      std::cerr << diagnostics.last_error() << "\n";
      return 1;
    }

    AnalyzerSession session(source_config, processing_config);
    if (!session.start()) {
      diagnostics.DrainFromSession(session);
      std::cerr << "Failed to start session: " << session.last_error() << "\n";
      return 1;
    }
    diagnostics.DrainFromSession(session);

    std::optional<JsonlExportWriter> export_writer;
    if (export_path) {
      export_writer.emplace(*export_path, export_interval, session.source_config(),
                            session.processing_config());
      if (!export_writer->Open()) {
        diagnostics.DrainFromSession(session);
        std::cerr << export_writer->last_error() << "\n";
        session.stop();
        diagnostics.DrainFromSession(session);
        return 1;
      }
    }

    if (enable_recording && !session.start_recording(recording_config)) {
      diagnostics.DrainFromSession(session);
      std::cerr << "Failed to start recording: " << session.last_error()
                << "\n";
      session.stop();
      diagnostics.DrainFromSession(session);
      return 1;
    }
    diagnostics.DrainFromSession(session);

    int frames_seen = 0;
    while (frames_seen < max_frames) {
      diagnostics.DrainFromSession(session);
      if (auto snapshot = session.poll_snapshot()) {
        ++frames_seen;
        if (export_writer && !export_writer->MaybeWriteFrame(*snapshot)) {
          diagnostics.DrainFromSession(session);
          std::cerr << export_writer->last_error() << "\n";
          session.stop();
          diagnostics.DrainFromSession(session);
          return 1;
        }

        std::cout << "frame=" << snapshot->sequence
                  << " noise=" << snapshot->analysis.noise_floor_dbfs
                  << "dBFS strongest=" << snapshot->analysis.strongest_peak_dbfs
                  << "dBFS detections=" << snapshot->analysis.detections.size()
                  << "\n";
        for (const auto &detection : snapshot->analysis.detections) {
          const std::string label =
              detection.labels.empty() ? "" : detection.labels.front();
          std::cout << "  peak=" << detection.center_frequency_hz << "Hz"
                    << " bw=" << detection.bandwidth_hz << "Hz"
                    << " power=" << detection.peak_power_dbfs << "dBFS"
                    << " label=" << label << "\n";
        }
        continue;
      }

      if (!session.is_running()) {
        diagnostics.DrainFromSession(session);
        const std::string error = session.last_error();
        if (frames_seen == 0) {
          std::cerr << "Session ended before producing a frame";
          if (!error.empty()) {
            std::cerr << ": " << error;
          }
          std::cerr << "\n";
          session.stop();
          diagnostics.DrainFromSession(session);
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
    diagnostics.DrainFromSession(session);
    return 0;
  } catch (const std::exception &ex) {
    std::cerr << ex.what() << "\n";
    return 1;
  }
}

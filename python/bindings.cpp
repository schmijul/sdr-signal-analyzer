#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "sdr_analyzer/session.hpp"

namespace py = pybind11;
using namespace sdr_analyzer;

PYBIND11_MODULE(_sdr_signal_analyzer, module) {
  module.doc() = "Bindings for the sdr_signal_analyzer C++ backend.";

  py::enum_<SourceKind>(module, "SourceKind")
      .value("SIMULATOR", SourceKind::kSimulator)
      .value("REPLAY", SourceKind::kReplay)
      .value("RTL_TCP", SourceKind::kRtlTcp)
      .value("UHD", SourceKind::kUhd)
      .value("SOAPY", SourceKind::kSoapy);

  py::enum_<SampleFormat>(module, "SampleFormat")
      .value("COMPLEX_FLOAT32", SampleFormat::kComplexFloat32)
      .value("COMPLEX_INT16", SampleFormat::kComplexInt16);

  py::enum_<RecordingFormat>(module, "RecordingFormat")
      .value("RAW_BIN", RecordingFormat::kRawBin)
      .value("SIGMF", RecordingFormat::kSigMF);

  py::class_<SourceConfig>(module, "SourceConfig")
      .def(py::init<>())
      .def_readwrite("kind", &SourceConfig::kind)
      .def_readwrite("device_string", &SourceConfig::device_string)
      .def_readwrite("device_args", &SourceConfig::device_args)
      .def_readwrite("input_path", &SourceConfig::input_path)
      .def_readwrite("metadata_path", &SourceConfig::metadata_path)
      .def_readwrite("network_host", &SourceConfig::network_host)
      .def_readwrite("network_port", &SourceConfig::network_port)
      .def_readwrite("channel", &SourceConfig::channel)
      .def_readwrite("antenna", &SourceConfig::antenna)
      .def_readwrite("bandwidth_hz", &SourceConfig::bandwidth_hz)
      .def_readwrite("clock_source", &SourceConfig::clock_source)
      .def_readwrite("time_source", &SourceConfig::time_source)
      .def_readwrite("center_frequency_hz", &SourceConfig::center_frequency_hz)
      .def_readwrite("sample_rate_hz", &SourceConfig::sample_rate_hz)
      .def_readwrite("gain_db", &SourceConfig::gain_db)
      .def_readwrite("frame_samples", &SourceConfig::frame_samples)
      .def_readwrite("loop_playback", &SourceConfig::loop_playback)
      .def_readwrite("sample_format", &SourceConfig::sample_format);

  py::class_<ProcessingConfig>(module, "ProcessingConfig")
      .def(py::init<>())
      .def_readwrite("fft_size", &ProcessingConfig::fft_size)
      .def_readwrite("display_samples", &ProcessingConfig::display_samples)
      .def_readwrite("averaging_factor", &ProcessingConfig::averaging_factor)
      .def_readwrite("peak_hold_enabled", &ProcessingConfig::peak_hold_enabled)
      .def_readwrite("detection_threshold_db",
                     &ProcessingConfig::detection_threshold_db)
      .def_readwrite("max_peaks", &ProcessingConfig::max_peaks)
      .def_readwrite("minimum_peak_spacing_bins",
                     &ProcessingConfig::minimum_peak_spacing_bins)
      .def_readwrite("bandwidth_threshold_db",
                     &ProcessingConfig::bandwidth_threshold_db);

  py::class_<RecordingConfig>(module, "RecordingConfig")
      .def(py::init<>())
      .def_readwrite("format", &RecordingConfig::format)
      .def_readwrite("base_path", &RecordingConfig::base_path);

  py::class_<Marker>(module, "Marker")
      .def(py::init<>())
      .def_readwrite("center_frequency_hz", &Marker::center_frequency_hz)
      .def_readwrite("bandwidth_hz", &Marker::bandwidth_hz);

  py::class_<DetectionResult>(module, "DetectionResult")
      .def(py::init<>())
      .def_readonly("center_frequency_hz",
                    &DetectionResult::center_frequency_hz)
      .def_readonly("offset_hz", &DetectionResult::offset_hz)
      .def_readonly("peak_power_dbfs", &DetectionResult::peak_power_dbfs)
      .def_readonly("bandwidth_hz", &DetectionResult::bandwidth_hz)
      .def_readonly("labels", &DetectionResult::labels);

  py::class_<MarkerMeasurement>(module, "MarkerMeasurement")
      .def(py::init<>())
      .def_readonly("center_frequency_hz",
                    &MarkerMeasurement::center_frequency_hz)
      .def_readonly("bandwidth_hz", &MarkerMeasurement::bandwidth_hz)
      .def_readonly("peak_power_dbfs", &MarkerMeasurement::peak_power_dbfs)
      .def_readonly("average_power_dbfs",
                    &MarkerMeasurement::average_power_dbfs);

  py::class_<AnalysisReport>(module, "AnalysisReport")
      .def(py::init<>())
      .def_readonly("noise_floor_dbfs", &AnalysisReport::noise_floor_dbfs)
      .def_readonly("strongest_peak_dbfs", &AnalysisReport::strongest_peak_dbfs)
      .def_readonly("burst_score", &AnalysisReport::burst_score)
      .def_readonly("detections", &AnalysisReport::detections)
      .def_readonly("marker_measurements",
                    &AnalysisReport::marker_measurements);

  py::class_<SpectrumFrame>(module, "SpectrumFrame")
      .def(py::init<>())
      .def_readonly("center_frequency_hz", &SpectrumFrame::center_frequency_hz)
      .def_readonly("bin_resolution_hz", &SpectrumFrame::bin_resolution_hz)
      .def_readonly("power_dbfs", &SpectrumFrame::power_dbfs)
      .def_readonly("average_dbfs", &SpectrumFrame::average_dbfs)
      .def_readonly("peak_hold_dbfs", &SpectrumFrame::peak_hold_dbfs);

  py::class_<TimeDomainFrame>(module, "TimeDomainFrame")
      .def(py::init<>())
      .def_readonly("i", &TimeDomainFrame::i)
      .def_readonly("q", &TimeDomainFrame::q)
      .def_readonly("magnitude", &TimeDomainFrame::magnitude);

  py::class_<AnalyzerSnapshot>(module, "AnalyzerSnapshot")
      .def(py::init<>())
      .def_readonly("sequence", &AnalyzerSnapshot::sequence)
      .def_readonly("spectrum", &AnalyzerSnapshot::spectrum)
      .def_readonly("time_domain", &AnalyzerSnapshot::time_domain)
      .def_readonly("analysis", &AnalyzerSnapshot::analysis);

  py::class_<RecordingStatus>(module, "RecordingStatus")
      .def(py::init<>())
      .def_readonly("active", &RecordingStatus::active)
      .def_readonly("base_path", &RecordingStatus::base_path)
      .def_readonly("samples_written", &RecordingStatus::samples_written);

  py::class_<SessionStatus>(module, "SessionStatus")
      .def(py::init<>())
      .def_readonly("running", &SessionStatus::running)
      .def_readonly("source_description", &SessionStatus::source_description)
      .def_readonly("recording", &SessionStatus::recording);

  py::class_<DiagnosticEntry>(module, "DiagnosticEntry")
      .def(py::init<>())
      .def_readonly("session_id", &DiagnosticEntry::session_id)
      .def_readonly("timestamp_utc", &DiagnosticEntry::timestamp_utc)
      .def_readonly("level", &DiagnosticEntry::level)
      .def_readonly("component", &DiagnosticEntry::component)
      .def_readonly("code", &DiagnosticEntry::code)
      .def_readonly("message", &DiagnosticEntry::message)
      .def_readonly("source_kind", &DiagnosticEntry::source_kind)
      .def_readonly("source_description", &DiagnosticEntry::source_description);

  py::class_<AnalyzerSession>(module, "AnalyzerSession")
      .def(py::init<SourceConfig, ProcessingConfig>(),
           py::arg("source_config") = SourceConfig{},
           py::arg("processing_config") = ProcessingConfig{})
      .def("start", &AnalyzerSession::start)
      .def("stop", &AnalyzerSession::stop)
      .def("is_running", &AnalyzerSession::is_running)
      .def("status", &AnalyzerSession::status)
      .def("update_source_config", &AnalyzerSession::update_source_config)
      .def("update_processing_config",
           &AnalyzerSession::update_processing_config)
      .def("reset_peak_hold", &AnalyzerSession::reset_peak_hold)
      .def("start_recording", &AnalyzerSession::start_recording)
      .def("stop_recording", &AnalyzerSession::stop_recording)
      .def("poll_snapshot", &AnalyzerSession::poll_snapshot)
      .def("set_markers", &AnalyzerSession::set_markers)
      .def("source_config", &AnalyzerSession::source_config)
      .def("processing_config", &AnalyzerSession::processing_config)
      .def("diagnostics", &AnalyzerSession::diagnostics)
      .def("drain_diagnostics", &AnalyzerSession::drain_diagnostics)
      .def("last_error", &AnalyzerSession::last_error);
}

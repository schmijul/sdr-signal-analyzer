# Public API

The stable public surface is intentionally small:
- C++ headers under `include/sdr_analyzer/`
- Python names exported from `sdr_signal_analyzer.__all__`
- the installed executables documented in the README

Everything else should be treated as implementation detail unless documented otherwise.

## Core Types

The public C++ surface lives in `include/sdr_analyzer/`.

### `SourceConfig`

Controls where IQ samples come from and how a live source is configured.

Common fields:
- `kind`
- `center_frequency_hz`
- `sample_rate_hz`
- `gain_db`
- `frame_samples`

Replay-related fields:
- `input_path`
- `metadata_path`
- `loop_playback`
- `sample_format`

`rtl_tcp` fields:
- `network_host`
- `network_port`

UHD/Soapy-related fields:
- `device_string`
- `device_args`
- `channel`
- `antenna`
- `bandwidth_hz`
- `clock_source`
- `time_source`

Semantics:
- `device_args` is intended for UHD-style argument strings
- `device_string` is kept for generic device identifiers such as Soapy
- `bandwidth_hz == 0` means “do not force analog bandwidth”
- empty antenna/clock/time fields mean “leave backend default”

### `ProcessingConfig`

Controls DSP behavior:
- `fft_size`
- `display_samples`
- `averaging_factor`
- `peak_hold_enabled`
- `detection_threshold_db`
- `max_peaks`
- `minimum_peak_spacing_bins`
- `bandwidth_threshold_db`

Semantics:
- `fft_size` sets the FFT frame size and therefore the displayed bin resolution
- `display_samples` controls the time-domain preview only
- the current implementation uses a Hann window before the FFT
- detection thresholds are empirical heuristics intended for triage and UI filtering rather than calibrated classification

### `RecordingConfig`

Controls recording behavior:
- `format`
- `base_path`

Formats:
- `kRawBin`
- `kSigMF`

### `Marker`

User-defined region for power measurement:
- `center_frequency_hz`
- `bandwidth_hz`

Markers are evaluated against the current FFT snapshot, so the returned values are frame-local measurements, not long-running calibrated averages.

## Session Surface

### `AnalyzerSession`

Primary lifecycle object.

Important methods:
- `start()`
- `stop()`
- `is_running()`
- `status()`
- `update_source_config(...)`
- `update_processing_config(...)`
- `start_recording(...)`
- `stop_recording()`
- `poll_snapshot()`
- `set_markers(...)`
- `last_error()`

Usage pattern:

1. Construct with `SourceConfig` and `ProcessingConfig`
2. Call `start()`
3. Poll snapshots until enough data is available
4. Optionally start/stop recording
5. Call `stop()` when done

Lifecycle note:
- the session owns a worker thread while running
- control methods are intended for the caller thread and are synchronized internally
- `poll_snapshot()` returns bounded queued snapshots, so callers should drain it regularly
- `update_source_config(...)` updates the desired source settings immediately and the worker applies them on a later read cycle
- after EOF or a source failure, `last_error()` preserves the stop reason and `start()` may be called again to create a fresh worker

Minimal C++ example:

```cpp
#include <chrono>
#include <thread>

#include "sdr_analyzer/session.hpp"

int main() {
  sdr_analyzer::SourceConfig source;
  source.kind = sdr_analyzer::SourceKind::kSimulator;

  sdr_analyzer::ProcessingConfig processing;
  processing.fft_size = 2048;

  sdr_analyzer::AnalyzerSession session(source, processing);
  if (!session.start()) {
    return 1;
  }

  for (int attempt = 0; attempt < 50; ++attempt) {
    if (auto snapshot = session.poll_snapshot()) {
      (void)snapshot;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  session.stop();
  return 0;
}
```

## Result Types

### `AnalyzerSnapshot`

Container returned by `poll_snapshot()`:
- `sequence`
- `spectrum`
- `time_domain`
- `analysis`

### `SpectrumFrame`

- `center_frequency_hz`
- `bin_resolution_hz`
- `power_dbfs`
- `average_dbfs`
- `peak_hold_dbfs`

`power_dbfs` and the derived frames are relative to the full-scale complex input representation used by the backend. They are useful for comparing captures inside the tool, but they are not absolute RF power.

### `TimeDomainFrame`

- `i`
- `q`
- `magnitude`

### `AnalysisReport`

- `noise_floor_dbfs`
- `strongest_peak_dbfs`
- `burst_score`
- `detections`
- `marker_measurements`

`noise_floor_dbfs` and `burst_score` are heuristic analysis outputs. They support ranking and UI explanations, not formal calibration.

The DSP thresholds behind these values are intentionally empirical. They are documented in the implementation as heuristic cuts, not as universal signal-identification rules.

### `DetectionResult`

- `center_frequency_hz`
- `offset_hz`
- `peak_power_dbfs`
- `bandwidth_hz`
- `labels`

`labels` are descriptive heuristic labels such as `broadband`, `narrowband`, `likely FM`, and `burst-like`. They are not validated classifiers or protocol identifiers.

### `MarkerMeasurement`

- `center_frequency_hz`
- `bandwidth_hz`
- `peak_power_dbfs`
- `average_power_dbfs`

These values are frame-local measurements inside the current spectrum snapshot.

## Python Surface

The Python bindings mirror the C++ API closely:
- same source and processing config objects
- same source kinds
- same session lifecycle
- same snapshot/result objects

The Python GUI is intentionally built on those bindings instead of reimplementing backend behavior.

### `sdr_signal_analyzer.export`

The Python package also exposes a thin export helper layer above the public snapshot API:
- `JsonlMeasurementExporter`
- `build_metadata_record(...)`
- `build_frame_record(...)`
- `write_jsonl_record(...)`
- `export_jsonl(...)`

The helper writes the same JSONL schema used by the CLI export path:
- first record: metadata/header
- later records: one frame record per exported snapshot

Metadata records include:
- `format_version`
- `source_config`
- `processing_config`
- `export_started_at_utc`
- `marker_definitions`

Frame records include:
- `timestamp_utc`
- `elapsed_seconds`
- `sequence`
- `center_frequency_hz`
- `sample_rate_hz`
- `noise_floor_dbfs`
- `strongest_peak_dbfs`
- `burst_score`
- `detection_count`
- `detections`
- `marker_measurements`

Marker measurements in the export are frame-local values logged over time. The JSONL file is intended for analysis and reproducibility, not calibrated instrumentation.

## CLI Surface

The CLI now supports structured measurement export while polling snapshots:
- `--export-jsonl PATH`
- `--export-interval N`

Example:

```bash
./build/sdr-analyzer-cli \
  --source simulator \
  --frames 20 \
  --export-jsonl measurements.jsonl \
  --export-interval 2
```

The CLI writes one metadata record first and then one frame record for each exported snapshot.

## Public Versus Internal

Public:
- `AnalyzerSession`
- `SourceConfig`
- `ProcessingConfig`
- `RecordingConfig`
- `RecordingFormat`
- `SourceKind`
- the result and measurement types in `include/sdr_analyzer/results.hpp`

Internal:
- `src/**`
- GUI widgets and helper methods prefixed with `_`
- source factory details
- transport-specific implementation classes

This boundary matters because the release process should not promise stability for internals that can still change without notice.

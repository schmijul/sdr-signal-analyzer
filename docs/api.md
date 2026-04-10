# Public API

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

### `DetectionResult`

- `center_frequency_hz`
- `offset_hz`
- `peak_power_dbfs`
- `bandwidth_hz`
- `labels`

### `MarkerMeasurement`

- `center_frequency_hz`
- `bandwidth_hz`
- `peak_power_dbfs`
- `average_power_dbfs`

## Python Surface

The Python bindings mirror the C++ API closely:
- same source and processing config objects
- same source kinds
- same session lifecycle
- same snapshot/result objects

The Python GUI is intentionally built on those bindings instead of reimplementing backend behavior.

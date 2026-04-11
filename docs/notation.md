# Notation Registry

This page is the canonical registry for mathematical quantities used in the analyzer docs and code.

Use it together with [Code-Symbol Mapping](code-symbol-mapping.md) and [Terminology and Status Labels](terminology.md).

## Conventions

- Code identifiers are written in `snake_case`.
- Units are explicit when a quantity is meaningful.
- `dBFS` means decibels relative to full-scale complex samples.
- Heuristic quantities are documented as heuristics, not calibrated measurements.

## Capture Geometry

| Symbol | English name | Unit | Meaning | Canonical code identifier | Implementation path | Test path |
| --- | --- | --- | --- | --- | --- | --- |
| `f_c` | center frequency | Hz | RF center frequency for the current source and spectrum frame | `SourceConfig.center_frequency_hz`, `SpectrumFrame.center_frequency_hz`, `Marker.center_frequency_hz`, `DetectionResult.center_frequency_hz` | `include/sdr_analyzer/config.hpp`, `include/sdr_analyzer/results.hpp`, `src/core/session.cpp`, `src/dsp/analyzer.cpp` | `tests/test_session.cpp`, `tests/test_pipeline.cpp`, `tests/test_export_helper.py` |
| `f_s` | sample rate | Hz | Complex sample rate of the capture stream | `SourceConfig.sample_rate_hz` | `include/sdr_analyzer/config.hpp`, `src/core/session.cpp`, `src/sdr/simulator_source.cpp`, `src/sdr/rtltcp_source.cpp`, `src/sdr/uhd_source.cpp`, `src/sdr/soapy_source.cpp` | `tests/test_session.cpp`, `tests/test_record_replay.cpp`, `tests/test_export_helper.py` |
| `N_fft` | FFT size | samples | Number of complex samples transformed per analysis frame | `ProcessingConfig.fft_size` | `include/sdr_analyzer/config.hpp`, `src/dsp/analyzer.cpp`, `src/cli/main.cpp`, `python/sdr_signal_analyzer/gui.py` | `tests/test_pipeline.cpp`, `tests/test_session.cpp`, `tests/test_gui_validation.py` |

## Spectral Quantities

| Symbol | English name | Unit | Meaning | Canonical code identifier | Implementation path | Test path |
| --- | --- | --- | --- | --- | --- | --- |
| `Δf` | bin resolution | Hz/bin | Frequency spacing between adjacent FFT bins | `SpectrumFrame.bin_resolution_hz` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_export_helper.py` |
| `P_k` | bin power | dBFS/bin | Power at FFT bin `k` after windowing, FFT, and normalization | `SpectrumFrame.power_dbfs` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp` |
| `\bar{P}_k` | averaged bin power | dBFS/bin | Exponentially smoothed display power | `SpectrumFrame.average_dbfs` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp` |
| `H_k` | peak-hold bin power | dBFS/bin | Maximum retained per bin when peak hold is enabled | `SpectrumFrame.peak_hold_dbfs` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp` |

## Analysis Quantities

| Symbol | English name | Unit | Meaning | Canonical code identifier | Implementation path | Test path |
| --- | --- | --- | --- | --- | --- | --- |
| `P_floor` | noise floor | dBFS | Low-quantile estimate of the current frame noise floor | `AnalysisReport.noise_floor_dbfs` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp` |
| `P_peak` | strongest peak | dBFS | Maximum power found in the current frame | `AnalysisReport.strongest_peak_dbfs` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp` |
| `S_burst` | burst score | unitless | Ratio-based burst heuristic used only for coarse labeling | `AnalysisReport.burst_score` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp` |
| `T_det` | detection threshold offset | dB | Prominence threshold above the estimated floor for peak candidates | `ProcessingConfig.detection_threshold_db` | `include/sdr_analyzer/config.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp` |
| `T_bw` | bandwidth threshold offset | dB | Lower bound used with the roll-off rule to estimate occupied bandwidth | `ProcessingConfig.bandwidth_threshold_db` | `include/sdr_analyzer/config.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp` |

## Marker And Detection Quantities

| Symbol | English name | Unit | Meaning | Canonical code identifier | Implementation path | Test path |
| --- | --- | --- | --- | --- | --- | --- |
| `f_m` | marker center frequency | Hz | Center frequency of a user-defined marker region | `Marker.center_frequency_hz`, `MarkerMeasurement.center_frequency_hz` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_pipeline.cpp`, `tests/test_export_helper.py` |
| `B_m` | marker bandwidth | Hz | Width of a user-defined marker region | `Marker.bandwidth_hz`, `MarkerMeasurement.bandwidth_hz` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_pipeline.cpp`, `tests/test_export_helper.py` |
| `f_det` | detection center frequency | Hz | Estimated signal center frequency for one detection | `DetectionResult.center_frequency_hz` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp`, `src/cli/main.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp`, `tests/test_cli_export.py` |
| `B_det` | detection bandwidth | Hz | Estimated occupied bandwidth for one detection | `DetectionResult.bandwidth_hz` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp`, `tests/test_export_helper.py` |
| `P_det` | detection peak power | dBFS | Peak power at the detected bin | `DetectionResult.peak_power_dbfs` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp` |
| `L_det` | detection labels | unitless | Descriptive heuristic labels applied to a detection | `DetectionResult.labels` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp`, `src/cli/main.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp`, `tests/test_cli_export.py` |

## Export And Session Surfaces

| Symbol | English name | Unit | Meaning | Canonical code identifier | Implementation path | Test path |
| --- | --- | --- | --- | --- | --- | --- |
| `n` | frame sequence | count | Monotonic snapshot counter within a session | `AnalyzerSnapshot.sequence` | `include/sdr_analyzer/results.hpp`, `src/core/session.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_session.cpp`, `tests/test_cli_export.py` |
| `P_marker,peak` | marker peak power | dBFS | Peak power within a marker region in one frame | `MarkerMeasurement.peak_power_dbfs` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_pipeline.cpp`, `tests/test_export_helper.py` |
| `P_marker,avg` | marker average power | dBFS | Mean power within a marker region in one frame | `MarkerMeasurement.average_power_dbfs` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_pipeline.cpp`, `tests/test_export_helper.py` |

## Reading Rule

If a prose statement refers to one of these quantities, use the same symbol, code identifier, and unit consistently in related docs and tests.

For any new quantity, add it here first, then update the code-symbol mapping page and any method pages that depend on it.

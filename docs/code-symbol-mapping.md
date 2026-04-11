# Code-Symbol Mapping

This page is the engineering crosswalk from canonical code identifiers back to the mathematical symbols defined in [Notation Registry](notation.md).

## Capture And Geometry Fields

| Canonical code identifier | Symbol | English name | Unit | Semantics | Implementation path | Test path |
| --- | --- | --- | --- | --- | --- | --- |
| `SourceConfig.center_frequency_hz` | `f_c` | center frequency | Hz | Requested capture center frequency for a source | `include/sdr_analyzer/config.hpp`, `src/core/session.cpp`, `src/sdr/rtltcp_source.cpp`, `src/sdr/uhd_source.cpp`, `src/sdr/soapy_source.cpp` | `tests/test_session.cpp`, `tests/test_record_replay.cpp` |
| `SourceConfig.sample_rate_hz` | `f_s` | sample rate | Hz | Requested complex sample rate for a source | `include/sdr_analyzer/config.hpp`, `src/core/session.cpp`, `src/sdr/simulator_source.cpp`, `src/sdr/rtltcp_source.cpp`, `src/sdr/uhd_source.cpp`, `src/sdr/soapy_source.cpp` | `tests/test_session.cpp`, `tests/test_record_replay.cpp`, `tests/test_export_helper.py` |
| `ProcessingConfig.fft_size` | `N_fft` | FFT size | samples | Number of complex samples transformed per frame | `include/sdr_analyzer/config.hpp`, `src/dsp/analyzer.cpp`, `src/cli/main.cpp`, `python/sdr_signal_analyzer/gui.py` | `tests/test_pipeline.cpp`, `tests/test_session.cpp`, `tests/test_gui_validation.py` |

## Derived Spectrum Fields

| Canonical code identifier | Symbol | English name | Unit | Semantics | Implementation path | Test path |
| --- | --- | --- | --- | --- | --- | --- |
| `SpectrumFrame.bin_resolution_hz` | `Δf` | bin resolution | Hz/bin | Frequency spacing between adjacent bins, computed from `f_s / N_fft` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_export_helper.py` |
| `SpectrumFrame.power_dbfs` | `P_k` | bin power | dBFS/bin | Current-frame power spectrum after windowing and FFT normalization | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp` |
| `SpectrumFrame.average_dbfs` | `\bar{P}_k` | averaged bin power | dBFS/bin | Exponentially smoothed display spectrum | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp` |
| `SpectrumFrame.peak_hold_dbfs` | `H_k` | peak-hold bin power | dBFS/bin | Maximum retained per bin while peak hold is enabled | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp` |

## Analysis Outputs

| Canonical code identifier | Symbol | English name | Unit | Semantics | Implementation path | Test path |
| --- | --- | --- | --- | --- | --- | --- |
| `AnalysisReport.noise_floor_dbfs` | `P_floor` | noise floor | dBFS | Low-quantile estimate used as the baseline for heuristics | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp` |
| `AnalysisReport.strongest_peak_dbfs` | `P_peak` | strongest peak | dBFS | Maximum bin power in the current frame | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp` |
| `AnalysisReport.burst_score` | `S_burst` | burst score | unitless | Ratio heuristic used to label short-duty-cycle activity | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp` |
| `ProcessingConfig.detection_threshold_db` | `T_det` | detection threshold offset | dB | Prominence cutoff for candidate peaks | `include/sdr_analyzer/config.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp` |
| `ProcessingConfig.bandwidth_threshold_db` | `T_bw` | bandwidth threshold offset | dB | Lower bound used with the roll-off rule for occupied bandwidth | `include/sdr_analyzer/config.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp` |

## Marker And Detection Outputs

| Canonical code identifier | Symbol | English name | Unit | Semantics | Implementation path | Test path |
| --- | --- | --- | --- | --- | --- | --- |
| `Marker.center_frequency_hz` | `f_m` | marker center frequency | Hz | User-defined center of a measurement region | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_pipeline.cpp`, `tests/test_export_helper.py` |
| `Marker.bandwidth_hz` | `B_m` | marker bandwidth | Hz | Width of a measurement region | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_pipeline.cpp`, `tests/test_export_helper.py` |
| `DetectionResult.center_frequency_hz` | `f_det` | detection center frequency | Hz | Estimated location of a detected signal peak | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp`, `src/cli/main.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp`, `tests/test_cli_export.py` |
| `DetectionResult.offset_hz` | `f_offset` | detection offset | Hz | Difference between `f_det` and `f_c` | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp` |
| `DetectionResult.bandwidth_hz` | `B_det` | detection bandwidth | Hz | Estimated occupied bandwidth for one detection | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp`, `tests/test_export_helper.py` |
| `DetectionResult.peak_power_dbfs` | `P_det` | detection peak power | dBFS | Peak power at the detection bin | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp` |
| `DetectionResult.labels` | `L_det` | detection labels | unitless | Descriptive heuristic labels applied to a detection | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp`, `src/cli/main.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_pipeline.cpp`, `tests/test_record_replay.cpp`, `tests/test_cli_export.py` |

## Export And Session Surfaces

| Canonical code identifier | Symbol | English name | Unit | Semantics | Implementation path | Test path |
| --- | --- | --- | --- | --- | --- | --- |
| `AnalyzerSnapshot.sequence` | `n` | frame sequence | count | Monotonic snapshot counter within a session | `include/sdr_analyzer/results.hpp`, `src/core/session.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_session.cpp`, `tests/test_cli_export.py` |
| `MarkerMeasurement.peak_power_dbfs` | `P_marker,peak` | marker peak power | dBFS | Peak power within a marker region in one frame | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_pipeline.cpp`, `tests/test_export_helper.py` |
| `MarkerMeasurement.average_power_dbfs` | `P_marker,avg` | marker average power | dBFS | Mean power within a marker region in one frame | `include/sdr_analyzer/results.hpp`, `src/dsp/analyzer.cpp`, `python/sdr_signal_analyzer/export.py` | `tests/test_pipeline.cpp`, `tests/test_export_helper.py` |

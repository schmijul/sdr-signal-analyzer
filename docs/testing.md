# Testing And Validation

## Automated Coverage

The test suite is split into critical and non-critical paths so the release bar stays honest.

### Critical Paths

These paths are expected to stay stable and are covered by automated tests:

`tests/test_pipeline.cpp`
- tone detection
- frequency accuracy across multiple offsets
- narrowband bandwidth sanity
- peak-hold behavior after a strong transient
- burst-like heuristic labeling
- marker measurement behavior

`tests/test_session.cpp`
- simulator start/stop smoke
- non-blocking source config updates while running
- replay EOF shutdown and session restart behavior

`tests/test_rtltcp.cpp`
- mock `rtl_tcp` server
- source startup
- command exchange
- snapshot production

`tests/test_record_replay.cpp`
- raw record -> replay consistency
- SigMF record -> replay consistency
- committed fixture replay
- missing metadata failure paths
- malformed replay data handling

`tests/test_gui_validation.py`
- invalid center frequency input
- invalid FFT size input
- missing replay path validation
- invalid marker update validation

`tests/test_diagnostics.py`
- structured diagnostics for failed starts
- diagnostics buffer drain semantics

`tests/test_cli_export.py`
- JSONL export schema and interval behavior
- CLI diagnostic log file creation on failure

When `PySide6` is missing, the GUI validation test is skipped explicitly instead of failing the whole suite.

### Non-Critical Paths

These paths matter for usability, but they are not treated as hard correctness gates:

- Qt painting and pixel-perfect rendering
- live hardware backends beyond startup and basic transport validation
- benchmark numbers
- screenshot aesthetics

## Local Commands

Build and run all tests:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Python GUI smoke:

```bash
QT_QPA_PLATFORM=offscreen PYTHONPATH=python python - <<'PY'
from PySide6 import QtWidgets
from sdr_signal_analyzer.gui import MainWindow
app = QtWidgets.QApplication([])
window = MainWindow()
window._toggle_stream()
for _ in range(10):
    app.processEvents()
assert window.grab().save("/tmp/sdr_gui_smoke.png")
window._session.stop()
PY
```

Python coverage and benchmark helpers:

```bash
python3 scripts/python_coverage.py --output-dir quality-artifacts/python-coverage
python3 scripts/benchmark_ci.py --cli ./build/sdr-analyzer-cli
python3 scripts/release_validate.py --skip-install-smoke
```

Expected outputs:

- `quality-artifacts/python-coverage/html/index.html`
- `quality-artifacts/python-coverage/coverage.xml`
- `quality-artifacts/benchmark/benchmark.json`
- `quality-artifacts/benchmark/benchmark.md`
- `dist/*` plus an installed-wheel smoke if `python3 scripts/release_validate.py --skip-install-smoke` is run

Diagnostics smoke:

```bash
./build/sdr-analyzer-cli \
  --source replay \
  --input tests/fixtures/tone_cf32.sigmf-data \
  --meta tests/fixtures/tone_cf32.sigmf-meta \
  --frames 4 \
  --log-level debug \
  --log-json \
  --log-file quality-artifacts/diagnostics/replay.jsonl
```

Expected outputs:
- `quality-artifacts/diagnostics/replay.jsonl`
- one JSON diagnostic record per runtime event

## CI Coverage

The `quality.yml` workflow is intended to publish two kinds of evidence:

- C++ coverage from `gcovr`
- Python coverage from `coverage.py`

The workflow uploads HTML and XML artifacts so regressions are inspectable without reproducing the run locally.

CI is intentionally hardware-free.

## Benchmark Output

Coverage and benchmark reporting are informational. They publish evidence and artifacts, but they are not merge-gating performance thresholds.

Benchmark runs are informational and should be compared only across the same runner class and build mode. See [Benchmarking](benchmarking.md) for the recorded fields and local invocation.

## Hardware Validation

Automated tests do not replace attached-device validation.

Prepared now:
- [Hardware Validation Plan](hardware_validation_plan.md)
- [Hardware Validation Status](hardware_validation_status.md)
- [UHD / USRP Source Guide](sources/uhd.md)
- [SoapySDR Source Guide](sources/soapy.md)
- [Hardware Validation Report Template](hardware_validation_report_template.md)

Current truth:
- UHD is `Prepared for validation` and `Pending lab validation`
- SoapySDR is `Prepared for validation` and `Pending lab validation`
- CI remains intentionally hardware-free

For `rtl_tcp`, the code path is verified through a mock server. Real server and RF behavior still depend on the live environment and should be documented with diagnostics logs and exact commands.

## Test Philosophy

The project prefers:

- deterministic simulator and replay coverage in CI
- hardware checklists for real radio validation
- fixtures for repeatability
- heuristic outputs that are checked against controlled expectations, not treated as ground truth
- explicit separation between diagnostics logs and measurement export

This keeps the repo portable while still supporting serious hardware-oriented workflows.

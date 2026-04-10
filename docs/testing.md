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
```

Expected outputs:

- `quality-artifacts/python-coverage/html/index.html`
- `quality-artifacts/python-coverage/coverage.xml`
- `quality-artifacts/benchmark/benchmark.json`
- `quality-artifacts/benchmark/benchmark.md`

## CI Coverage

The `quality.yml` workflow is intended to publish two kinds of evidence:

- C++ coverage from `gcovr`
- Python coverage from `coverage.py`

The workflow uploads HTML and XML artifacts so regressions are inspectable without reproducing the run locally.

CI is intentionally hardware-free.

## Benchmark Output

Benchmark runs are informational and should be compared only across the same runner class and build mode. See [Benchmarking](benchmarking.md) for the recorded fields and local invocation.

## Manual Hardware Validation

Automated tests do not replace real-device validation. For a serious release candidate, run:

### UHD / USRP

1. Start a UHD session with explicit device args.
2. Tune center frequency, sample rate, gain, bandwidth, and antenna.
3. Confirm the GUI updates in real time.
4. Record a capture and replay it.

### rtl_tcp

1. Connect to a live server.
2. Confirm tuning and stream stability.
3. Force a disconnect and check error handling.

### Replay

1. Capture from a live source.
2. Replay that capture.
3. Compare detections and power levels against the live session.

## Test Philosophy

The project prefers:

- deterministic simulator and replay coverage in CI
- hardware checklists for real radio validation
- fixtures for repeatability
- heuristic outputs that are checked against controlled expectations, not treated as ground truth

This keeps the repo portable while still supporting serious hardware-oriented workflows.

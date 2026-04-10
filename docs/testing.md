# Testing And Validation

## Automated Coverage

The current automated suite covers five layers:

### DSP Regression

`tests/test_pipeline.cpp`
- tone detection
- frequency accuracy across multiple offsets
- narrowband bandwidth sanity
- peak-hold behavior after a strong transient
- burst-like heuristic labeling
- marker measurement behavior

### Session Lifecycle Regression

`tests/test_session.cpp`
- simulator start/stop smoke
- non-blocking source config updates while running
- replay EOF shutdown and session restart behavior

### Source / Transport Regression

`tests/test_rtltcp.cpp`
- mock `rtl_tcp` server
- source startup
- command exchange
- snapshot production

### Recording / Replay Regression

`tests/test_record_replay.cpp`
- raw record -> replay consistency
- SigMF record -> replay consistency
- committed fixture replay
- missing metadata failure paths
- malformed replay data handling

### GUI Validation Regression

`tests/test_gui_validation.py`
- invalid center frequency input
- invalid FFT size input
- missing replay path validation
- invalid marker update validation

When `PySide6` is missing, the GUI validation test is skipped explicitly instead of failing the whole suite.

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

## CI Coverage

GitHub Actions currently verifies:
- configure/build on Ubuntu
- no-hardware build path with UHD and Soapy disabled explicitly
- full CTest run
- replay CLI EOF behavior
- Python binding smoke
- offscreen GUI smoke

CI is intentionally hardware-free.

## Lightweight Performance Check

For a quick local throughput sanity check, measure the simulator path rather than treating CI as a benchmark:

```bash
time ./build/sdr-analyzer-cli --source simulator --frames 200 >/tmp/sdr_cli_bench.txt
```

This is not a published performance guarantee. It is a local regression check for obviously degraded frame throughput or shutdown behavior.

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

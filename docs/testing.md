# Testing And Validation

## Automated Coverage

The current automated suite covers three layers:

### DSP Regression

`tests/test_pipeline.cpp`
- tone detection
- burst-like classification
- marker measurement behavior

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
- Python binding smoke
- offscreen GUI smoke

CI is intentionally hardware-free.

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

This keeps the repo portable while still supporting serious hardware-oriented workflows.

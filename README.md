# SDR Signal Analyzer

C++-first SDR spectrum analyzer with a Python GUI on top. The backend owns device access, DSP, detection, recording, replay, and analysis so it can run headless through a CLI or be embedded into other frontends without dragging Python into the core.

## Architecture

- `include/sdr_analyzer/`: public C++ API
- `src/core/`: session orchestration and streaming lifecycle
- `src/sdr/`: live device and replay sources
- `src/dsp/`: FFT, averaging, peak hold, peak detection, bandwidth/noise estimation, coarse classification
- `src/io/`: `.bin` IQ recording, SigMF metadata, replay helpers
- `src/cli/`: backend-only CLI
- `python/sdr_signal_analyzer/`: `pybind11` bindings and optional PySide6/PyQtGraph GUI
- `examples/`: simulator-driven usage examples

## MVP Features

- Live spectrum view data from the backend
- Waterfall-ready frame stream
- Time-domain I/Q and magnitude samples
- Frequency, sample-rate, gain, FFT-size, averaging, and peak-hold controls
- Band markers and dBFS-oriented frame data
- Automatic peak detection
- Bandwidth estimation
- Noise-floor estimation
- Coarse signal classification:
  - `likely FM`
  - `narrowband`
  - `broadband`
  - `burst-like`
- IQ recording and replay:
  - raw `.bin` + metadata sidecar
  - SigMF `.sigmf-data` + `.sigmf-meta`

## Build

### C++ backend and CLI

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build
```

### Python bindings and GUI

If `pybind11` is available, the CMake build emits the extension into `python/sdr_signal_analyzer/`.

```bash
python3 -m pip install -e .[gui]
python3 -m sdr_signal_analyzer
```

The GUI dependencies are optional. The backend and CLI still build without `PySide6` or `pyqtgraph`.

## CLI Examples

Run the simulator backend:

```bash
./build/sdr-analyzer-cli --source simulator --frames 20
```

Record a replayable capture:

```bash
./build/sdr-analyzer-cli \
  --source simulator \
  --frames 100 \
  --record-base captures/demo \
  --record-format sigmf
```

Replay a raw IQ dump:

```bash
./build/sdr-analyzer-cli \
  --source replay \
  --input captures/demo.bin \
  --meta captures/demo.bin.json \
  --loop
```

## RF Portfolio Targets

This repo is meant to present RF understanding, not only plotting:

- FM broadcast case study near 100 MHz
- 433 MHz ISM activity capture and replay
- narrowband signal example with estimated bandwidth

Once real captures are added, the README should include screenshots and reproducible replays for each case so the analysis output can be verified from stored IQ data.

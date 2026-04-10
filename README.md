# SDR Signal Analyzer

C++-first SDR spectrum analyzer with a Python GUI on top. The backend owns capture, FFT processing, detection, recording, replay, and analysis so it can run headless through a CLI or be embedded into other frontends without pulling Python into the core runtime.

The current repo state is a publishable MVP:
- real-time spectrum, waterfall, and time-domain views
- automatic peak detection, bandwidth estimation, noise-floor estimation, and coarse signal labels
- IQ recording and deterministic replay
- built-in simulator, self-implemented `rtl_tcp` client, and optional SoapySDR support
- self-rendered Qt GUI using only PySide6 on the frontend side

## Screenshots

Simulator-driven portfolio scenes generated from the checked-in codepath:

![Overview UI](docs/screenshots/overview.png)

![433 MHz style scene](docs/screenshots/ism_433.png)

![Narrowband focus](docs/screenshots/narrowband_focus.png)

You can regenerate these assets with [generate_portfolio_assets.py](scripts/generate_portfolio_assets.py) in an offscreen Qt session.

## Architecture

- `include/sdr_analyzer/`: public C++ API
- `src/core/`: session lifecycle and dataflow orchestration
- `src/sdr/`: simulator, replay, `rtl_tcp`, and optional SoapySDR sources
- `src/dsp/`: FFT, averaging, peak hold, peak detection, bandwidth/noise estimation, classification
- `src/io/`: raw `.bin` and SigMF metadata handling, recording, replay helpers
- `src/cli/`: backend-only CLI for capture/replay/analysis
- `python/sdr_signal_analyzer/`: `pybind11` bindings and PySide6 GUI
- `tests/`: DSP, `rtl_tcp`, and record/replay regression coverage

## What Is Implemented

- Live spectrum frame generation with dBFS scaling
- Waterfall and time-domain rendering
- Controls for center frequency, sample rate, gain, FFT size, averaging, and peak hold
- Marker measurements with peak and average dBFS inside the marked span
- Automatic peak detection
- Occupied-bandwidth estimation
- Robust noise-floor estimation
- Coarse heuristic labels:
  - `likely FM`
  - `narrowband`
  - `broadband`
  - `burst-like`
- IQ recording and replay:
  - raw `.bin` + JSON metadata sidecar
  - SigMF `.sigmf-data` + `.sigmf-meta`
- Live source options:
  - built-in simulator
  - self-implemented `rtl_tcp` network backend
  - optional SoapySDR backend when available at build time

## Quickstart

### Backend build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### Python GUI

Use a virtual environment on Linux systems with externally managed Python:

```bash
python3 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install PySide6 pybind11 scikit-build-core
cmake -S . -B build
cmake --build build
PYTHONPATH=python python -m sdr_signal_analyzer
```

### Fastest demo path

```bash
./build/sdr-analyzer-cli --source simulator --frames 20
```

## CLI Examples

Run the simulator backend:

```bash
./build/sdr-analyzer-cli --source simulator --frames 20
```

Connect to an `rtl_tcp` server without SoapySDR:

```bash
./build/sdr-analyzer-cli \
  --source rtl_tcp \
  --host 127.0.0.1 \
  --port 1234 \
  --center-hz 100000000 \
  --sample-rate 2400000 \
  --gain-db 20
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

## Publish Notes

This repo is careful about what it claims:
- the screenshots above are deterministic simulator scenes, not over-the-air recordings
- the DSP and transport path are implemented in this repo rather than delegated to NumPy, SciPy, or plotting libraries
- SoapySDR is optional convenience for broader hardware support, not a hard dependency for live input

What would still strengthen it further later:
- real RF capture case studies with committed replay samples
- CSV/PNG export workflows
- scanning and demod preview features

## Continuous Integration

GitHub Actions builds the C++ targets, runs all current tests, and performs a Python binding smoke check on Ubuntu through [.github/workflows/ci.yml](.github/workflows/ci.yml).

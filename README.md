# SDR Signal Analyzer

[![CI](https://github.com/schmijul/sdr-signal-analyzer/actions/workflows/ci.yml/badge.svg)](https://github.com/schmijul/sdr-signal-analyzer/actions/workflows/ci.yml)
[![Docs](https://github.com/schmijul/sdr-signal-analyzer/actions/workflows/docs.yml/badge.svg)](https://github.com/schmijul/sdr-signal-analyzer/actions/workflows/docs.yml)
[![PyPI](https://img.shields.io/pypi/v/sdr-signal-analyzer)](https://pypi.org/project/sdr-signal-analyzer/)
[![Python Versions](https://img.shields.io/pypi/pyversions/sdr-signal-analyzer)](https://pypi.org/project/sdr-signal-analyzer/)

C++-first SDR spectrum analyzer with a Python GUI, deterministic replay tooling, and optional live-device integrations. The backend owns capture, FFT processing, detection, recording, replay, and analysis so it can run headless from a CLI, power a Qt desktop UI, or be embedded into other frontends without pulling Python into the core runtime.

![Overview screenshot](https://raw.githubusercontent.com/schmijul/sdr-signal-analyzer/main/docs/screenshots/overview.png)

## Why This Project

`sdr-signal-analyzer` is built around one principle: the radio and DSP path should stay reusable even when the UI changes.

That gives you:
- real-time spectrum, waterfall, and time-domain views
- automatic peak detection, bandwidth estimation, noise-floor estimation, and coarse labels
- raw `.bin` and SigMF recording plus deterministic replay
- a built-in simulator for screenshots, demos, and regression tests
- a built-in `rtl_tcp` client for RTL-SDR style remote streams
- optional native UHD/USRP and SoapySDR backends when those SDKs are available
- backend regression tests plus Python and GUI smoke coverage in CI

## Hardware And Backend Support

| Backend | Status | Extra runtime requirement | Typical use |
| --- | --- | --- | --- |
| `simulator` | Built in | None | Fastest demo path, screenshots, CI, deterministic local testing |
| `replay` | Built in | Recorded `.bin` + JSON or SigMF files | Offline analysis, regression testing, reproducible docs |
| `rtl_tcp` | Built in | Reachable `rtl_tcp` server | Remote RTL-SDR style streaming without SoapySDR |
| `uhd` | Optional build-time backend | UHD headers, libraries, and a supported USRP | Native USRP RX streaming |
| `soapy` | Optional build-time backend | SoapySDR headers, libraries, and a supported device driver | Generic live SDR device support |

Optional hardware SDKs are detected during a direct CMake build. The repository still builds and tests cleanly without them.

## Installation

### Python package from PyPI

Install the GUI-capable package:

```bash
python -m pip install "sdr-signal-analyzer[gui]"
```

Notes:
- if a wheel is available for your platform, this is the fastest install path
- if `pip` falls back to a source build, you need a working C++20 toolchain, CMake, and Python development headers
- PySide6 is only needed when you want the Qt GUI

Minimal package install without GUI extras:

```bash
python -m pip install sdr-signal-analyzer
```

### Build from source

Baseline requirements:
- CMake 3.24 or newer
- C++20-capable compiler
- Python 3.10 or newer
- `pybind11` and `scikit-build-core` if you want to build the Python package locally
- PySide6 if you want to run the Qt GUI

Optional hardware requirements:
- install UHD development packages before configuring if you want native USRP support
- install SoapySDR development packages before configuring if you want the generic Soapy backend

Local developer setup:

```bash
python3 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -e ".[dev,gui]"
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Quickstart

### 1. Fastest demo path: simulator CLI

```bash
cmake -S . -B build
cmake --build build
./build/sdr-analyzer-cli --source simulator --frames 20
```

### 2. Launch the Qt GUI

From an installed package:

```bash
sdr-signal-analyzer-gui
```

From a source checkout after building the extension:

```bash
PYTHONPATH=python python -m sdr_signal_analyzer
```

### 3. Replay a committed fixture

```bash
./build/sdr-analyzer-cli \
  --source replay \
  --input tests/fixtures/tone_cf32.sigmf-data \
  --meta tests/fixtures/tone_cf32.sigmf-meta \
  --frames 4
```

### 4. Connect to a live `rtl_tcp` server

```bash
./build/sdr-analyzer-cli \
  --source rtl_tcp \
  --host 127.0.0.1 \
  --port 1234 \
  --center-hz 100000000 \
  --sample-rate 2400000 \
  --gain-db 20
```

### 5. Connect to a USRP through UHD

```bash
./build/sdr-analyzer-cli \
  --source uhd \
  --device-args "type=b200" \
  --channel 0 \
  --antenna RX2 \
  --bandwidth-hz 2000000 \
  --center-hz 100000000 \
  --sample-rate 2000000 \
  --gain-db 25
```

## Python Example

The repository ships a small Python example that drives the simulator and prints detections:

```bash
python examples/run_simulator.py
```

If you installed the project into a virtual environment first, the same example becomes a quick smoke test for the Python bindings.

## Packaging And Releases

Package artifacts are built from `pyproject.toml` via `scikit-build-core`.

Local release validation:

```bash
python -m pip install build twine
pyproject-build
twine check dist/*
```

`pyproject-build` is used instead of `python -m build` because this repository has a top-level `build/` directory for CMake output.

Releases are intended to follow:
- changelog-driven notes in [CHANGELOG.md](https://github.com/schmijul/sdr-signal-analyzer/blob/main/CHANGELOG.md)
- a tagged GitHub release such as `v0.1.0`
- PyPI publishing from GitHub Actions through Trusted Publishing

## Documentation

- [Documentation home](https://schmijul.github.io/sdr-signal-analyzer/)
- [Architecture](https://github.com/schmijul/sdr-signal-analyzer/blob/main/docs/architecture.md)
- [Public API](https://github.com/schmijul/sdr-signal-analyzer/blob/main/docs/api.md)
- [Replay and recording](https://github.com/schmijul/sdr-signal-analyzer/blob/main/docs/replay-and-recording.md)
- [Source guides](https://github.com/schmijul/sdr-signal-analyzer/blob/main/docs/sources/index.md)
- [Testing and validation](https://github.com/schmijul/sdr-signal-analyzer/blob/main/docs/testing.md)
- [Case studies and screenshots](https://github.com/schmijul/sdr-signal-analyzer/blob/main/docs/case-studies.md)

Additional screenshots:
- [433 MHz style scene](https://raw.githubusercontent.com/schmijul/sdr-signal-analyzer/main/docs/screenshots/ism_433.png)
- [Narrowband focus](https://raw.githubusercontent.com/schmijul/sdr-signal-analyzer/main/docs/screenshots/narrowband_focus.png)

## Development

Useful local commands:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen PYTHONPATH=python python -m sdr_signal_analyzer
pre-commit run --all-files
```

Contributor guidance lives in [CONTRIBUTING.md](https://github.com/schmijul/sdr-signal-analyzer/blob/main/CONTRIBUTING.md).

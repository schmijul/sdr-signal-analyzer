# SDR Signal Analyzer

[![CI](https://github.com/schmijul/sdr-signal-analyzer/actions/workflows/ci.yml/badge.svg)](https://github.com/schmijul/sdr-signal-analyzer/actions/workflows/ci.yml)
[![Docs](https://github.com/schmijul/sdr-signal-analyzer/actions/workflows/docs.yml/badge.svg)](https://github.com/schmijul/sdr-signal-analyzer/actions/workflows/docs.yml)
[![Status](https://img.shields.io/badge/status-alpha-orange)](https://github.com/schmijul/sdr-signal-analyzer)
[![Python](https://img.shields.io/badge/python-3.10--3.12-blue)](https://github.com/schmijul/sdr-signal-analyzer/blob/main/pyproject.toml)

## What This Is

`sdr-signal-analyzer` is a C++-first SDR spectrum analyzer with a thin Python GUI, deterministic replay support, and optional live-device backends. It captures IQ samples, runs FFT-based analysis, estimates noise and occupied bandwidth, and presents the results through a CLI, Qt GUI, or embedded session API.

It is a reusable analysis backend, not a demodulator, protocol decoder, or measurement-grade instrument.

## Who It Is For

Use this project if you want:
- a deterministic simulator and replay path for development and regression tests
- a backend you can embed in C++ or drive from Python
- a GUI and CLI that share the same processing pipeline
- a lightweight way to inspect spectrum activity without coupling the core to the frontend

It is a good fit for developers, radio hobbyists, and tooling authors who need reproducible signal analysis workflows. It is not aimed at calibrated lab measurements or automatic modulation classification.

## What Works Today

The current stable workflow is:
1. `simulator`
2. `replay`
3. `rtl_tcp`
4. advanced backends such as UHD and SoapySDR

Verified today:
- simulator analysis, CLI smoke, and Python example smoke
- replay of committed deterministic fixtures
- `rtl_tcp` source coverage with a mock server
- Qt GUI startup smoke in offscreen mode
- native C++ regression tests for DSP, session lifecycle, and recording/replay

## What Is Experimental

The following are available but should still be treated as experimental or hardware-dependent:
- UHD live streaming
- SoapySDR live streaming
- heuristic detection labels such as `narrowband`, `burst-like`, and `likely FM`
- performance characteristics on live SDR hardware

The analyzer reports useful hints, not guaranteed classifications. Detection accuracy is heuristic and should not be treated as a promise.

## Quickstart

### 1. Install

```bash
python3 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -e ".[gui]"
```

Expected result:
- the editable install succeeds
- the Python bindings import successfully
- the Qt GUI dependencies are available in the venv

### 2. Run the one-command demo

```bash
sdr-signal-analyzer-demo
```

Expected result:
- the simulator starts without hardware
- detections print to stdout
- the command exits successfully after a short run

### 3. Run the simulator CLI

```bash
cmake -S . -B build
cmake --build build
./build/sdr-analyzer-cli --source simulator --frames 20
```

Expected result:
- frames print with `noise=` and `detections=`
- no live SDR hardware is required

### 4. Replay a committed fixture

```bash
./build/sdr-analyzer-cli \
  --source replay \
  --input tests/fixtures/tone_cf32.sigmf-data \
  --meta tests/fixtures/tone_cf32.sigmf-meta \
  --frames 4
```

Expected result:
- the session produces repeatable detections near `100.15 MHz`
- replay exits cleanly at end of stream

### 5. Connect to `rtl_tcp`

```bash
./build/sdr-analyzer-cli \
  --source rtl_tcp \
  --host 127.0.0.1 \
  --port 1234 \
  --center-hz 100000000 \
  --sample-rate 2400000 \
  --gain-db 20
```

Expected result:
- the analyzer connects to the remote server
- spectral movement follows the incoming stream
- disconnects surface as actionable source errors

## Expected Outputs

Typical CLI output includes:
- frame number
- estimated noise floor in `dBFS`
- strongest peak estimate in `dBFS`
- number of detections
- per-detection center frequency, bandwidth, and heuristic label

Typical GUI output includes:
- spectrum
- waterfall
- time-domain view
- detection table
- marker measurements

## Validation Matrices

### Platform Validation

Only verified combinations are listed here.

| OS | Python version | GUI working | Backend working |
| --- | --- | --- | --- |
| Ubuntu 24.04 | 3.12 | Yes | Yes |

Evidence:
- local editable install in a fresh venv using `python3`
- simulator demo and Python smoke ran successfully
- C++ test suite passed in the current build tree

### Backend Validation

Only combinations with direct evidence are marked verified.

| Backend | Tested hardware | Sample rate range | Known limitations | Stability |
| --- | --- | --- | --- | --- |
| `simulator` | None | Verified at 2.4 Msps in the demo/smoke path | Synthetic source only | Stable |
| `replay` | None | Matches the committed 2.4 Msps fixture metadata | Depends on correct metadata and file integrity | Stable |
| `rtl_tcp` | Mock `rtl_tcp` server in CI | Verified at 2.4 Msps in the mock-server test | Network timing and server behavior matter | Stable for code path, hardware-dependent in practice |
| `uhd` | Not yet release-validated on attached hardware | Not yet documented from verified runs | Requires UHD SDK and supported device | Experimental |
| `soapy` | Not yet release-validated on attached hardware | Not yet documented from verified runs | Requires SoapySDR SDK and a working driver stack | Experimental |

## Comparison Positioning

Use this project when you want:
- replayable captures that can be used as deterministic tests
- an embeddable backend instead of a GUI-only tool
- a clear simulator-first workflow for demos and validation

Compared with a pure live-viewer, this repository puts more emphasis on reproducibility and testability. Compared with a demodulation tool, it intentionally stops at spectrum analysis and heuristic labeling.

## Documentation

- [Documentation home](https://schmijul.github.io/sdr-signal-analyzer/)
- [Architecture](docs/architecture.md)
- [Public API](docs/api.md)
- [Trust and limits](docs/limitations.md)
- [Replay and recording](docs/replay-and-recording.md)
- [Testing and validation](docs/testing.md)
- [Source guides](docs/sources/index.md)
- [Case studies and screenshots](docs/case-studies.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Recommended workflows](docs/workflows.md)

## Release And Packaging

Package artifacts are built from `pyproject.toml` via `scikit-build-core`.

Local release validation:

```bash
python -m pip install build twine
pyproject-build
twine check dist/*
```

`pyproject-build` is used instead of `python -m build` because this repository has a top-level `build/` directory for CMake output.

Releases are intended to follow:
- changelog-driven notes in [CHANGELOG.md](CHANGELOG.md)
- a tagged GitHub release such as `v0.1.0`
- PyPI publishing from GitHub Actions through Trusted Publishing

## Development

Useful local commands:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen PYTHONPATH=python python -m sdr_signal_analyzer
pre-commit run --all-files
```

Contributor guidance lives in [CONTRIBUTING.md](CONTRIBUTING.md).

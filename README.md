# SDR Signal Analyzer

[![CI](https://github.com/schmijul/sdr-signal-analyzer/actions/workflows/ci.yml/badge.svg)](https://github.com/schmijul/sdr-signal-analyzer/actions/workflows/ci.yml)
[![Docs](https://github.com/schmijul/sdr-signal-analyzer/actions/workflows/docs.yml/badge.svg)](https://github.com/schmijul/sdr-signal-analyzer/actions/workflows/docs.yml)
[![Status](https://img.shields.io/badge/status-alpha-orange)](https://github.com/schmijul/sdr-signal-analyzer)
[![Python](https://img.shields.io/badge/python-3.10--3.12-blue)](https://github.com/schmijul/sdr-signal-analyzer/blob/main/pyproject.toml)

## What This Is

`sdr-signal-analyzer` is a C++-first SDR spectrum analyzer with a thin Python GUI, deterministic replay support, and optional live-device backends. It captures IQ samples, runs FFT-based analysis, estimates noise and occupied bandwidth, and presents the results through a CLI, Qt GUI, or embedded session API.

It is a reusable analysis backend, not a demodulator, protocol decoder, or measurement-grade instrument.

![Simulator-generated overview screenshot of the GUI. Not live RF validation.](docs/screenshots/overview.png)

*The committed screenshots are simulator-generated documentation assets. They demonstrate the offscreen GUI layout and backend wiring, not live RF performance.*

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
- structured session diagnostics surfaced through the C++ core, Python API, GUI, and CLI

Status taxonomy used in this repository:
- `Verified`: direct deterministic or mocked evidence exists in the repo now
- `Prepared for validation`: protocol, commands, and templates are ready
- `Pending lab validation`: requires attached-device evidence before stronger claims
- `Experimental`: intentionally heuristic or otherwise not a stability promise
- `Not supported`: unavailable on the current platform or build

`rtl_tcp` is the one backend with an explicit transport caveat: the built-in implementation uses POSIX sockets, is not authenticated, and is not encrypted. Treat it as a trusted-network workflow until a separate transport layer exists.

## What Is Experimental

The following remain prepared but not yet hardware-verified:
- UHD live streaming (`Prepared for validation`, `Pending lab validation`)
- SoapySDR live streaming (`Prepared for validation`, `Pending lab validation`)

The following are available but intentionally heuristic:
- heuristic detection labels such as `narrowband`, `burst-like`, and `likely FM`
- live-hardware performance characteristics until a real lab report is linked

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

### 4. Log measurements as JSONL

Log structured measurements over time as JSONL:

```bash
./build/sdr-analyzer-cli \
  --source simulator \
  --frames 20 \
  --export-jsonl measurements.jsonl \
  --export-interval 2
```

Expected result:
- the first JSONL record contains source and processing metadata
- later JSONL records contain frame-local detections and marker measurements
- the export is useful for analysis and reproducibility, not calibrated instrumentation

### 5. Replay a committed fixture

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

### 6. Connect to `rtl_tcp`

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
- the current built-in `rtl_tcp` path is intended for POSIX-like systems and trusted networks

## Expected Outputs

Typical CLI output includes:
- frame number
- estimated noise floor in `dBFS`
- strongest peak estimate in `dBFS`
- number of detections
- per-detection center frequency, bandwidth, and heuristic label
- optional JSONL export records for reproducible time-series analysis

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

| Backend | Status | Tested hardware | Sample rate range | Known limitations |
| --- | --- | --- | --- | --- |
| `simulator` | `Verified` | None | Verified at 2.4 Msps in the demo/smoke path | Synthetic source only |
| `replay` | `Verified` | None | Matches the committed fixture metadata | Depends on correct metadata and file integrity |
| `rtl_tcp` | `Verified` for the code path | Mock `rtl_tcp` server in CI | Verified at 2.4 Msps in the mock-server test | Network timing, trusted-network assumptions, and server behavior matter |
| `uhd` | `Prepared for validation`, `Pending lab validation` | Not yet release-validated on attached hardware | Not yet documented from verified runs | Requires UHD SDK and supported device |
| `soapy` | `Prepared for validation`, `Pending lab validation` | Not yet release-validated on attached hardware | Not yet documented from verified runs | Requires SoapySDR SDK and a working driver stack |

For the detailed plan and promotion criteria, see:
- [Hardware Validation Plan](docs/hardware_validation_plan.md)
- [Hardware Validation Status](docs/hardware_validation_status.md)

## Diagnostics

Runtime diagnostics are separate from measurement export.

Use CLI diagnostics when you need reproducible startup or backend failure evidence:

```bash
./build/sdr-analyzer-cli \
  --source replay \
  --input tests/fixtures/tone_cf32.sigmf-data \
  --meta tests/fixtures/tone_cf32.sigmf-meta \
  --frames 4 \
  --log-level info \
  --log-file diagnostics.log
```

Use JSON diagnostics when you want machine-readable bug-report artifacts:

```bash
./build/sdr-analyzer-cli \
  --source replay \
  --input tests/fixtures/tone_cf32.sigmf-data \
  --meta tests/fixtures/tone_cf32.sigmf-meta \
  --frames 4 \
  --log-level debug \
  --log-json \
  --log-file diagnostics.jsonl
```

Expected result:
- frame output stays on stdout
- diagnostics go to stderr and the requested log file
- `--export-jsonl` remains measurement data, not a diagnostics stream

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
- [Diagnostics](docs/diagnostics.md)
- [Quality evidence](docs/quality-evidence.md)
- [Replay and recording](docs/replay-and-recording.md)
- [Release readiness](docs/release.md)
- [Release checklist](docs/release_checklist.md)
- [Hardware validation plan](docs/hardware_validation_plan.md)
- [Hardware validation status](docs/hardware_validation_status.md)
- [Testing and validation](docs/testing.md)
- [Source guides](docs/sources/index.md)
- [Case studies and screenshots](docs/case-studies.md)
- [Troubleshooting](docs/troubleshooting.md)
- [Recommended workflows](docs/workflows.md)

## Release And Packaging

Package artifacts are built from `pyproject.toml` via `scikit-build-core`.

Local release validation:

```bash
python -m pip install -e ".[dev,gui]"
python3 scripts/release_validate.py --skip-install-smoke
```

The maintainer-facing release docs live in [docs/release.md](docs/release.md) and [docs/release_checklist.md](docs/release_checklist.md).

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

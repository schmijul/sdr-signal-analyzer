# SDR Signal Analyzer

C++-first SDR spectrum analyzer with a Python GUI on top. The backend owns capture, FFT processing, detection, recording, replay, and analysis so it can run headless through a CLI or be embedded into other frontends without pulling Python into the core runtime.

This repo currently ships as a strong MVP with:
- real-time spectrum, waterfall, and time-domain views
- automatic peak detection, bandwidth estimation, noise-floor estimation, and coarse labels
- raw `.bin` and SigMF recording/replay
- built-in simulator, self-implemented `rtl_tcp`, first-class optional UHD/USRP support, and optional SoapySDR
- backend regression tests plus Python/GUI smoke coverage

## Screenshots

Simulator-driven portfolio scenes generated from the checked-in codepath:

![Overview UI](docs/screenshots/overview.png)

![433 MHz style scene](docs/screenshots/ism_433.png)

![Narrowband focus](docs/screenshots/narrowband_focus.png)

## Quickstart

### Backend build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

### Python GUI

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

## Docs

- [Architecture](docs/architecture.md)
- [Public API](docs/api.md)
- [UHD / USRP Source Guide](docs/sources/uhd.md)
- [rtl_tcp Source Guide](docs/sources/rtltcp.md)
- [Replay And Recording](docs/replay-and-recording.md)
- [Testing And Validation](docs/testing.md)
- [Case Studies And Screenshots](docs/case-studies.md)

## Source Backends

- `simulator`: deterministic local source for development, screenshots, and most smoke tests
- `replay`: raw `.bin` plus JSON sidecar or SigMF replay path
- `rtl_tcp`: built-in network client for RTL-SDR style remote streaming
- `uhd`: first-class native USRP backend when UHD is available at build time
- `soapy`: optional generic device path when SoapySDR is available

## Representative CLI Flows

Run the simulator backend:

```bash
./build/sdr-analyzer-cli --source simulator --frames 20
```

Connect to an `rtl_tcp` server:

```bash
./build/sdr-analyzer-cli \
  --source rtl_tcp \
  --host 127.0.0.1 \
  --port 1234 \
  --center-hz 100000000 \
  --sample-rate 2400000 \
  --gain-db 20
```

Connect to a USRP through UHD:

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

Replay a committed fixture:

```bash
./build/sdr-analyzer-cli \
  --source replay \
  --input tests/fixtures/tone_cf32.sigmf-data \
  --meta tests/fixtures/tone_cf32.sigmf-meta \
  --frames 4
```

## Notes

- The screenshots in this repo are simulator-generated unless explicitly documented otherwise.
- Small replayable fixtures are committed under `tests/fixtures/` for deterministic tests and examples.
- UHD and Soapy are optional at build time; the repo still builds and tests cleanly without either SDK installed.

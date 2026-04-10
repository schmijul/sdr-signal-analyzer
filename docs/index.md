# SDR Signal Analyzer Docs

`sdr-signal-analyzer` is a backend-first SDR analysis stack with:
- a C++ core for source access, DSP, detection, recording, and replay
- Python bindings via `pybind11`
- a Qt GUI that consumes the same session API as the CLI

## Start Here

- [Architecture](architecture.md) for system layout and data flow
- [Public API](api.md) for the session/config/result model
- [Replay and Recording](replay-and-recording.md) for deterministic capture workflows
- [Testing and Validation](testing.md) for regression coverage and hardware checklists
- [Source Guides](sources/index.md) for backend-specific notes
- [Case Studies and Screenshots](case-studies.md) for reproducible demo evidence

## Installation Paths

### Python package

```bash
python -m pip install "sdr-signal-analyzer[gui]"
```

### Source build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Quick Commands

Run the simulator backend:

```bash
./build/sdr-analyzer-cli --source simulator --frames 20
```

Replay the committed SigMF fixture:

```bash
./build/sdr-analyzer-cli \
  --source replay \
  --input tests/fixtures/tone_cf32.sigmf-data \
  --meta tests/fixtures/tone_cf32.sigmf-meta \
  --frames 4
```

Launch the GUI from a source checkout:

```bash
PYTHONPATH=python python -m sdr_signal_analyzer
```

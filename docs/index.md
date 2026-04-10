# SDR Signal Analyzer Docs

`sdr-signal-analyzer` is a C++-first SDR analysis stack with a Python GUI, deterministic replay, and optional live-device backends. The docs follow the same stable path recommended in the README: simulator, replay, `rtl_tcp`, then hardware-specific backends.

## Start Here

- [Architecture](architecture.md) for system layout and data flow
- [Public API](api.md) for the public C++ and Python surface
- [Trust and Limits](limitations.md) for heuristic labels and failure modes
- [Replay and Recording](replay-and-recording.md) for deterministic capture workflows
- [Troubleshooting](troubleshooting.md) for common install and runtime blockers
- [Recommended Workflows](workflows.md) for the simulator-first path
- [Testing and Validation](testing.md) for regression coverage and validation scope
- [Source Guides](sources/index.md) for backend-specific notes
- [Case Studies and Screenshots](case-studies.md) for reproducible evidence

## Fast Paths

- Install with `python3 -m venv .venv` followed by `python -m pip install -e ".[gui]"`
- Run the demo with `sdr-signal-analyzer-demo`
- Run the simulator CLI with `./build/sdr-analyzer-cli --source simulator --frames 20`
- Replay the committed fixture with `./build/sdr-analyzer-cli --source replay --input tests/fixtures/tone_cf32.sigmf-data --meta tests/fixtures/tone_cf32.sigmf-meta --frames 4`

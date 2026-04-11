# SDR Signal Analyzer Docs

`sdr-signal-analyzer` is a C++-first SDR analysis stack with a Python GUI, deterministic replay, and optional live-device backends. The docs follow the same stable path recommended in the README: simulator, replay, `rtl_tcp`, then hardware-specific backends.

The canonical notation, code-symbol mapping, and terminology are documented separately so equations, code, and tests stay aligned.

![Simulator-generated overview screenshot of the GUI. Not live RF validation.](screenshots/overview.png)

## Start Here

- [Notation Registry](notation.md) for the canonical symbol table
- [Code-Symbol Mapping](code-symbol-mapping.md) for the code-to-symbol crosswalk
- [Terminology and Status Labels](terminology.md) for preferred wording and status labels
- [Architecture](architecture.md) for system layout and data flow
- [Public API](api.md) for the public C++ and Python surface
- [Trust and Limits](limitations.md) for heuristic labels and failure modes
- [Diagnostics](diagnostics.md) for runtime logging and bug-report evidence
- [Quality Evidence](quality-evidence.md) for reproducible artifacts and expected outputs
- [Replay and Recording](replay-and-recording.md) for deterministic capture workflows
- [Release Readiness](release.md) for maintainer release validation
- [Release Checklist](release_checklist.md) for the step-by-step cut process
- [Hardware Validation Plan](hardware_validation_plan.md) for attached-device lab preparation
- [Hardware Validation Status](hardware_validation_status.md) for backend status labels
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

## Status Labels

- `Verified`: direct deterministic or mocked evidence exists in the repository now.
- `Prepared for validation`: protocol, commands, and templates are present.
- `Pending lab validation`: attached-device evidence has not yet been collected.
- `Experimental`: intentionally heuristic or exploratory behavior.
- `Not supported`: unavailable on the current platform or build.

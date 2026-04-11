# Hardware Validation Plan

This page prepares live-lab validation work without claiming that it has already been done.

Status:
- `Prepared for validation`: yes
- `Pending lab validation`: UHD, SoapySDR

## Purpose

The repository already verifies the analyzer through simulator, replay, committed fixtures, mock `rtl_tcp`, and offscreen GUI smoke. This plan exists to make future attached-device sessions efficient, reproducible, and honest.

## Scope

Candidate backends for future lab sessions:
- `uhd`
- `soapy`

Related but already stronger in repo evidence:
- `simulator`: `Verified`
- `replay`: `Verified`
- `rtl_tcp`: code path `Verified` through a mock server, hardware/network behavior still environment-dependent

## Evidence Rules

What counts as validation evidence:
- exact commands used
- machine, OS, Python, compiler, SDK, firmware, and driver versions
- backend/device identification output
- saved diagnostics log
- saved JSONL measurement export
- screenshots or notes showing meaningful spectrum movement
- clean startup and clean shutdown notes
- replay of a recorded capture for deterministic follow-up analysis

What does not count as validation evidence:
- a claim that the backend compiled
- a single screenshot without environment details
- an anecdotal statement that “it seemed to work”
- benchmark numbers from simulator or replay
- CI results, because CI is intentionally hardware-free
- a diagnostics log without the corresponding session command
- a measurement export without runtime diagnostics for the same session

## Promotion Criteria

Do not mark a hardware backend as `Verified` until all of the following exist in a recorded report:
1. startup succeeds with the documented command
2. a short stream runs stably
3. center-frequency changes show meaningful spectrum movement
4. gain/sample-rate changes behave plausibly for the device
5. the session shuts down cleanly
6. diagnostics logs are saved
7. JSONL export is saved
8. environment and hardware metadata are recorded
9. at least one replayable capture is kept for later comparison when practical

## Lab Workflow

1. Start from a repo state that already passes the deterministic path:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

2. Use the stable order first:
   - simulator
   - replay
   - `rtl_tcp` if relevant
   - target hardware backend
3. Run the backend-specific checklist:
   - [UHD / USRP Source Guide](sources/uhd.md)
   - [SoapySDR Source Guide](sources/soapy.md)
4. Fill in the report template:
   - [Hardware Validation Report Template](hardware_validation_report_template.md)

The docs-visible page above is the canonical template. `docs/templates/hardware_validation_report_template.md` now exists only as a maintainer pointer so the template content does not drift in two places.

## Required Artifacts Per Session

Save all of the following under a session directory such as `lab-notes/YYYY-MM-DD-backend-name/`:
- `commands.txt`
- `diagnostics.log` or `diagnostics.jsonl`
- `measurements.jsonl`
- screenshots
- recorded capture paths
- completed report markdown

Keep runtime diagnostics and measurement export as separate files. The diagnostics log is the software evidence trail; `measurements.jsonl` is the analyzer-output record.

## Command Templates

Startup smoke:

```bash
./build/sdr-analyzer-cli --source uhd --device-args "type=b200" --center-hz 100000000 --sample-rate 2000000 --gain-db 20 --frames 20 --log-level info --log-file diagnostics.log
```

Short stream with measurement export:

```bash
./build/sdr-analyzer-cli --source uhd --device-args "type=b200" --center-hz 100000000 --sample-rate 2000000 --gain-db 20 --frames 50 --export-jsonl measurements.jsonl --export-interval 5 --log-level info --log-file diagnostics.log
```

GUI session:

```bash
PYTHONPATH=python python -m sdr_signal_analyzer
```

Replay comparison after a recorded capture exists:

```bash
./build/sdr-analyzer-cli --source replay --input CAPTURE.sigmf-data --meta CAPTURE.sigmf-meta --frames 20 --log-level info --log-file replay-diagnostics.log
```

Failure reproduction with structured diagnostics:

```bash
./build/sdr-analyzer-cli --source soapy --device-string "driver=rtlsdr" --center-hz 100000000 --sample-rate 2048000 --gain-db 20 --frames 10 --log-level debug --log-json --log-file diagnostics.jsonl
```

Suggested failure bundle workflow for a live session:

1. run the target command with diagnostics enabled
2. rerun with `--export-jsonl` if analyzer output is relevant
3. save the exact command line in `commands.txt`
4. place `commands.txt`, diagnostics, exports, screenshots, and report notes in the same session directory

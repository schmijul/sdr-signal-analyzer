# Diagnostics And Observability

Measurement export and runtime diagnostics are separate on purpose.

- JSONL measurement export:
  reproducible analyzer output for later analysis
- diagnostics logs:
  software/runtime evidence for debugging backend startup, replay failures, and session lifecycle

Do not treat JSONL measurement export as a software log stream.

## Which Artifact To Collect

| If you need... | Collect... | Why |
| --- | --- | --- |
| backend startup, disconnect, or shutdown evidence | diagnostics log via `--log-level`, `--log-file`, and optionally `--log-json` | it preserves runtime context, severity, component, and error codes |
| analyzer outputs over time | JSONL measurement export via `--export-jsonl` | it preserves frame-local detections and marker measurements for later analysis |
| a reproducible hardware bug report | both artifacts, plus the exact command and environment details | diagnostics explain the software failure path while measurement export shows what the analyzer saw before or during the issue |

For attached-device sessions, save the diagnostics log and the measurement export as separate files in the same artifact directory. They answer different questions and should not be merged into one stream.

## CLI Diagnostics

The CLI supports:
- `--log-level error|warn|info|debug`
- `--log-file PATH`
- `--log-json`

Example:

```bash
./build/sdr-analyzer-cli \
  --source replay \
  --input tests/fixtures/tone_cf32.sigmf-data \
  --meta tests/fixtures/tone_cf32.sigmf-meta \
  --frames 4 \
  --log-level info \
  --log-file quality-artifacts/diagnostics/replay.log
```

JSON-formatted diagnostics:

```bash
./build/sdr-analyzer-cli \
  --source simulator \
  --frames 4 \
  --log-level debug \
  --log-json \
  --log-file quality-artifacts/diagnostics/simulator.jsonl
```

Expected fields in a diagnostic record:
- `timestamp_utc`
- `level`
- `component`
- `code`
- `message`
- `session_id`
- `source_kind`
- `source_description`

## Python API Diagnostics

`AnalyzerSession` exposes:
- `diagnostics()`
- `drain_diagnostics()`
- `last_error()`

Example:

```python
from sdr_signal_analyzer import AnalyzerSession, ProcessingConfig, SourceConfig, SourceKind

source = SourceConfig()
source.kind = SourceKind.REPLAY
session = AnalyzerSession(source, ProcessingConfig())
if not session.start():
    for entry in session.diagnostics():
        print(entry.level, entry.code, entry.message)
```

Use this path for bug reports, GUI debugging, and backend-unavailable investigations.

Typical backend-unavailable pattern:
- `last_error()` contains the contextual startup failure
- the last diagnostic record uses `code == "source.create_failed"` or `code == "source.configure_failed"`
- `source_kind` preserves the requested backend such as `replay`, `uhd`, or `soapy`

## Debug Workflow

Preferred order:
1. `simulator`
2. `replay`
3. `rtl_tcp`
4. `uhd` or `soapy`

When a failure occurs:
1. rerun with `--log-level info`
2. save a log file with `--log-file`
3. attach the exact command
4. if the issue is analysis-related, also attach a JSONL export
5. if hardware is involved, record device, driver, SDK, and OS details

Recommended artifact directory for one investigation:

```text
quality-artifacts/diagnostics/<date>-<source-kind>/
```

Suggested contents:
- `command.txt`
- `diagnostics.log` or `diagnostics.jsonl`
- `measurements.jsonl` when analyzer output matters
- environment notes for live hardware sessions

## Verified Now

- contextual startup and runtime errors are buffered as diagnostics in the shared session layer
- CLI logging is deterministic and hardware-free
- the GUI surfaces runtime stop reasons instead of leaving the UI in a stale running state

## Pending Lab Validation

- backend-specific diagnostic quality for real UHD hardware behavior
- backend-specific diagnostic quality for real SoapySDR driver stacks

The logging path is prepared for those sessions but not yet backed by attached-hardware evidence.

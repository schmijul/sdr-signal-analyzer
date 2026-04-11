# Hardware Validation

This page prepares live lab work. It does not claim that lab validation has already happened.

Status labels:
- `Verified`: direct deterministic or mocked evidence exists in the repository now.
- `Prepared for validation`: commands, templates, and protocols are present.
- `Pending lab validation`: attached-device evidence has not yet been collected.
- `Experimental`: intentionally heuristic or exploratory behavior.

## Current Status

| Backend | Status | Evidence |
| --- | --- | --- |
| `simulator` | `Verified` | deterministic tests, demo smoke, GUI smoke |
| `replay` | `Verified` | committed fixtures, replay regression tests, CLI export tests |
| `rtl_tcp` | `Verified` for the code path | mock transport test, deterministic disconnect/error handling |
| `uhd` | `Prepared for validation`, `Pending lab validation` | optional backend implementation, backend-unavailable diagnostics tests, lab protocol below |
| `soapy` | `Prepared for validation`, `Pending lab validation` | optional backend implementation, backend-unavailable diagnostics tests, lab protocol below |

## What Counts As Validation Evidence

Acceptable evidence for moving `uhd` or `soapy` to `Verified`:
- exact command lines used
- attached logs from `--log-level info` or `debug`
- saved JSONL measurement export from the same session
- hardware metadata: device model, firmware/FPGA, SDK/driver version, antenna/cabling notes
- a short stability run with clean startup and shutdown
- replay comparison against a saved capture where possible

Not enough on its own:
- “it seemed to work”
- screenshots without commands or metadata
- code compiles with the SDK installed
- a simulator or replay run
- a diagnostics log without the corresponding command and environment metadata
- a measurement export without the runtime diagnostics for the same session

## Lab Command Templates

CLI startup smoke:

```bash
./build/sdr-analyzer-cli \
  --source uhd \
  --device-args "type=b200" \
  --channel 0 \
  --center-hz 100000000 \
  --sample-rate 2000000 \
  --gain-db 20 \
  --frames 20 \
  --log-level info \
  --log-file quality-artifacts/lab/uhd-startup.log
```

CLI short stream plus JSONL export:

```bash
./build/sdr-analyzer-cli \
  --source soapy \
  --device-string "driver=rtlsdr" \
  --channel 0 \
  --center-hz 433920000 \
  --sample-rate 2400000 \
  --gain-db 20 \
  --frames 200 \
  --log-level info \
  --log-file quality-artifacts/lab/soapy-stream.log \
  --export-jsonl quality-artifacts/lab/soapy-stream.jsonl \
  --export-interval 5
```

GUI lab smoke:

```bash
QT_QPA_PLATFORM=xcb PYTHONPATH=python python -m sdr_signal_analyzer
```

Replay comparison after a captured run:

```bash
./build/sdr-analyzer-cli \
  --source replay \
  --input captures/lab.sigmf-data \
  --meta captures/lab.sigmf-meta \
  --frames 50 \
  --log-level info \
  --log-file quality-artifacts/lab/replay-compare.log \
  --export-jsonl quality-artifacts/lab/replay-compare.jsonl
```

## UHD Checklist

Status: `Prepared for validation`, `Pending lab validation`

Prerequisites:
- UHD SDK installed before configure time
- device visible to UHD tooling
- known-good `--device-args`

Matrix to cover:
- sample rate: 1 Msps, 2 Msps, 5 Msps if the device supports it
- center frequency: one FM-broadcast-like target, one quieter band, one device-specific high-frequency target
- gain: low, medium, and high gain values that the device accepts

Checks:
1. confirm the device enumerates outside this repo
2. start a short stream and confirm frames arrive
3. tune center frequency and verify visible spectrum movement
4. vary sample rate and gain and confirm the session remains stable
5. save diagnostics log and JSONL export
6. record a capture and replay it
7. document clean shutdown behavior
8. keep diagnostics and measurement export as separate files

Failure capture:
- exact command
- `last_error()` text
- diagnostics log path
- measurement export path when analysis output matters
- environment metadata

## SoapySDR Checklist

Status: `Prepared for validation`, `Pending lab validation`

Prerequisites:
- SoapySDR core and device driver installed before configure time
- known-good `--device-string`
- device visible to Soapy tooling outside this repo

Matrix to cover:
- sample rate: device-appropriate low/medium/high rates
- center frequency: at least one strong known signal and one quieter range
- gain: minimum, midpoint, and a high but not clipping-prone setting

Checks:
1. confirm the device is visible to Soapy tooling
2. run startup smoke and short streaming run
3. change center frequency and gain while observing motion
4. save diagnostics and JSONL export as separate artifacts
5. compare a recorded capture through replay
6. document driver-specific warnings or quirks

## Acceptance Criteria For Future Verification

Before marking `uhd` or `soapy` as verified in README/docs:
- startup succeeds on attached hardware
- at least one short stream is stable
- meaningful spectrum motion is visible
- shutdown is clean
- exact commands are preserved
- diagnostics logs and JSONL exports are attached
- environment and hardware metadata are recorded
- at least one report based on [the hardware validation report template](hardware_validation_report_template.md) is committed or linked

# UHD / USRP Source Guide

## Purpose

Use the `uhd` source when you want first-class USRP support without routing through SoapySDR.

This backend is optional:
- it is compiled only when UHD is available at build time
- the repo still builds without UHD
- CI does not require physical USRP hardware

Stability note:
- the integration point exists
- the release docs should treat UHD as hardware-dependent until a real-device validation run is captured

## Build Requirements

You need UHD development headers and libraries installed before configuring the project.

Typical Linux flow:

```bash
cmake -S . -B build
cmake --build build
```

If UHD is found, the native backend is compiled in automatically.

## CLI Example

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

## Important Fields

- `--device-args`
  - UHD device argument string, for example `type=b200` or `addr=192.168.10.2`
- `--channel`
  - RX channel index
- `--antenna`
  - RX antenna name such as `RX2`
- `--bandwidth-hz`
  - requested analog bandwidth
- `--clock-source`
  - optional UHD clock source string
- `--time-source`
  - optional UHD time source string

## GUI Use

In the GUI:
- choose `UHD` in the source selector
- fill in `UHD Device Args`
- optionally set channel, antenna, bandwidth, clock source, and time source
- use the common center-frequency, sample-rate, and gain controls as normal

## Manual Validation Checklist

Use this checklist on a machine with a real USRP attached:

1. Start with a known-good UHD device argument string.
2. Verify the session starts without `last_error()`.
3. Change center frequency, sample rate, and gain and confirm the live view updates.
4. If the radio supports it, vary analog bandwidth and antenna selection.
5. Record a short capture and replay it through the replay source.
6. Compare replay detections against the live run qualitatively.

Expected outcome:
- the session starts cleanly with the intended device args
- live snapshots appear without backend errors
- replay of a captured file remains the preferred deterministic comparison path

## Failure Modes

Common startup failures:
- UHD not found at build time
- device args do not match an attached radio
- unsupported antenna name
- unsupported channel index
- unsupported bandwidth or sample rate

Runtime failures usually surface through `last_error()` and stop the session.

## Current Scope

The current UHD backend focuses on RX streaming into the analyzer. It does not try to solve:
- GPSDO workflows
- timed streaming orchestration
- multi-channel coherent capture
- TX support

Those can be added later without changing the analyzer-facing session model.

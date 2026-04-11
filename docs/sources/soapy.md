# SoapySDR Source Guide

## Purpose

Use the `soapy` source when you want to route supported hardware through SoapySDR instead of the native UHD path or the built-in `rtl_tcp` client.

Status:
- integration exists in the codebase
- build-time behavior is exercised without hardware
- live-device behavior is `Prepared for validation` and `Pending lab validation`

## Build Requirements

SoapySDR must be installed before configuring the project. The repository still builds without it.

Typical configure flow:

```bash
cmake -S . -B build
cmake --build build
```

If SoapySDR is found, the backend is compiled in automatically.

## CLI Example

```bash
./build/sdr-analyzer-cli \
  --source soapy \
  --device-string "driver=rtlsdr" \
  --channel 0 \
  --center-hz 433920000 \
  --sample-rate 2400000 \
  --gain-db 20
```

## GUI Use

In the GUI:
- choose `SoapySDR`
- provide the device string
- use the shared center-frequency, sample-rate, and gain controls

## Current Scope

The backend is focused on RX streaming into the analyzer. It does not currently claim:
- device-specific calibration
- vendor-specific advanced features
- timing-sensitive orchestration
- TX workflows

## Failure Modes

Common startup failures:
- SoapySDR not found at build time
- driver missing for the target device
- invalid or incomplete device string
- unsupported sample rate, frequency, or gain settings

Runtime failures should surface through `last_error()` and session diagnostics.

## Lab Protocol

Use the checklist in [../hardware_validation_plan.md](../hardware_validation_plan.md) and [../hardware_validation_status.md](../hardware_validation_status.md) before changing the backend status to verified.

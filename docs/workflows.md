# Recommended Workflows

This page documents the default path the project wants users to follow.

## Stable Default Flow

1. Start with the simulator.
2. Move to replay using a committed fixture.
3. Use `rtl_tcp` for network-delivered live data.
4. Move to UHD or SoapySDR only after the hardware-free path is working.

This order keeps debugging deterministic:
- simulator validates the CLI, GUI, and analysis pipeline without hardware
- replay validates file I/O and metadata handling
- `rtl_tcp` validates network streaming and source control
- hardware backends validate the final live-device path

## One-Command Demo

For a quick reviewer or maintainer smoke test, use:

```bash
sdr-signal-analyzer-demo
```

Expected result:
- the simulator runs without extra hardware setup
- detections print to stdout
- the command exits cleanly in under two minutes

## Capture And Replay

Record first, then replay the same asset:

```bash
./build/sdr-analyzer-cli --source simulator --frames 100 --record-base captures/demo --record-format sigmf
./build/sdr-analyzer-cli --source replay --input captures/demo.sigmf-data --meta captures/demo.sigmf-meta
```

Use this path when you want deterministic documentation assets or regression fixtures.

## Public API Boundaries

The stable public surfaces are:
- C++ headers in `include/sdr_analyzer/`
- Python names exported from `sdr_signal_analyzer.__all__`
- installed executables documented in the README

Internal-only surfaces are:
- `src/**`
- private GUI implementation details
- `_`-prefixed helpers in the Python package

Treat anything outside those boundaries as implementation detail unless it is explicitly documented otherwise.

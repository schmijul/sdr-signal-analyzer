# Verified Behavior

This page keeps the strongest repo-backed claims close to the exact commands that reproduce them.

## Deterministic Replay

Input:
- `tests/fixtures/tone_cf32.sigmf-data`
- `tests/fixtures/tone_cf32.sigmf-meta`

Command:

```bash
./build/sdr-analyzer-cli \
  --source replay \
  --input tests/fixtures/tone_cf32.sigmf-data \
  --meta tests/fixtures/tone_cf32.sigmf-meta \
  --frames 4
```

Observed replay result:
- one narrowband detection at `100.15 MHz`
- strongest peak at `-8.87819 dBFS`
- occupied bandwidth at `5859.375 Hz`
- replay stops cleanly at end of stream after the fixture is consumed

Notes:
- the signal metrics above are stable across repeated runs of the committed fixture
- diagnostic timestamps differ across runs, so raw log lines are not byte-identical

Representative CLI output:

```text
Session stopped after 1 frame(s): Source read for source 'replay' failed: Replay file reached end of stream.
frame=0 noise=-140dBFS strongest=-8.87819dBFS detections=1
  peak=1.0015e+08Hz bw=5859.38Hz power=-8.87819dBFS label=narrowband
```

## Example Output

The screenshot below was regenerated locally from the committed replay fixture through the Qt GUI in offscreen mode.

![Replay-backed FFT example](img/example_fft.png)

## Hardware Sanity

Current repo evidence is intentionally conservative:
- replay is fully verified in-repo
- `rtl_tcp` is verified for the code path through a mock transport test
- UHD and SoapySDR are implemented, but attached-device validation is still pending

For the current hardware status matrix, see [Hardware Validation Status](hardware_validation_status.md).

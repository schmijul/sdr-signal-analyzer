# Trust And Limits

## What The Numbers Mean

The analyzer reports power in `dBFS`, which is relative to the full-scale complex sample representation used by the backend. It is useful for comparing captures, frames, and detections inside this tool, but it is not absolute RF power and it is not calibrated dBm.

The spectrum display is derived from the current FFT frame after windowing and normalization. That means:
- `fft_size` controls bin resolution and display detail
- the Hann window reduces leakage but also changes peak shape and amplitude
- `average_dbfs` and `peak_hold_dbfs` are display aids, not separate measurements

## What Detection Means

Detection is heuristic. The analyzer looks for spectral peaks, estimates bandwidth from local occupancy, and adds coarse labels from simple rules.

Current labels are intentionally modest:
- `broadband`
- `narrowband`
- `likely FM`
- `burst-like`
- `unclassified`

These labels are useful for triage and UI filtering, but they are not validated classifiers. They should be read as descriptive hints, not as protocol identification or modulation certainty.

Stable workflow reminder:
1. simulator
2. replay
3. `rtl_tcp`
4. hardware backends

This ordering keeps the trusted path deterministic until you intentionally move to live radios.

## Known Failure Modes

The current pipeline can be misleading when:
- the source metadata is wrong or incomplete
- the sample rate or center frequency is stale
- the signal sits near the noise floor
- the capture has strong DC/LO artifacts
- the occupied bandwidth changes rapidly inside one FFT frame
- `rtl_tcp` input is lossy or unstable at the network level
- the signal is broader than the current `fft_size` or narrower than the bin resolution

In those cases the analyzer can still produce useful snapshots, but the results should be treated as approximate.

## What The Project Does Not Claim

The project does not currently claim:
- absolute RF power calibration
- demodulation
- protocol decoding
- regulatory compliance measurements
- research-grade classification accuracy
- measurement-grade instrumentation

## How To Trust Results

Use the following hierarchy:
1. Simulator and replay prove deterministic pipeline behavior.
2. Synthetic fixtures prove regression behavior for known tones and bursts.
3. Real RTL-SDR and USRP captures prove the workflow against live hardware.

For public examples, prefer capture notes that state:
- center frequency
- sample rate
- approximate expected bandwidth
- hardware used
- whether the asset is synthetic, replayed, or live-captured

That keeps the repo honest about what is measured and what is inferred.

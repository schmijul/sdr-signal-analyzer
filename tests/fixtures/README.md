# Test Fixtures

This directory contains small committed replay fixtures used for deterministic tests and documentation examples.

Current fixtures:
- `tone_cf32.bin`
- `tone_cf32.bin.json`
- `tone_cf32.sigmf-data`
- `tone_cf32.sigmf-meta`
- `single_tone.sigmf-data`
- `single_tone.sigmf-meta`
- `multi_tone.sigmf-data`
- `multi_tone.sigmf-meta`
- `burst.sigmf-data`
- `burst.sigmf-meta`
- `weak.sigmf-data`
- `weak.sigmf-meta`
- `wideband.sigmf-data`
- `wideband.sigmf-meta`
- `clipped.sigmf-data`
- `clipped.sigmf-meta`

Fixture classes:
- `tone_cf32.*`: baseline narrowband tone used across raw and SigMF replay paths
- `single_tone.*`: deterministic single-peak replay coverage
- `multi_tone.*`: multiple separated narrowband peaks in one capture
- `burst.*`: burst-like transient behavior for heuristic labeling coverage
- `weak.*`: near-noise detection behavior
- `wideband.*`: broader occupied spectrum and detection-span behavior
- `clipped.*`: replay handling of clipped or distortion-heavy synthetic input

All fixtures are synthetic and deterministic. They are meant to prove analyzer behavior, not live RF truth.

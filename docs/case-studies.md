# Case Studies And Screenshots

## Scope

This file explains what the current screenshots and fixtures prove.

Important distinction:
- the committed screenshots are simulator-generated unless stated otherwise
- the committed replay fixtures are synthetic narrowband test captures
- these assets prove reproducibility and pipeline behavior, not calibrated RF truth

## Overview UI

Image: `docs/screenshots/overview.png`

What it shows:
- full spectrum view
- waterfall rendering
- time-domain view
- detection table
- marker-based annotation

What it proves:
- the GUI is fully driven by the backend snapshot API
- the analyzer can display multiple signal-like structures simultaneously
- the docs/screenshots are reproducible from code, not hand-edited assets
- the same backend path can power both the GUI and the CLI

## 433 MHz Style Scene

Image: `docs/screenshots/ism_433.png`

What it shows:
- a simulation positioned around 433.92 MHz
- visually plausible ISM-band style activity

What it proves:
- the UI handles alternate center frequencies and marker placement cleanly
- the portfolio can present sub-GHz workflows in a reproducible way
- the simulator path is useful for documentation and quick demos

What it does not prove:
- this is not a real over-the-air ISM capture
- this does not validate the analyzer against live RF conditions

## Narrowband Fixture

Image: `docs/screenshots/narrowband_focus.png`

Related fixtures:
- `tests/fixtures/tone_cf32.bin`
- `tests/fixtures/tone_cf32.bin.json`
- `tests/fixtures/tone_cf32.sigmf-data`
- `tests/fixtures/tone_cf32.sigmf-meta`

What it shows:
- a deterministic narrowband tone around 100.15 MHz
- replayable input suitable for regression tests

What it proves:
- replay analysis is deterministic
- raw and SigMF paths both support the same basic narrowband example
- bandwidth estimation and marker measurement can be validated against a known synthetic signal, within the limits described in the trust page
- CLI replay prints repeatable detections for the same capture

## CLI Replay Example

Input data:
- `tests/fixtures/tone_cf32.sigmf-data`
- `tests/fixtures/tone_cf32.sigmf-meta`

Command:

```bash
./build/sdr-analyzer-cli --source replay --input tests/fixtures/tone_cf32.sigmf-data --meta tests/fixtures/tone_cf32.sigmf-meta --frames 4
```

Expected output:
- frame lines with `noise=` and `detections=`
- detections near `100.15 MHz`

Interpretation:
- this is a deterministic replay proof, not a live-spectrum calibration

## Regenerating Screenshots

Use the screenshot generator in `scripts/` with an offscreen Qt session.

Example:

```bash
QT_QPA_PLATFORM=offscreen .venv/bin/python scripts/generate_portfolio_assets.py \
  --output docs/screenshots/overview.png \
  --center-hz 100000000 \
  --sample-rate 2400000 \
  --gain-db 10 \
  --marker-hz 100000000 \
  --marker-bw-hz 200000
```

## What Still Improves The Portfolio

The next step up from the current docs set is real capture evidence:
- FM broadcast recording with replay and expected bandwidth notes
- real 433 MHz activity capture
- one or more actual hardware screenshots from RTL-SDR and USRP sessions

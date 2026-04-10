# Replay And Recording

## Why Replay Matters

Recording and replay make the analyzer reproducible:
- captures can be reviewed later
- screenshots and docs can point at deterministic examples
- regressions can be tested without live hardware

## Supported Formats

### Raw `.bin` + JSON sidecar

Raw data is stored as complex float32 IQ samples.

The sidecar metadata stores:
- `sample_rate_hz`
- `center_frequency_hz`
- `gain_db`
- `frame_samples`
- `sample_format`
- `capture_start_utc`

Example:
- data: `capture.bin`
- metadata: `capture.bin.json`

### SigMF

SigMF recording uses:
- `capture.sigmf-data`
- `capture.sigmf-meta`

The current metadata handling includes:
- datatype
- sample rate
- capture center frequency
- capture datetime
- gain extension field

## Record Examples

Raw:

```bash
./build/sdr-analyzer-cli \
  --source simulator \
  --frames 100 \
  --record-base captures/demo_raw \
  --record-format raw
```

SigMF:

```bash
./build/sdr-analyzer-cli \
  --source simulator \
  --frames 100 \
  --record-base captures/demo_sigmf \
  --record-format sigmf
```

## Replay Examples

Raw:

```bash
./build/sdr-analyzer-cli \
  --source replay \
  --input captures/demo_raw.bin \
  --meta captures/demo_raw.bin.json
```

SigMF:

```bash
./build/sdr-analyzer-cli \
  --source replay \
  --input captures/demo_sigmf.sigmf-data \
  --meta captures/demo_sigmf.sigmf-meta
```

Committed fixture:

```bash
./build/sdr-analyzer-cli \
  --source replay \
  --input tests/fixtures/tone_cf32.bin \
  --meta tests/fixtures/tone_cf32.bin.json \
  --frames 4
```

Expected result:
- repeatable detections near `100.15 MHz`
- a clean stop at end of stream
- identical behavior through raw and SigMF replay paths

## Replay Validation Rules

Current replay behavior is intentionally stricter than the earliest MVP:
- replay requires an input file
- explicitly requested metadata must exist
- SigMF replay requires a matching `.sigmf-meta` file
- malformed or truncated data stops the session with an error

## Committed Fixtures

The repo currently ships a small deterministic fixture set under `tests/fixtures/`:
- `tone_cf32.bin`
- `tone_cf32.bin.json`
- `tone_cf32.sigmf-data`
- `tone_cf32.sigmf-meta`

These fixtures are synthetic narrowband tones used for:
- replay regression tests
- documentation examples
- deterministic CLI demos

If you need broader replay coverage, add more committed fixtures rather than relying only on inline generated samples. Reproducible assets are preferred for cases that should remain stable over time.

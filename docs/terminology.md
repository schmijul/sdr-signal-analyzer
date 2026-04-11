# Terminology And Status Labels

This page defines the canonical terms used across the repository. Use these terms in README content, docs entry points, code comments, and review notes.

## Canonical Terms

| Canonical term | Preferred code identifiers | Meaning | Avoid or use only locally |
| --- | --- | --- | --- |
| sample rate | `sample_rate_hz` | Complex sample rate of the capture stream | sampling rate, rate |
| center frequency | `center_frequency_hz` | RF center frequency for the capture stream | tuning frequency, freq |
| FFT size | `fft_size` | Number of complex samples per transform frame | frame length, transform size |
| bin resolution | `bin_resolution_hz` | Frequency spacing between adjacent FFT bins | bin width when the formula is already clear |
| noise floor | `noise_floor_dbfs` | Low-quantile frame noise estimate | baseline, floor estimate |
| strongest peak | `strongest_peak_dbfs` | Maximum power bin in the current frame | max peak |
| detection | `DetectionResult` | A heuristic spectral peak result | classifier, protocol ID |
| marker measurement | `MarkerMeasurement` | Frame-local measurement within a user marker | cursor measurement |
| measurement export | `export_jsonl(...)` | Reproducible snapshot export in JSONL form | diagnostics log |
| diagnostics | `DiagnosticEntry` | Runtime evidence for startup, session, and backend behavior | measurement log |

## Status Labels

Use these labels exactly as written:

- `Verified`: direct deterministic or mocked evidence exists in the repository now.
- `Prepared for validation`: protocol, commands, and templates are present.
- `Pending lab validation`: attached-device evidence has not yet been collected.
- `Experimental`: intentionally heuristic or exploratory behavior.
- `Not supported`: unavailable on the current platform or build path.

## What Each Label Means

`Verified` is reserved for behavior with direct evidence in the repository, such as a passing deterministic test or a mocked backend test.

`Prepared for validation` means the repo has the commands, checklists, and templates required to run a future lab session, but the attached-hardware claim has not yet been made.

`Pending lab validation` means the feature is expected to be testable on real hardware, but no attached-device evidence is present yet.

`Experimental` means the behavior is intentionally heuristic or exploratory. It may be useful, but it is not a promise of classifier or measurement accuracy.

`Not supported` means the feature is unavailable on the current platform or build configuration.

## Consistency Rule

If a doc says `sample rate`, the code should say `sample_rate_hz`. If a doc says `center frequency`, the code should say `center_frequency_hz`. If a status label appears in one file, it should use the same capitalization and wording everywhere else.

That rule keeps the docs, code, and tests aligned.

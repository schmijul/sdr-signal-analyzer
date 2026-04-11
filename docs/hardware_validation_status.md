# Hardware Validation Status

This matrix separates what is verified in the repository now from what is only prepared for later lab work.

## Status Labels

- `Verified`: direct deterministic or mocked evidence exists in the repository now.
- `Prepared for validation`: protocol, commands, and templates are present.
- `Pending lab validation`: attached-device evidence has not yet been collected.
- `Experimental`: intentionally heuristic or exploratory behavior.
- `Not supported`: unavailable on the current platform/build path.

## Backend Matrix

| Backend | Status | Evidence in repo now | Still pending |
| --- | --- | --- | --- |
| `simulator` | `Verified` | deterministic CLI, GUI, Python, and regression tests | none for the synthetic path |
| `replay` | `Verified` | committed fixtures, replay/record tests, CLI replay smoke | more fixture classes can still be added |
| `rtl_tcp` | `Verified` for the code path | mock transport test, source guide, troubleshooting notes | attached-server behavior, network conditions, and live-RF observations remain environment-dependent |
| `uhd` | `Prepared for validation`, `Pending lab validation` | optional backend implementation, backend-unavailable diagnostics tests, documented checklist, command templates, canonical report template | attached-device startup, short-stream stability, tuning/gain evidence, logs, exports, screenshots |
| `soapy` | `Prepared for validation`, `Pending lab validation` | optional backend implementation, backend-unavailable diagnostics tests, documented checklist, command templates, canonical report template | attached-device startup, short-stream stability, tuning/gain evidence, logs, exports, screenshots |

## Feature Class Matrix

| Area | Status | Notes |
| --- | --- | --- |
| heuristic labels such as `narrowband`, `burst-like`, `likely FM` | `Experimental` | deterministic tests cover behavior, not classifier truth |
| JSONL measurement export | `Verified` | reproducible analysis artifact, not a diagnostics log and not calibrated instrumentation |
| CLI diagnostics logging | `Verified` | session-backed diagnostics are available through `--log-level`, `--log-file`, and optional JSON diagnostics |
| live RF performance claims | `Pending lab validation` | do not make release claims without attached-device evidence |

## Required Next Evidence

Before changing UHD or SoapySDR status:
1. run the backend checklist
2. save diagnostics and JSONL export as separate artifacts
3. record environment metadata
4. fill in the report template
5. link the completed report from docs or release notes

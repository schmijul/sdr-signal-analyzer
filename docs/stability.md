# API And Schema Stability

`sdr-signal-analyzer` is still an alpha project. Public-facing contracts exist, but they are intentionally narrow and must not be described more broadly than the repository can support today.

## Stability Classes

### Public Contract

The following surfaces are public and must be reviewed carefully before changing:

- C++ headers under `include/sdr_analyzer/`
- Python names exported from `python/sdr_signal_analyzer/__init__.py`
- documented CLI options and output behavior
- the JSONL measurement export schema described in [Public API](api.md)

Changes to these surfaces require:

- matching documentation updates
- explicit regression tests
- release-note or changelog visibility when behavior changes

Implementation locations:

- public C++ types and methods: `include/sdr_analyzer/config.hpp`, `include/sdr_analyzer/results.hpp`, `include/sdr_analyzer/session.hpp`
- Python export helpers: `python/sdr_signal_analyzer/export.py`
- CLI export and diagnostics emission: `src/cli/main.cpp`

### Internal Implementation

The following are not public contracts and may change without a schema or API promise:

- files under `src/` unless mirrored by a documented public header or CLI contract
- transport-specific source classes and factory logic
- DSP helper structure and private algorithms
- GUI widget internals and helper methods prefixed with `_`

Internal changes still need tests when they affect behavior, but they do not by themselves create a public compatibility promise.

## JSONL Measurement Schema Policy

The measurement export schema is versioned by the `format_version` field.

Current version:

- `sdr_signal_analyzer.measurements.v1`

Implementation locations:

- Python helper constant: `python/sdr_signal_analyzer/export.py`
- CLI constant and writers: `src/cli/main.cpp`
- regression tests: `tests/test_cli_export.py`, `tests/test_export_helper.py`, `tests/export_schema.py`

### `v1` Contract

For `sdr_signal_analyzer.measurements.v1`, the CLI/common schema contract is:

- the first record is a metadata record with `record_type == "metadata"`
- later records are frame records with `record_type == "frame"`
- field names documented in [Public API](api.md) are exact for the CLI output
- nested detection and marker-measurement objects also use exact documented field names

This repo intentionally tests exact key sets for the CLI/common `v1` schema. Do not add, remove, or rename CLI measurement fields inside `v1` casually.

### Python Helper Scope

The Python helper layer may emit optional helper-only metadata such as `session_metadata` when the caller provides it.

That field is implemented in `python/sdr_signal_analyzer/export.py`. It is not part of the required CLI/common `v1` metadata shape, and consumers that target CLI output should not assume it is present.

### When To Bump The Version

Change `format_version` when any of the following happen:

- a required top-level field is added, removed, or renamed
- a nested required field in detections or marker measurements changes
- record ordering or record-type semantics change
- a field changes meaning in a backward-incompatible way

Pure documentation clarifications do not require a version bump. Additive helper-only fields outside the CLI/common contract must be documented explicitly and kept out of the CLI `v1` schema unless the policy is revised.

## CLI Compatibility

The CLI is public-facing, but still alpha. The practical rule is:

- documented options and output files should not change without docs, tests, and release-note visibility
- undocumented formatting details that are not part of the JSONL measurement schema are not stable contracts

Implementation locations:

- CLI option parsing: `src/cli/main.cpp`
- CLI export tests: `tests/test_cli_export.py`

## Extension Points

Supported extension-oriented entry points today:

- constructing and controlling `AnalyzerSession`
- supplying `SourceConfig`, `ProcessingConfig`, and `RecordingConfig`
- consuming `AnalyzerSnapshot` and related result types
- using `sdr_signal_analyzer.export` to build JSONL exports from public snapshot objects

These are the safest places to build tooling around the repository because they are documented and already covered by regression tests.

## Review Rules

Before merging a change that touches a public contract, verify all of the following:

- docs and tests were updated together
- schema claims still match `python/sdr_signal_analyzer/export.py` and `src/cli/main.cpp`
- public/internal boundary wording is still honest
- no release document implies broader stability than the current alpha status supports

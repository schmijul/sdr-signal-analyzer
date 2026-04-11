# Contributing

Thanks for working on `sdr-signal-analyzer`.

## Before You Open A PR

- search existing issues and pull requests first
- describe the real user-visible problem, not just the code change
- include platform details, Python version, compiler, and SDR hardware details when the issue depends on them
- mention whether the report affects `simulator`, `replay`, `rtl_tcp`, `uhd`, or `soapy`
- prefer deterministic replay fixtures over live hardware when you can reproduce the problem locally

## Local Development Setup

Create a virtual environment and install the development toolchain:

```bash
python3 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -e ".[dev,gui]"
```

Build and test the backend:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Run the GUI from the source tree:

```bash
PYTHONPATH=python python -m sdr_signal_analyzer
```

Validate package metadata and distribution artifacts:

```bash
python3 scripts/release_validate.py --skip-install-smoke
```

`pyproject-build` is still used under the hood because this repository keeps CMake output in the top-level `build/` directory, but `scripts/release_validate.py` is the preferred maintainer entrypoint because it keeps the release checks repeatable.

If Docker is available and you want the fresh-Ubuntu install smoke too:

```bash
python3 scripts/release_validate.py
```

When a change affects behavior, follow test-driven development:

1. add or update the regression test first
2. implement the smallest change that makes the test pass
3. update docs when the user-facing behavior, install path, or workflow changes

Deterministic fixtures are preferred for signal-processing work. If a change depends on a capture, add a replayable fixture or a synthetic test case rather than relying on live SDR hardware.

## Formatting And Hooks

Install and enable pre-commit:

```bash
pre-commit install
pre-commit run --all-files
```

The default hooks cover:
- `black` for Python formatting
- `isort` for Python import ordering
- `clang-format` for C, C++, and header files
- basic whitespace, line-ending, and YAML validation hooks

The repository root contains `.clang-format`, so local formatting should match the hook and CI output instead of relying on editor defaults.

## Optional SDR Backends

The repository builds without UHD or SoapySDR.

If you are working on those backends:
- install the relevant SDK before configuring the project
- say explicitly which SDK and device you validated against
- keep the no-hardware developer path working unless the change is intentionally backend-specific
- include a replay or simulator path in your test plan whenever the live backend is not available in CI

## Pull Request Expectations

Every PR should:
- explain what changed and why
- mention how it was validated locally
- update docs when behavior, install steps, or supported workflows change
- add or extend tests when regression coverage is practical
- note whether the change touches a public API, a CLI command, or documentation only
- call out any known gaps, especially if a hardware-backed path could not be validated
- include the exact commands used for verification when practical
- keep diagnostics logs separate from JSONL measurement export when both are attached

For hardware-sensitive changes, include a short manual validation checklist in the PR description if full automation is not realistic.
When attached-device validation is prepared but not executed, say that explicitly and link the relevant checklist or template instead of implying success.

Useful maintainer and lab-prep docs:
- `docs/release.md`
- `docs/release_checklist.md`
- `docs/hardware_validation_plan.md`
- `docs/hardware_validation_status.md`
- `docs/hardware_validation_report_template.md`
- `docs/templates/hardware_validation_report_template.md`

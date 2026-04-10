# Contributing

Thanks for working on `sdr-signal-analyzer`.

## Before You Open A PR

- search existing issues and pull requests first
- describe the real user-visible problem, not just the code change
- include platform details, Python version, compiler, and SDR hardware details when the issue depends on them
- mention whether the report affects `simulator`, `replay`, `rtl_tcp`, `uhd`, or `soapy`

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
pyproject-build
twine check dist/*
```

`pyproject-build` is preferred over `python -m build` because this repository keeps CMake output in the top-level `build/` directory.

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

## Optional SDR Backends

The repository builds without UHD or SoapySDR.

If you are working on those backends:
- install the relevant SDK before configuring the project
- say explicitly which SDK and device you validated against
- keep the no-hardware developer path working unless the change is intentionally backend-specific

## Pull Request Expectations

Every PR should:
- explain what changed and why
- mention how it was validated locally
- update docs when behavior, install steps, or supported workflows change
- add or extend tests when regression coverage is practical

For hardware-sensitive changes, include a short manual validation checklist in the PR description if full automation is not realistic.

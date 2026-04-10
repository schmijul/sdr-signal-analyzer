# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project follows [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added
- Release automation for GitHub Releases, PyPI publishing, and GitHub Pages documentation deployment.
- Expanded project metadata, contributor guidance, and documentation landing pages.
- Pre-commit hooks for Python and C/C++ formatting.

## [0.1.0] - 2026-04-10

### Added
- C++ backend for capture orchestration, FFT analysis, detection, recording, and replay.
- Qt-based Python GUI backed by `pybind11` bindings.
- Built-in simulator, built-in `rtl_tcp` source, optional UHD/USRP backend, and optional SoapySDR backend.
- Raw `.bin` plus JSON sidecar recording and replay support.
- SigMF recording and replay support.
- Deterministic replay fixtures for regression coverage and documentation examples.

### Testing
- C++ regression tests for DSP behavior, recording/replay, and `rtl_tcp` transport handling.
- Python binding smoke coverage and offscreen GUI smoke coverage in CI.

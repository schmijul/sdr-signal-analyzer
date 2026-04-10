# Architecture

## Overview

The project is intentionally backend-first:
- C++ owns source access, DSP, detection, recording, replay, and session lifecycle
- Python is only a thin frontend layer over the public C++ API
- the GUI is optional and consumes the same session/config/result model as the CLI

The design goal is that the analyzer can be used:
- headless from the CLI
- embedded as a C++ library
- from Python through `pybind11`
- with the GUI only as an additional consumer

## Module Layout

- `include/sdr_analyzer/`
  - public API types and session surface
- `src/core/`
  - `AnalyzerSession` orchestration
  - worker thread lifecycle
  - snapshot queue management
- `src/sdr/`
  - concrete sample sources:
    - simulator
    - replay
    - `rtl_tcp`
    - UHD
    - optional SoapySDR
- `src/dsp/`
  - FFT
  - averaging
  - peak hold
  - peak detection
  - bandwidth and noise-floor estimation
  - coarse classification
- `src/io/`
  - metadata readers/writers
  - raw and SigMF recording support
- `src/cli/`
  - simple backend-only entrypoint
- `python/`
  - `pybind11` module
  - Qt GUI

## Data Flow

The data path for every live or replay source is the same:

1. A `SourceConfig` selects a source backend and its source-specific fields.
2. `AnalyzerSession::start()` constructs the chosen source through the source factory.
3. The source pushes IQ samples into the session worker thread.
4. The worker thread optionally mirrors samples into the recorder.
5. The DSP analyzer converts samples into:
   - spectrum frame data
   - time-domain frame data
   - analysis results
6. Snapshots are stored in a bounded queue.
7. CLI and GUI poll snapshots from that queue.

This is why replay, simulator, `rtl_tcp`, UHD, and Soapy all share the same downstream analysis behavior.

## Threading Model

- the session owns one worker thread when running
- the source and DSP pipeline execute on that worker thread
- the worker produces bounded snapshots for consumers
- GUI code only polls snapshots on the UI thread
- `stop()` joins the worker thread even when the source stops itself due to EOF or failure

Current lock strategy:
- the main session mutex protects desired config, marker state, lifecycle flags, and the bounded snapshot queue
- recorder writes are isolated behind a recorder mutex
- DSP analyzer state updates are isolated behind an analysis mutex
- blocking sample reads and FFT processing happen outside the main session mutex

Practical consequences:
- `poll_snapshot()` and `update_source_config()` no longer wait behind `ReadSamples()` or FFT work
- source config updates are staged on the caller thread and applied by the worker before a later read cycle
- EOF or source failure stops the worker, records `last_error()`, and allows a later `start()` to create a fresh worker cleanly
- `stop()` waits for the current read/process cycle to finish before the source is torn down, which keeps source ownership single-threaded

## Backend/Frontend Separation

The frontend does not own:
- source lifecycle
- device configuration logic
- FFT logic
- detection logic
- recording or replay logic

The frontend does own:
- control widgets
- presenting source-specific fields
- plotting and tables
- marker editing
- surfacing backend errors to the user

## Optional Backends

UHD and Soapy are optional at build time:
- if UHD is found, the native USRP backend is compiled in
- if SoapySDR is found, the generic Soapy backend is compiled in
- if either SDK is absent, the corresponding source kind returns a clear startup error instead of silently misbehaving

This keeps the default developer and CI path lightweight while still allowing richer hardware integrations on machines that have the right SDKs installed.

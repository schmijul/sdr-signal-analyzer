# Quality Evidence

This page maps the repository's public evidence to the commands that reproduce it.

## Verified Now

- deterministic C++ regression tests
- Python diagnostics/export/helper tests
- offscreen GUI smoke and screenshot generation
- mock-tested `rtl_tcp` transport behavior
- package build and install smoke
- strict docs build

## Evidence Map

Core regression:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected result:
- native C++ tests pass
- Python-driven tests registered by CTest pass

Python coverage:

```bash
python3 scripts/python_coverage.py --output-dir quality-artifacts/python-coverage
```

Expected artifacts:
- `quality-artifacts/python-coverage/html/index.html`
- `quality-artifacts/python-coverage/coverage.xml`

Benchmarks:

```bash
python3 scripts/benchmark_ci.py --cli ./build/sdr-analyzer-cli
```

Expected artifacts:
- `quality-artifacts/benchmark/benchmark.json`
- `quality-artifacts/benchmark/benchmark.md`

Install smoke:

```bash
docker run --rm -v "$PWD:/work" -w /work ubuntu:24.04 bash scripts/install_smoke_ubuntu.sh
```

Release validation bundle:

```bash
python3 scripts/release_validate.py --skip-install-smoke
```

Screenshot regeneration:

```bash
QT_QPA_PLATFORM=offscreen PYTHONPATH=python python scripts/generate_portfolio_assets.py --all --output-dir docs/screenshots
```

## Scope Limits

These artifacts improve trust, but they do not prove:
- live RF calibration
- measurement-grade behavior
- UHD validation on real USRP hardware
- SoapySDR validation on real hardware

Benchmark numbers are throughput and resource evidence, not RF validation.

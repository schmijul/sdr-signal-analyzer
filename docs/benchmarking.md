# Benchmarking

The benchmark harness is intentionally informational. It records whether the analyzer still behaves sanely under a fixed simulator workload and a fixed replay workload, but it does not try to enforce a hard performance gate in CI.

## What It Measures

The CI benchmark script records:

- wall-clock time
- user CPU time
- system CPU time
- combined CPU usage
- peak resident set size
- observed frame count
- derived frames per second

The two scenarios are:

- `simulator`, which exercises the deterministic built-in signal generator
- `replay`, which exercises fixture-backed replay with `--loop`

## How To Run It Locally

Build the CLI first, then run the benchmark script:

```bash
cmake -S . -B build
cmake --build build
python3 scripts/benchmark_ci.py --cli ./build/sdr-analyzer-cli
```

Expected outputs:

- `quality-artifacts/benchmark/benchmark.json`
- `quality-artifacts/benchmark/benchmark.md`

## How To Read The Output

The JSON artifact is the canonical record. It is useful for archiving raw numbers and comparing runs. The markdown artifact is a human-readable summary that is easy to inspect in CI.

The numbers are only comparable when the runner class, build mode, and fixture set are the same. Treat them as regression evidence, not as an absolute performance guarantee.

## What Not To Infer

Do not interpret the benchmark as:

- a profiling substitute
- a latency guarantee
- a measurement of RF performance
- a hard CI acceptance threshold

The benchmark is there to catch obviously broken throughput, replay, or memory behavior early.

# rtl_tcp Source Guide

## Purpose

Use the built-in `rtl_tcp` source when you want live streaming from an RTL-SDR-style remote server without depending on SoapySDR.

This path is implemented directly in the repo and is covered by a mock-server regression test.

Stability note:
- the code path is verified
- real hardware/network deployments still depend on the remote server implementation and transport quality

## CLI Example

```bash
./build/sdr-analyzer-cli \
  --source rtl_tcp \
  --host 127.0.0.1 \
  --port 1234 \
  --center-hz 100000000 \
  --sample-rate 2400000 \
  --gain-db 20
```

## GUI Use

In the GUI:
- choose `rtl_tcp`
- set host and port
- use the shared center-frequency, sample-rate, and gain controls

## Behavior

The source:
- connects to the remote server
- reads the initial `rtl_tcp` header
- sends frequency/sample-rate/gain commands
- converts unsigned IQ8 network samples into normalized complex float samples
- forwards them into the shared DSP path

## Operational Notes

- sample format from `rtl_tcp` is IQ8, so it is lower fidelity than the replay and UHD float paths
- gain handling depends on the remote server implementation
- disconnects are treated as source failures and surfaced through `last_error()`

## Common Failure Modes

- wrong host or port
- server not running
- socket disconnect during streaming
- timeout while waiting for samples

## Validation Checklist

1. Confirm the server is reachable.
2. Start a session and confirm snapshots arrive.
3. Tune center frequency and verify spectral movement.
4. Disconnect the server and confirm the session stops with an error.

Expected outcome:
- a successful connection produces spectrum snapshots
- a disconnect turns into an actionable `last_error()` message instead of silent failure

# Troubleshooting

This page collects the failure modes that are most likely to block a first run.

## Build Failures

### `cmake` cannot find Python or `pybind11`

Use a Python environment that has the development dependencies installed:

```bash
python3 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -e ".[gui]"
```

If you are building from source outside a venv, make sure the system Python development headers are installed.

### Qt or GUI dependencies are missing

Install the GUI extra:

```bash
python -m pip install -e ".[gui]"
```

If the GUI still fails to start, confirm that `PySide6` is installed in the active environment and that `QT_QPA_PLATFORM=offscreen` is set for headless smoke tests.

## Missing Dependencies

### `python` is not available

Use `python3` to create the virtual environment and then use the venv interpreter:

```bash
python3 -m venv .venv
. .venv/bin/activate
python -m pip install -e ".[gui]"
```

### `mkdocs` is not installed

Install the docs extras in a virtual environment:

```bash
python3 -m venv .venv
. .venv/bin/activate
python -m pip install mkdocs mkdocs-material
```

## Device Not Found

### `rtl_tcp`

Check host, port, and server availability:

```bash
./build/sdr-analyzer-cli --source rtl_tcp --host 127.0.0.1 --port 1234
```

If the session exits immediately, confirm that the remote server is running and that the chosen port matches the server configuration.

### UHD

Confirm the USB or network-connected device is visible to UHD and that the `--device-args` string matches the attached radio.

### SoapySDR

Confirm the device driver is installed and the `--device-string` or `--device-args` value matches the backend you intend to use.

## GUI Issues

### The GUI opens but does not show live data

Check that the source is running and that the selected source kind is correct. For a first smoke test, use the simulator workflow before trying a live device.

### Headless validation fails

Set:

```bash
QT_QPA_PLATFORM=offscreen
```

This is required for CI-style GUI validation on machines without a display server.

## Connection Errors

### `rtl_tcp` disconnects

Disconnects are treated as source failures. The session should stop and surface the reason through `last_error()`.

### Replay ends early

Replay files stop at end of stream by design. For repeated playback, use a looping source configuration or replay a longer capture.

## Fast Recovery Workflow

When something is unclear, use the stable sequence:
1. `simulator`
2. `replay`
3. `rtl_tcp`
4. live hardware backends

That isolates configuration problems before you involve hardware and the network.

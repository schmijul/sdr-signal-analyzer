# Source Guides

Use these pages when you need backend-specific behavior or failure-mode details.

## Stable Order

The recommended path is:
1. simulator
2. replay
3. `rtl_tcp`
4. UHD / SoapySDR

## Available Backends

- [`rtl_tcp`](rtltcp.md)
- [`UHD / USRP`](uhd.md)

Common split:
- `simulator`, `replay`, and `rtl_tcp` are available in the default repository build
- `uhd` and `soapy` are optional integrations that require their SDKs at configure time
- only simulator, replay, and `rtl_tcp` have direct repo-level validation today

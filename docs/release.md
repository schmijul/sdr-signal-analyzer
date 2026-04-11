# Release Readiness

This page is the maintainer-facing release path for `sdr-signal-analyzer`.

Status labels used here:
- `Verified now`: exercised in the repository today without SDR hardware.
- `Prepared for validation`: documented and scripted, but not exercised in this session.
- `Pending lab validation`: requires attached SDR hardware or a lab session before the claim can be upgraded.

## Verified Now

The following release checks are reproducible without hardware:
- native configure, build, and `ctest`
- Python demo and CLI export regression checks
- MkDocs strict build
- source distribution and wheel build
- `twine check` package metadata validation
- fresh-Ubuntu install smoke through `scripts/install_smoke_ubuntu.sh`

Primary local command:

```bash
python3 scripts/release_validate.py --skip-install-smoke
```

Full local command, including the Docker-based install smoke:

```bash
python3 scripts/release_validate.py
```

Expected outputs:
- a successful `ctest` run from `build-release`
- a successful `mkdocs build --strict`
- wheel and sdist artifacts under `dist/`
- a successful `twine check dist/*`
- `release-validation-ok` at the end of the run
- release artifacts and docs wording remain aligned with [Release Checklist](release_checklist.md)
- API and schema wording remain aligned with [Public API](api.md) and [API And Schema Stability](stability.md)

## Maintainer Checklist

1. Confirm the working tree is intentional and the target version is correct in `pyproject.toml` and `CMakeLists.txt`.
2. Review the repository changelog and move release-ready notes out of `Unreleased`.
3. Run `python3 scripts/release_validate.py --skip-install-smoke`.
4. If Docker is available, run `python3 scripts/release_validate.py --skip-native --skip-python-tests --skip-docs --skip-package` only when you specifically need to re-run the fresh-install smoke in isolation.
5. Build artifacts locally and inspect `dist/*`.
6. Verify documentation pages and README links render locally with `mkdocs build --strict`.
7. Review public API and schema impact:
   changes under `include/sdr_analyzer/`, `python/sdr_signal_analyzer/__init__.py`, `python/sdr_signal_analyzer/export.py`, or the measurement-export logic in `src/cli/main.cpp` require explicit docs and regression-test review.
   If the CLI/common JSONL schema changes incompatibly, `format_version` must change as well.
8. Check the backend status wording:
   `simulator`, `replay`, and `rtl_tcp` may be described as verified code paths only where repo evidence exists.
   `uhd` and `soapy` must remain `Prepared for validation` / `Pending lab validation` until real lab reports exist.
9. Tag the release as `vX.Y.Z`.
10. Confirm the GitHub Actions `Release` workflow uploads the sdist and wheels and that PyPI publishing succeeds.
11. Verify the GitHub Release notes reflect the changelog and do not overstate hardware validation, API stability, or schema compatibility.

Use the checkbox-driven companion page for sign-off:
- [Release Checklist](release_checklist.md)

## Versioning Rules

- `pyproject.toml` and `CMakeLists.txt` must carry the same project version.
- `CHANGELOG.md` remains Keep-a-Changelog style with an `Unreleased` section at the top.
- Tag names use a `v` prefix such as `v0.1.0`.
- Hardware readiness wording is release data, not marketing copy. Do not move `uhd` or `soapy` out of pending status without attached-hardware evidence.
- JSONL measurement compatibility is governed by `format_version`. Do not ship an incompatible CLI/common schema change inside `sdr_signal_analyzer.measurements.v1`.

## Artifact Validation

Local artifact checks:

```bash
rm -rf dist build-release
python3 scripts/release_validate.py --skip-install-smoke
python -m pip install --force-reinstall dist/*.whl
python -m sdr_signal_analyzer.demo --frames 2
sdr-signal-analyzer-demo --frames 2
```

Expected result:
- the installed wheel imports and runs the simulator-backed demo
- no source-tree-only paths are required for the installed wheel smoke

## What Is Still Pending

- `uhd` live-device validation on attached hardware
- `soapy` live-device validation on attached hardware
- release notes backed by lab reports for those backends

Those items are intentionally blocked on later lab work. See [Hardware Validation Plan](hardware_validation_plan.md) and [Hardware Validation Status](hardware_validation_status.md).

# Release Checklist

Use this checklist when cutting a public release.

## Versioning

- [ ] Update `pyproject.toml` version.
- [ ] Update `CMakeLists.txt` project version.
- [ ] Move release notes from `CHANGELOG.md` `Unreleased` into a dated section.
- [ ] Confirm the README does not claim new hardware validation that has not happened.

## Deterministic Validation

- [ ] Run `python3 scripts/release_validate.py`.
- [ ] Optionally run `python3 scripts/benchmark_ci.py --cli ./build/sdr-analyzer-cli` when you want a fresh benchmark artifact refresh.
- [ ] Inspect `ctest` output for failures or skips that need release notes.
- [ ] Inspect `quality-artifacts/benchmark/benchmark.json` and `benchmark.md`.
- [ ] Confirm `mkdocs build --strict` passed.

## Docs And Links

- [ ] Run the local link check command from [release.md](release.md).
- [ ] Confirm new docs pages are present in `mkdocs.yml`.
- [ ] Confirm README links match the docs tree.
- [ ] Confirm troubleshooting, testing, workflows, and release docs agree on commands.

## Packaging

- [ ] Confirm `dist/*.tar.gz` and `dist/*.whl` were rebuilt for the release commit.
- [ ] Run `twine check dist/*`.
- [ ] Install the wheel into a clean venv and run the demo smoke.

## Installability

- [ ] Run `docker run --rm -v "$PWD:/work" -w /work ubuntu:24.04 bash scripts/install_smoke_ubuntu.sh`.
- [ ] Confirm editable install succeeds.
- [ ] Confirm built-wheel install succeeds.

## Hardware Truthfulness

- [ ] Confirm UHD status is still labeled `Pending lab validation` unless a new lab report is committed.
- [ ] Confirm SoapySDR status is still labeled `Pending lab validation` unless a new lab report is committed.
- [ ] If hardware reports are included, verify they record operator, device, driver, commands, logs, and export artifacts.

## Publishing

- [ ] Create and push an annotated tag `vX.Y.Z`.
- [ ] Confirm the GitHub Release workflow finishes successfully.
- [ ] Confirm GitHub Release assets were attached.
- [ ] Confirm PyPI publish completed.
- [ ] Confirm GitHub Pages docs remain reachable after the release.

## Post-Release

- [ ] Reopen `CHANGELOG.md` with a fresh `Unreleased` section.
- [ ] Note any lab-validation work still pending before the next release.

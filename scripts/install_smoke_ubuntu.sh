#!/usr/bin/env bash

set -euo pipefail

export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get install -y --no-install-recommends \
  ca-certificates \
  cmake \
  g++ \
  git \
  libegl1 \
  libxcb-cursor0 \
  libxkbcommon-x11-0 \
  ninja-build \
  python3 \
  python3-dev \
  python3-pip \
  python3-venv

python3 -m venv /tmp/sdrsa-ci-venv
. /tmp/sdrsa-ci-venv/bin/activate

python -m pip install --upgrade pip
python -m pip install build pybind11 scikit-build-core twine PySide6

cmake -S . -B build-install -G Ninja \
  -DSDR_ANALYZER_ENABLE_SOAPYSDR=OFF \
  -DSDR_ANALYZER_ENABLE_UHD=OFF
cmake --build build-install
ctest --test-dir build-install --output-on-failure

python -m pip install -e ".[gui]"
python -m sdr_signal_analyzer.demo --frames 4
sdr-signal-analyzer-demo --frames 4

pyproject-build
twine check dist/*
python -m pip install --force-reinstall dist/*.whl
python -m sdr_signal_analyzer.demo --frames 4
sdr-signal-analyzer-demo --frames 4

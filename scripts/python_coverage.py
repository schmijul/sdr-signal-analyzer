#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import shutil
import subprocess
import sys
from pathlib import Path


def _run(command: list[str], *, cwd: Path, env: dict[str, str]) -> None:
    print("+", " ".join(command), flush=True)
    subprocess.run(command, cwd=cwd, env=env, check=True)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Collect Python coverage for the analyzer's deterministic paths."
    )
    parser.add_argument(
        "--output-dir",
        default="quality-artifacts/python-coverage",
        help="Directory for the coverage HTML and XML reports.",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    output_dir = (repo_root / args.output_dir).resolve()
    html_dir = output_dir / "html"
    xml_path = output_dir / "coverage.xml"

    env = os.environ.copy()
    python_path = str(repo_root / "python")
    env["PYTHONPATH"] = (
        python_path
        if "PYTHONPATH" not in env or not env["PYTHONPATH"]
        else python_path + os.pathsep + env["PYTHONPATH"]
    )
    env.setdefault("QT_QPA_PLATFORM", "offscreen")

    output_dir.mkdir(parents=True, exist_ok=True)
    coverage_bin = shutil.which("coverage")
    coverage_cmd = [coverage_bin] if coverage_bin else [sys.executable, "-m", "coverage"]
    build_dir = repo_root / "build-coverage"
    _run(
        [
            "cmake",
            "-S",
            ".",
            "-B",
            str(build_dir),
            "-G",
            "Ninja",
            "-DSDR_ANALYZER_ENABLE_SOAPYSDR=OFF",
            "-DSDR_ANALYZER_ENABLE_UHD=OFF",
        ],
        cwd=repo_root,
        env=env,
    )
    _run(
        [
            "cmake",
            "--build",
            str(build_dir),
            "--target",
            "_sdr_signal_analyzer",
        ],
        cwd=repo_root,
        env=env,
    )
    _run([*coverage_cmd, "erase"], cwd=repo_root, env=env)
    _run(
        [*coverage_cmd, "run", "--parallel-mode", "tests/test_gui_validation.py"],
        cwd=repo_root,
        env=env,
    )
    _run(
        [*coverage_cmd, "run", "--parallel-mode", "examples/run_simulator.py"],
        cwd=repo_root,
        env=env,
    )
    _run([*coverage_cmd, "combine"], cwd=repo_root, env=env)
    package_include = "python/sdr_signal_analyzer/*"
    _run(
        [*coverage_cmd, "html", "--include", package_include, "-d", str(html_dir)],
        cwd=repo_root,
        env=env,
    )
    _run(
        [*coverage_cmd, "xml", "--include", package_include, "-o", str(xml_path)],
        cwd=repo_root,
        env=env,
    )
    _run(
        [*coverage_cmd, "report", "--include", package_include, "--show-missing"],
        cwd=repo_root,
        env=env,
    )

    print(f"coverage_html={html_dir}")
    print(f"coverage_xml={xml_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

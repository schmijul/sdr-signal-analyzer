#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
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
    _run(["coverage", "erase"], cwd=repo_root, env=env)
    _run(
        ["coverage", "run", "--parallel-mode", "tests/test_gui_validation.py"],
        cwd=repo_root,
        env=env,
    )
    _run(
        ["coverage", "run", "--parallel-mode", "examples/run_simulator.py"],
        cwd=repo_root,
        env=env,
    )
    _run(["coverage", "combine"], cwd=repo_root, env=env)
    package_include = "python/sdr_signal_analyzer/*"
    _run(
        ["coverage", "html", "--include", package_include, "-d", str(html_dir)],
        cwd=repo_root,
        env=env,
    )
    _run(
        ["coverage", "xml", "--include", package_include, "-o", str(xml_path)],
        cwd=repo_root,
        env=env,
    )
    _run(
        ["coverage", "report", "--include", package_include, "--show-missing"],
        cwd=repo_root,
        env=env,
    )

    print(f"coverage_html={html_dir}")
    print(f"coverage_xml={xml_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

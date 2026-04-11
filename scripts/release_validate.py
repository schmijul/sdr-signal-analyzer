#!/usr/bin/env python3
from __future__ import annotations

import argparse
import os
import shlex
import shutil
import subprocess
import sys
from pathlib import Path


def _command_label(command: list[str]) -> str:
    return " ".join(shlex.quote(part) for part in command)


def _run(
    command: list[str], *, cwd: Path, dry_run: bool, env: dict[str, str] | None = None
) -> None:
    print(f"+ {_command_label(command)}")
    if dry_run:
        return
    subprocess.run(command, cwd=cwd, check=True, env=env)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run the repository's repeatable release-validation steps."
    )
    parser.add_argument("--build-dir", default="build-release")
    parser.add_argument("--artifact-dir", default="dist")
    parser.add_argument("--skip-native", action="store_true")
    parser.add_argument("--skip-python-tests", action="store_true")
    parser.add_argument("--skip-docs", action="store_true")
    parser.add_argument("--skip-package", action="store_true")
    parser.add_argument("--skip-install-smoke", action="store_true")
    parser.add_argument("--with-benchmark", action="store_true")
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    build_dir = repo_root / args.build_dir
    artifact_dir = repo_root / args.artifact_dir
    env = os.environ.copy()
    python_path = str(repo_root / "python")
    env["PYTHONPATH"] = (
        python_path
        if not env.get("PYTHONPATH")
        else python_path + os.pathsep + env["PYTHONPATH"]
    )
    env.setdefault("QT_QPA_PLATFORM", "offscreen")
    env["SDR_ANALYZER_CLI"] = str(build_dir / "sdr-analyzer-cli")

    if not args.skip_native:
        _run(
            [
                "cmake",
                "-S",
                ".",
                "-B",
                str(build_dir),
                "-DSDR_ANALYZER_ENABLE_SOAPYSDR=OFF",
                "-DSDR_ANALYZER_ENABLE_UHD=OFF",
            ],
            cwd=repo_root,
            dry_run=args.dry_run,
            env=env,
        )
        _run(
            ["cmake", "--build", str(build_dir)],
            cwd=repo_root,
            dry_run=args.dry_run,
            env=env,
        )
        _run(
            ["ctest", "--test-dir", str(build_dir), "--output-on-failure"],
            cwd=repo_root,
            dry_run=args.dry_run,
            env=env,
        )

    if not args.skip_python_tests:
        _run(
            [sys.executable, "-m", "sdr_signal_analyzer.demo", "--frames", "2"],
            cwd=repo_root,
            dry_run=args.dry_run,
            env=env,
        )
        _run(
            [sys.executable, "tests/test_cli_export.py"],
            cwd=repo_root,
            dry_run=args.dry_run,
            env=env,
        )
        _run(
            [sys.executable, "tests/test_diagnostics.py"],
            cwd=repo_root,
            dry_run=args.dry_run,
            env=env,
        )

    if not args.skip_docs:
        _run(
            [sys.executable, "-m", "mkdocs", "build", "--strict"],
            cwd=repo_root,
            dry_run=args.dry_run,
            env=env,
        )

    if not args.skip_package:
        if artifact_dir.exists() and not args.dry_run:
            shutil.rmtree(artifact_dir)
        _run(
            [sys.executable, "-m", "pip", "install", "build", "twine"],
            cwd=repo_root,
            dry_run=args.dry_run,
            env=env,
        )
        pyproject_build = Path(sys.executable).with_name("pyproject-build")
        if args.dry_run:
            package_command = [str(pyproject_build)]
        else:
            package_command = (
                [str(pyproject_build)]
                if pyproject_build.exists()
                else [sys.executable, "-m", "build"]
            )
        _run(package_command, cwd=repo_root, dry_run=args.dry_run, env=env)
        if args.dry_run:
            _run(
                [sys.executable, "-m", "twine", "check", f"{artifact_dir}/*"],
                cwd=repo_root,
                dry_run=True,
                env=env,
            )
        else:
            artifact_paths = sorted(str(path) for path in artifact_dir.glob("*"))
            if not artifact_paths:
                raise SystemExit("No built artifacts were found under dist/.")
            _run(
                [sys.executable, "-m", "twine", "check", *artifact_paths],
                cwd=repo_root,
                dry_run=False,
                env=env,
            )

    if args.with_benchmark:
        _run(
            [
                sys.executable,
                "scripts/benchmark_ci.py",
                "--cli",
                str(build_dir / "sdr-analyzer-cli"),
            ],
            cwd=repo_root,
            dry_run=args.dry_run,
            env=env,
        )

    if not args.skip_install_smoke:
        docker = shutil.which("docker")
        if docker is None:
            raise SystemExit(
                "Docker is required for install smoke. Use --skip-install-smoke if unavailable."
            )
        _run(
            [
                docker,
                "run",
                "--rm",
                "-v",
                f"{repo_root}:/work",
                "-w",
                "/work",
                "ubuntu:24.04",
                "bash",
                "scripts/install_smoke_ubuntu.sh",
            ],
            cwd=repo_root,
            dry_run=args.dry_run,
            env=env,
        )

    print("release-validation-ok")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path


def _candidate_paths() -> list[Path]:
    package_dir = Path(__file__).resolve().parent
    exe_name = "sdr-analyzer-cli.exe" if os.name == "nt" else "sdr-analyzer-cli"
    env_path = os.environ.get("SDR_ANALYZER_CLI")
    candidates: list[Path] = []
    if env_path:
        candidates.append(Path(env_path))
    candidates.extend(
        [
            package_dir.parent / "bin" / exe_name,
            package_dir.parent.parent / "build" / exe_name,
        ]
    )
    return candidates


def _resolve_cli_binary() -> Path:
    for candidate in _candidate_paths():
        if candidate.is_file():
            return candidate
    checked = "\n".join(f"- {path}" for path in _candidate_paths())
    raise SystemExit(
        "Could not locate the native sdr-analyzer-cli binary.\n"
        "Checked:\n"
        f"{checked}"
    )


def main(argv: list[str] | None = None) -> int:
    binary = _resolve_cli_binary()
    completed = subprocess.run([str(binary), *(argv or sys.argv[1:])], check=False)
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())

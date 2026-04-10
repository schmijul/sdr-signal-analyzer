#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
import os
import platform
import re
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


FRAME_RE = re.compile(r"^frame=(\d+)\b", re.MULTILINE)


def _time_command(command: list[str], *, cwd: Path, env: dict[str, str]) -> dict[str, float | int | str]:
    timer = shutil.which("time") or "/usr/bin/time"
    if not Path(timer).exists():
        raise RuntimeError("The GNU time command was not found.")

    timing_format = "MAX_RSS_KB=%M\nUSER_SECONDS=%U\nSYS_SECONDS=%S"
    start = time.perf_counter()
    completed = subprocess.run(
        [timer, "-f", timing_format, *command],
        cwd=cwd,
        env=env,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    elapsed = time.perf_counter() - start
    parsed: dict[str, float | int | str] = {
        "command": " ".join(command),
        "returncode": completed.returncode,
        "stdout": completed.stdout,
        "stderr": completed.stderr,
        "wall_seconds": elapsed,
    }
    for line in completed.stderr.splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        if key == "MAX_RSS_KB":
            parsed[key.lower()] = int(value)
        else:
            parsed[key.lower()] = float(value)
    parsed.setdefault("max_rss_kb", 0)
    parsed.setdefault("user_seconds", 0.0)
    parsed.setdefault("sys_seconds", 0.0)
    return parsed


def _run_benchmark(
    *,
    name: str,
    command: list[str],
    cwd: Path,
    env: dict[str, str],
    expected_frames: int,
) -> dict[str, object]:
    result = _time_command(command, cwd=cwd, env=env)
    stdout = str(result["stdout"])
    frames = len(FRAME_RE.findall(stdout))
    user_seconds = float(result["user_seconds"])
    sys_seconds = float(result["sys_seconds"])
    cpu_seconds = user_seconds + sys_seconds
    wall_seconds = float(result["wall_seconds"])
    cpu_percent = 0.0 if wall_seconds <= 0 else (cpu_seconds / wall_seconds) * 100.0

    summary = {
        "name": name,
        "command": result["command"],
        "returncode": result["returncode"],
        "frames_expected": expected_frames,
        "frames_observed": frames,
        "wall_seconds": round(wall_seconds, 6),
        "user_seconds": round(user_seconds, 6),
        "sys_seconds": round(sys_seconds, 6),
        "cpu_seconds": round(cpu_seconds, 6),
        "cpu_percent": round(cpu_percent, 2),
        "max_rss_kb": int(result["max_rss_kb"]),
    }
    return summary


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Run informational simulator and replay benchmarks for CI."
    )
    parser.add_argument("--cli", required=True, help="Path to the sdr-analyzer-cli binary.")
    parser.add_argument(
        "--output-dir",
        default="quality-artifacts/benchmark",
        help="Directory for benchmark JSON and markdown outputs.",
    )
    parser.add_argument("--simulator-frames", type=int, default=200)
    parser.add_argument("--replay-frames", type=int, default=200)
    parser.add_argument(
        "--replay-input",
        default="tests/fixtures/tone_cf32.sigmf-data",
        help="Replay capture data file.",
    )
    parser.add_argument(
        "--replay-meta",
        default="tests/fixtures/tone_cf32.sigmf-meta",
        help="Replay metadata file.",
    )
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    output_dir = (repo_root / args.output_dir).resolve()
    output_dir.mkdir(parents=True, exist_ok=True)
    env = os.environ.copy()
    env.setdefault("LC_ALL", "C")
    env.setdefault("LANG", "C")

    cli_path = str((repo_root / args.cli).resolve()) if not Path(args.cli).is_absolute() else args.cli
    benchmark_data = {
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "platform": platform.platform(),
        "python": sys.version.split()[0],
        "simulator": _run_benchmark(
            name="simulator",
            command=[cli_path, "--source", "simulator", "--frames", str(args.simulator_frames)],
            cwd=repo_root,
            env=env,
            expected_frames=args.simulator_frames,
        ),
        "replay": _run_benchmark(
            name="replay",
            command=[
                cli_path,
                "--source",
                "replay",
                "--input",
                str((repo_root / args.replay_input).resolve()),
                "--meta",
                str((repo_root / args.replay_meta).resolve()),
                "--loop",
                "--frames",
                str(args.replay_frames),
            ],
            cwd=repo_root,
            env=env,
            expected_frames=args.replay_frames,
        ),
    }

    json_path = output_dir / "benchmark.json"
    md_path = output_dir / "benchmark.md"
    json_path.write_text(json.dumps(benchmark_data, indent=2, sort_keys=True) + "\n")

    md_lines = [
        "# CI Benchmark Summary",
        "",
        f"Generated at: {benchmark_data['generated_at_utc']}",
        f"Platform: {benchmark_data['platform']}",
        f"Python: {benchmark_data['python']}",
        "",
        "| Scenario | Frames | Wall s | Frames/s | CPU % | Max RSS KB |",
        "| --- | ---: | ---: | ---: | ---: | ---: |",
    ]
    for key in ("simulator", "replay"):
        result = benchmark_data[key]
        wall_seconds = float(result["wall_seconds"])
        frames_observed = int(result["frames_observed"])
        frames_per_second = 0.0 if wall_seconds <= 0 else frames_observed / wall_seconds
        md_lines.append(
            "| {name} | {frames_observed} | {wall_seconds:.3f} | {fps:.1f} | {cpu_percent:.1f} | {max_rss_kb} |".format(
                name=result["name"],
                frames_observed=frames_observed,
                wall_seconds=wall_seconds,
                fps=frames_per_second,
                cpu_percent=float(result["cpu_percent"]),
                max_rss_kb=int(result["max_rss_kb"]),
            )
        )

    md_lines.append("")
    md_lines.append("This benchmark is informational only. Compare runs on the same runner class and build mode.")
    md_path.write_text("\n".join(md_lines) + "\n")

    print(f"benchmark_json={json_path}")
    print(f"benchmark_md={md_path}")

    failures = [
        key
        for key in ("simulator", "replay")
        if int(benchmark_data[key]["returncode"]) != 0
        or int(benchmark_data[key]["frames_observed"]) != int(benchmark_data[key]["frames_expected"])
    ]
    if failures:
        print(f"benchmark failures: {', '.join(failures)}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

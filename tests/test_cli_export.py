#!/usr/bin/env python3

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

from export_schema import (
    DETECTION_KEYS,
    FRAME_KEYS,
    FORMAT_VERSION,
    MARKER_MEASUREMENT_KEYS,
    METADATA_KEYS,
)


def cli_binary() -> Path:
    from_env = os.environ.get("SDR_ANALYZER_CLI")
    if from_env:
        return Path(from_env)
    root = Path(__file__).resolve().parents[1]
    return root / "build" / "sdr-analyzer-cli"


def fixture_path(name: str) -> Path:
    return Path(__file__).resolve().parent / "fixtures" / name


def load_jsonl(path: Path) -> list[dict]:
    with path.open("r", encoding="utf-8") as handle:
        return [json.loads(line) for line in handle if line.strip()]


class CliExportTests(unittest.TestCase):
    def run_cli(self, *args: str) -> subprocess.CompletedProcess[str]:
        return subprocess.run(
            [str(cli_binary()), *args],
            capture_output=True,
            text=True,
            check=False,
        )

    def test_simulator_export_creates_non_empty_jsonl(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            output = Path(tempdir) / "simulator.jsonl"
            completed = self.run_cli(
                "--source",
                "simulator",
                "--frames",
                "4",
                "--export-jsonl",
                str(output),
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            records = load_jsonl(output)
            self.assertGreaterEqual(len(records), 2)
            self.assertEqual(set(records[0].keys()), METADATA_KEYS)
            self.assertEqual(records[0]["record_type"], "metadata")
            self.assertEqual(records[0]["format_version"], FORMAT_VERSION)
            self.assertNotIn("session_metadata", records[0])
            for frame in records[1:]:
                self.assertEqual(set(frame.keys()), FRAME_KEYS)
                self.assertEqual(frame["record_type"], "frame")
                self.assertEqual(frame["detection_count"], len(frame["detections"]))
                for detection in frame["detections"]:
                    self.assertEqual(set(detection.keys()), DETECTION_KEYS)
                for marker in frame["marker_measurements"]:
                    self.assertEqual(set(marker.keys()), MARKER_MEASUREMENT_KEYS)

    def test_replay_export_creates_deterministic_frame_structure(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            output = Path(tempdir) / "replay.jsonl"
            completed = self.run_cli(
                "--source",
                "replay",
                "--input",
                str(fixture_path("tone_cf32.sigmf-data")),
                "--meta",
                str(fixture_path("tone_cf32.sigmf-meta")),
                "--frames",
                "3",
                "--export-jsonl",
                str(output),
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            records = load_jsonl(output)
            self.assertEqual(records[0]["source_config"]["kind"], "replay")
            self.assertEqual(records[0]["record_type"], "metadata")
            self.assertEqual(records[0]["format_version"], FORMAT_VERSION)
            self.assertNotIn("session_metadata", records[0])
            self.assertGreaterEqual(len(records[1:]), 1)
            sequences = [frame["sequence"] for frame in records[1:]]
            self.assertEqual(sequences, sorted(sequences))
            self.assertTrue(all(frame["record_type"] == "frame" for frame in records[1:]))
            self.assertTrue(all(set(frame.keys()) == FRAME_KEYS for frame in records[1:]))

    def test_export_interval_reduces_frame_count(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            full_output = Path(tempdir) / "full.jsonl"
            sparse_output = Path(tempdir) / "sparse.jsonl"

            full = self.run_cli(
                "--source",
                "simulator",
                "--frames",
                "6",
                "--export-jsonl",
                str(full_output),
            )
            sparse = self.run_cli(
                "--source",
                "simulator",
                "--frames",
                "6",
                "--export-jsonl",
                str(sparse_output),
                "--export-interval",
                "2",
            )

            self.assertEqual(full.returncode, 0, full.stderr)
            self.assertEqual(sparse.returncode, 0, sparse.stderr)

            full_frames = load_jsonl(full_output)[1:]
            sparse_frames = load_jsonl(sparse_output)[1:]
            expected_sequences = [
                frame["sequence"] for frame in full_frames if frame["sequence"] % 2 == 0
            ]
            self.assertEqual(
                [frame["sequence"] for frame in sparse_frames], expected_sequences
            )

    def test_invalid_export_path_fails_with_actionable_error(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            output = Path(tempdir) / "missing" / "dir" / "export.jsonl"
            completed = self.run_cli(
                "--source",
                "simulator",
                "--frames",
                "2",
                "--export-jsonl",
                str(output),
            )
            self.assertNotEqual(completed.returncode, 0)
            self.assertIn("Failed to open export file", completed.stderr)

    def test_invalid_interval_fails_fast(self) -> None:
        completed = self.run_cli(
            "--source",
            "simulator",
            "--frames",
            "2",
            "--export-jsonl",
            "ignored.jsonl",
            "--export-interval",
            "0",
        )
        self.assertNotEqual(completed.returncode, 0)
        self.assertIn("--export-interval must be at least 1", completed.stderr)

    def test_log_file_receives_human_diagnostics(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            log_path = Path(tempdir) / "run.log"
            completed = self.run_cli(
                "--source",
                "simulator",
                "--frames",
                "2",
                "--log-level",
                "info",
                "--log-file",
                str(log_path),
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            text = log_path.read_text(encoding="utf-8")
            self.assertIn("session.started", text)
            self.assertIn("session=", text)
            self.assertIn("source=simulator", text)

    def test_log_json_emits_structured_diagnostics(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            log_path = Path(tempdir) / "run.jsonl"
            completed = self.run_cli(
                "--source",
                "replay",
                "--input",
                str(fixture_path("tone_cf32.sigmf-data")),
                "--meta",
                str(fixture_path("tone_cf32.sigmf-meta")),
                "--frames",
                "2",
                "--log-level",
                "info",
                "--log-file",
                str(log_path),
                "--log-json",
            )
            self.assertEqual(completed.returncode, 0, completed.stderr)
            records = load_jsonl(log_path)
            self.assertTrue(records)
            self.assertTrue(all(record["record_type"] == "diagnostic" for record in records))
            self.assertTrue(all(record["source_kind"] == "replay" for record in records))
            self.assertTrue(all("session_id" in record for record in records))

    def test_log_file_captures_structured_start_failure(self) -> None:
        with tempfile.TemporaryDirectory() as tempdir:
            log_path = Path(tempdir) / "diagnostics.jsonl"
            completed = self.run_cli(
                "--source",
                "replay",
                "--log-file",
                str(log_path),
                "--log-json",
                "--log-level",
                "error",
            )
            self.assertNotEqual(completed.returncode, 0)
            self.assertTrue(log_path.exists())
            records = load_jsonl(log_path)
            self.assertGreaterEqual(len(records), 1)
            self.assertTrue(all(record["record_type"] == "diagnostic" for record in records))
            self.assertEqual(records[-1]["level"], "error")
            self.assertEqual(records[-1]["source_kind"], "replay")
            self.assertEqual(records[-1]["code"], "source.configure_failed")
            self.assertIn("Replay source requires an input file", records[-1]["message"])


if __name__ == "__main__":
    unittest.main()

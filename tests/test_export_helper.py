#!/usr/bin/env python3

from __future__ import annotations

import io
import json
import tempfile
import time
import unittest
from datetime import datetime, timedelta, timezone

from export_schema import (
    DETECTION_KEYS,
    FRAME_KEYS,
    MARKER_MEASUREMENT_KEYS,
    METADATA_KEYS,
)
from sdr_signal_analyzer import (
    AnalyzerSession,
    FORMAT_VERSION,
    Marker,
    ProcessingConfig,
    SourceConfig,
    SourceKind,
)
from sdr_signal_analyzer.export import JsonlMeasurementExporter, export_jsonl


def wait_for_snapshot(session: AnalyzerSession):
    deadline = time.monotonic() + 2.0
    while time.monotonic() < deadline:
        snapshot = session.poll_snapshot()
        if snapshot is not None:
            return snapshot
        time.sleep(0.02)
    raise AssertionError("timed out waiting for snapshot")


class ExportHelperTests(unittest.TestCase):
    def _session_with_marker(self):
        source = SourceConfig()
        source.kind = SourceKind.SIMULATOR

        processing = ProcessingConfig()
        processing.fft_size = 2048
        processing.display_samples = 1024

        marker = Marker()
        marker.center_frequency_hz = source.center_frequency_hz
        marker.bandwidth_hz = 200_000.0

        session = AnalyzerSession(source, processing)
        session.set_markers([marker])
        self.assertTrue(session.start(), session.last_error())
        return session, source, processing, marker

    def test_exporter_writes_metadata_and_frame_records(self) -> None:
        session, source, processing, marker = self._session_with_marker()
        try:
            snapshot = wait_for_snapshot(session)
        finally:
            session.stop()

        started_at = datetime(2026, 1, 2, 3, 4, 5, tzinfo=timezone.utc)
        frame_at = started_at + timedelta(seconds=1.25)
        exporter = JsonlMeasurementExporter(
            source,
            processing,
            export_started_at=started_at,
            markers=[marker],
            session_metadata={"operator": "test"},
        )

        buffer = io.StringIO()
        metadata = exporter.write_header(buffer)
        frame = exporter.write_frame(buffer, snapshot, frame_timestamp=frame_at)
        records = [json.loads(line) for line in buffer.getvalue().splitlines()]

        self.assertEqual(len(records), 2)
        self.assertEqual(metadata["record_type"], "metadata")
        self.assertEqual(frame["record_type"], "frame")
        self.assertEqual(set(metadata.keys()), METADATA_KEYS | {"session_metadata"})
        self.assertEqual(set(frame.keys()), FRAME_KEYS)
        self.assertEqual(metadata["format_version"], FORMAT_VERSION)
        self.assertEqual(metadata["marker_definitions"][0]["bandwidth_hz"], 200_000.0)
        self.assertEqual(metadata["session_metadata"]["operator"], "test")
        self.assertEqual(frame["sequence"], snapshot.sequence)
        self.assertEqual(frame["detection_count"], len(snapshot.analysis.detections))
        self.assertAlmostEqual(frame["elapsed_seconds"], 1.25, places=6)
        self.assertEqual(frame["sample_rate_hz"], source.sample_rate_hz)

        for detection in frame["detections"]:
            self.assertEqual(set(detection.keys()), DETECTION_KEYS)
        for marker_measurement in frame["marker_measurements"]:
            self.assertEqual(set(marker_measurement.keys()), MARKER_MEASUREMENT_KEYS)

        self.assertEqual(records[0]["record_type"], "metadata")
        self.assertEqual(records[1]["record_type"], "frame")

    def test_export_jsonl_matches_snapshot_content(self) -> None:
        session, source, processing, marker = self._session_with_marker()
        try:
            snapshot = wait_for_snapshot(session)
        finally:
            session.stop()

        started_at = datetime(2026, 5, 1, 8, 0, 0, tzinfo=timezone.utc)
        frame_at = started_at + timedelta(seconds=0.5)

        buffer = io.StringIO()
        exporter = JsonlMeasurementExporter(
            source,
            processing,
            export_started_at=started_at,
            markers=[marker],
        )
        exporter.write_header(buffer)
        exporter.write_frame(buffer, snapshot, frame_timestamp=frame_at)
        metadata, frame = [json.loads(line) for line in buffer.getvalue().splitlines()]

        self.assertEqual(metadata["source_config"]["kind"], "simulator")
        self.assertEqual(
            metadata["processing_config"]["fft_size"], processing.fft_size
        )
        self.assertEqual(frame["center_frequency_hz"], snapshot.spectrum.center_frequency_hz)
        self.assertEqual(
            frame["noise_floor_dbfs"], snapshot.analysis.noise_floor_dbfs
        )
        self.assertEqual(
            frame["strongest_peak_dbfs"], snapshot.analysis.strongest_peak_dbfs
        )
        self.assertEqual(frame["burst_score"], snapshot.analysis.burst_score)

    def test_export_jsonl_function_emits_cli_schema_shape(self) -> None:
        session, source, processing, marker = self._session_with_marker()
        try:
            snapshot = wait_for_snapshot(session)
        finally:
            session.stop()

        started_at = datetime(2026, 6, 1, 12, 0, 0, tzinfo=timezone.utc)
        frame_at = started_at + timedelta(seconds=2)

        with tempfile.TemporaryDirectory() as tempdir:
            output = f"{tempdir}/measurements.jsonl"
            export_jsonl(
                output,
                source,
                processing,
                [snapshot],
                export_started_at=started_at,
                frame_timestamps=[frame_at],
                markers=[marker],
            )
            with open(output, "r", encoding="utf-8") as handle:
                metadata, frame = [json.loads(line) for line in handle.read().splitlines()]

        self.assertEqual(set(metadata.keys()), METADATA_KEYS)
        self.assertEqual(set(frame.keys()), FRAME_KEYS)


if __name__ == "__main__":
    unittest.main()

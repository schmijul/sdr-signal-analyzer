from __future__ import annotations

import json
from datetime import datetime, timezone
from pathlib import Path
from typing import IO, Any, Mapping, Sequence

from . import AnalyzerSnapshot, Marker, ProcessingConfig, SourceConfig

"""Helpers for exporting analyzer snapshots as line-delimited JSON records."""

FORMAT_VERSION = "sdr_signal_analyzer.measurements.v1"
"""Version tag written into JSONL metadata records produced by this module."""


def _utc_now() -> datetime:
    return datetime.now(timezone.utc)


def _utc_timestamp(value: datetime | None) -> str:
    timestamp = value or _utc_now()
    if timestamp.tzinfo is None:
        timestamp = timestamp.replace(tzinfo=timezone.utc)
    else:
        timestamp = timestamp.astimezone(timezone.utc)
    return timestamp.isoformat(timespec="milliseconds").replace("+00:00", "Z")


def _enum_name(value: Any) -> str:
    name = getattr(value, "name", None)
    if isinstance(name, str):
        return name.lower()
    text = str(value)
    if "." in text:
        text = text.rsplit(".", 1)[-1]
    return text.lower()


def _source_config_dict(source_config: SourceConfig) -> dict[str, Any]:
    return {
        "kind": _enum_name(source_config.kind),
        "device_string": source_config.device_string,
        "device_args": source_config.device_args,
        "input_path": source_config.input_path,
        "metadata_path": source_config.metadata_path,
        "network_host": source_config.network_host,
        "network_port": source_config.network_port,
        "channel": source_config.channel,
        "antenna": source_config.antenna,
        "bandwidth_hz": source_config.bandwidth_hz,
        "clock_source": source_config.clock_source,
        "time_source": source_config.time_source,
        "center_frequency_hz": source_config.center_frequency_hz,
        "sample_rate_hz": source_config.sample_rate_hz,
        "gain_db": source_config.gain_db,
        "frame_samples": source_config.frame_samples,
        "loop_playback": source_config.loop_playback,
        "sample_format": _enum_name(source_config.sample_format),
    }


def _processing_config_dict(processing_config: ProcessingConfig) -> dict[str, Any]:
    return {
        "fft_size": processing_config.fft_size,
        "display_samples": processing_config.display_samples,
        "averaging_factor": processing_config.averaging_factor,
        "peak_hold_enabled": processing_config.peak_hold_enabled,
        "detection_threshold_db": processing_config.detection_threshold_db,
        "max_peaks": processing_config.max_peaks,
        "minimum_peak_spacing_bins": processing_config.minimum_peak_spacing_bins,
        "bandwidth_threshold_db": processing_config.bandwidth_threshold_db,
    }


def _marker_definition_dict(marker: Marker) -> dict[str, float]:
    return {
        "center_frequency_hz": marker.center_frequency_hz,
        "bandwidth_hz": marker.bandwidth_hz,
    }


def build_metadata_record(
    source_config: SourceConfig,
    processing_config: ProcessingConfig,
    *,
    export_started_at: datetime | None = None,
    markers: Sequence[Marker] | None = None,
    session_metadata: Mapping[str, Any] | None = None,
) -> dict[str, Any]:
    """Build the leading metadata record for a JSONL measurement export."""
    record: dict[str, Any] = {
        "record_type": "metadata",
        "format_version": FORMAT_VERSION,
        "export_started_at_utc": _utc_timestamp(export_started_at),
        "source_config": _source_config_dict(source_config),
        "processing_config": _processing_config_dict(processing_config),
        "marker_definitions": [
            _marker_definition_dict(marker) for marker in (markers or ())
        ],
    }
    if session_metadata is not None:
        record["session_metadata"] = dict(session_metadata)
    return record


def build_frame_record(
    snapshot: AnalyzerSnapshot,
    source_config: SourceConfig,
    *,
    export_started_at: datetime,
    frame_timestamp: datetime | None = None,
) -> dict[str, Any]:
    """Build one frame record from a snapshot and its export timestamp context."""
    timestamp = frame_timestamp or _utc_now()
    elapsed_seconds = (
        timestamp.astimezone(timezone.utc) - export_started_at.astimezone(timezone.utc)
    ).total_seconds()
    return {
        "record_type": "frame",
        "timestamp_utc": _utc_timestamp(timestamp),
        "elapsed_seconds": elapsed_seconds,
        "sequence": snapshot.sequence,
        "center_frequency_hz": snapshot.spectrum.center_frequency_hz,
        "sample_rate_hz": source_config.sample_rate_hz,
        "noise_floor_dbfs": snapshot.analysis.noise_floor_dbfs,
        "strongest_peak_dbfs": snapshot.analysis.strongest_peak_dbfs,
        "burst_score": snapshot.analysis.burst_score,
        "detection_count": len(snapshot.analysis.detections),
        "detections": [
            {
                "center_frequency_hz": detection.center_frequency_hz,
                "offset_hz": detection.offset_hz,
                "bandwidth_hz": detection.bandwidth_hz,
                "peak_power_dbfs": detection.peak_power_dbfs,
                "labels": list(detection.labels),
            }
            for detection in snapshot.analysis.detections
        ],
        "marker_measurements": [
            {
                "center_frequency_hz": marker.center_frequency_hz,
                "bandwidth_hz": marker.bandwidth_hz,
                "peak_power_dbfs": marker.peak_power_dbfs,
                "average_power_dbfs": marker.average_power_dbfs,
            }
            for marker in snapshot.analysis.marker_measurements
        ],
    }


def write_jsonl_record(file_obj: IO[str], record: Mapping[str, Any]) -> None:
    """Write one compact JSON object followed by a newline."""
    file_obj.write(json.dumps(dict(record), separators=(",", ":")))
    file_obj.write("\n")


class JsonlMeasurementExporter:
    """Write the metadata header and frame records for one export session."""

    def __init__(
        self,
        source_config: SourceConfig,
        processing_config: ProcessingConfig,
        *,
        export_started_at: datetime | None = None,
        markers: Sequence[Marker] | None = None,
        session_metadata: Mapping[str, Any] | None = None,
    ) -> None:
        self.source_config = source_config
        self.processing_config = processing_config
        self.export_started_at = export_started_at or _utc_now()
        self.markers = list(markers or ())
        self.session_metadata = dict(session_metadata) if session_metadata else None

    def metadata_record(self) -> dict[str, Any]:
        """Return the metadata/header record for the current export session."""
        return build_metadata_record(
            self.source_config,
            self.processing_config,
            export_started_at=self.export_started_at,
            markers=self.markers,
            session_metadata=self.session_metadata,
        )

    def frame_record(
        self,
        snapshot: AnalyzerSnapshot,
        *,
        frame_timestamp: datetime | None = None,
    ) -> dict[str, Any]:
        """Return a frame record for one snapshot."""
        return build_frame_record(
            snapshot,
            self.source_config,
            export_started_at=self.export_started_at,
            frame_timestamp=frame_timestamp,
        )

    def write_header(self, file_obj: IO[str]) -> dict[str, Any]:
        """Write and return the metadata/header record."""
        record = self.metadata_record()
        write_jsonl_record(file_obj, record)
        return record

    def write_frame(
        self,
        file_obj: IO[str],
        snapshot: AnalyzerSnapshot,
        *,
        frame_timestamp: datetime | None = None,
    ) -> dict[str, Any]:
        """Write and return one frame record."""
        record = self.frame_record(snapshot, frame_timestamp=frame_timestamp)
        write_jsonl_record(file_obj, record)
        return record


def export_jsonl(
    output_path: str | Path,
    source_config: SourceConfig,
    processing_config: ProcessingConfig,
    snapshots: Sequence[AnalyzerSnapshot],
    *,
    export_started_at: datetime | None = None,
    frame_timestamps: Sequence[datetime] | None = None,
    markers: Sequence[Marker] | None = None,
    session_metadata: Mapping[str, Any] | None = None,
) -> None:
    """Write a complete JSONL export containing one header and many frames."""
    exporter = JsonlMeasurementExporter(
        source_config,
        processing_config,
        export_started_at=export_started_at,
        markers=markers,
        session_metadata=session_metadata,
    )
    timestamps = list(frame_timestamps) if frame_timestamps is not None else []
    with Path(output_path).open("w", encoding="utf-8") as handle:
        exporter.write_header(handle)
        for index, snapshot in enumerate(snapshots):
            frame_timestamp = timestamps[index] if index < len(timestamps) else None
            exporter.write_frame(handle, snapshot, frame_timestamp=frame_timestamp)

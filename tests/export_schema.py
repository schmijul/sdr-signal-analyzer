METADATA_KEYS = {
    "record_type",
    "format_version",
    "export_started_at_utc",
    "source_config",
    "processing_config",
    "marker_definitions",
}

FRAME_KEYS = {
    "record_type",
    "timestamp_utc",
    "elapsed_seconds",
    "sequence",
    "center_frequency_hz",
    "sample_rate_hz",
    "noise_floor_dbfs",
    "strongest_peak_dbfs",
    "burst_score",
    "detection_count",
    "detections",
    "marker_measurements",
}

DETECTION_KEYS = {
    "center_frequency_hz",
    "offset_hz",
    "bandwidth_hz",
    "peak_power_dbfs",
    "labels",
}

MARKER_MEASUREMENT_KEYS = {
    "center_frequency_hz",
    "bandwidth_hz",
    "peak_power_dbfs",
    "average_power_dbfs",
}

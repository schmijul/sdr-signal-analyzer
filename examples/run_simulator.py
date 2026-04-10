from __future__ import annotations

import time

from sdr_signal_analyzer import (
    AnalyzerSession,
    ProcessingConfig,
    SourceConfig,
    SourceKind,
)


def main() -> int:
    source = SourceConfig()
    source.kind = SourceKind.SIMULATOR

    processing = ProcessingConfig()
    processing.fft_size = 2048
    processing.display_samples = 1024

    session = AnalyzerSession(source, processing)
    if not session.start():
        raise SystemExit(session.last_error())

    try:
        for _ in range(30):
            snapshot = session.poll_snapshot()
            if snapshot:
                print(
                    f"frame={snapshot.sequence} "
                    f"noise={snapshot.analysis.noise_floor_dbfs:.1f} dBFS "
                    f"detections={len(snapshot.analysis.detections)}"
                )
                for detection in snapshot.analysis.detections[:3]:
                    print(
                        f"  {detection.center_frequency_hz/1e6:.3f} MHz "
                        f"{detection.bandwidth_hz/1e3:.1f} kHz "
                        f"{', '.join(detection.labels)}"
                    )
            time.sleep(0.1)
    finally:
        session.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

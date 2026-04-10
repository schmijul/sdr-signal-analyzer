from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path

from sdr_signal_analyzer import (
    AnalyzerSession,
    ProcessingConfig,
    SourceConfig,
    SourceKind,
)


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run a deterministic SDR analyzer demo without hardware."
    )
    parser.add_argument(
        "--source",
        choices=("simulator", "replay"),
        default="simulator",
        help="Demo source. Defaults to the built-in simulator.",
    )
    parser.add_argument(
        "--frames",
        type=int,
        default=10,
        help="Number of snapshots to print before exiting.",
    )
    parser.add_argument(
        "--fft-size",
        type=int,
        default=2048,
        help="FFT size for the demo session.",
    )
    parser.add_argument(
        "--input",
        help="Replay data path. Required when --source replay is selected.",
    )
    parser.add_argument(
        "--meta",
        help=(
            "Replay metadata path. Optional for raw .bin replay, inferred for "
            "SigMF when omitted."
        ),
    )
    return parser


def _configure_source(args: argparse.Namespace) -> SourceConfig:
    source = SourceConfig()
    if args.source == "replay":
        if not args.input:
            raise SystemExit("--input is required when --source replay is selected.")
        source.kind = SourceKind.REPLAY
        source.input_path = str(Path(args.input))
        if args.meta:
            source.metadata_path = str(Path(args.meta))
        source.frame_samples = args.fft_size
    else:
        source.kind = SourceKind.SIMULATOR
    return source


def _configure_processing(args: argparse.Namespace) -> ProcessingConfig:
    processing = ProcessingConfig()
    processing.fft_size = args.fft_size
    processing.display_samples = min(1024, args.fft_size)
    return processing


def main(argv: list[str] | None = None) -> int:
    """Run the deterministic demo CLI and return an exit status."""
    parser = _build_parser()
    args = parser.parse_args(argv)
    if args.frames <= 0:
        parser.error("--frames must be positive.")
    if args.fft_size <= 0 or args.fft_size & (args.fft_size - 1):
        parser.error("--fft-size must be a positive power of two.")

    session = AnalyzerSession(_configure_source(args), _configure_processing(args))
    if not session.start():
        print(session.last_error(), file=sys.stderr)
        return 1

    try:
        printed = 0
        while printed < args.frames:
            snapshot = session.poll_snapshot()
            if snapshot is None:
                if not session.is_running():
                    error = session.last_error()
                    if printed == 0:
                        print(
                            error or "session ended before producing data",
                            file=sys.stderr,
                        )
                        return 1
                    break
                time.sleep(0.02)
                continue

            printed += 1
            print(
                f"frame={snapshot.sequence} "
                f"noise={snapshot.analysis.noise_floor_dbfs:.1f} dBFS "
                f"detections={len(snapshot.analysis.detections)}"
            )
            for detection in snapshot.analysis.detections[:3]:
                labels = ", ".join(detection.labels)
                print(
                    f"  {detection.center_frequency_hz / 1e6:.3f} MHz "
                    f"{detection.bandwidth_hz / 1e3:.1f} kHz {labels}"
                )
    finally:
        session.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

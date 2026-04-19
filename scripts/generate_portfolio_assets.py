from __future__ import annotations

import argparse
import dataclasses
import os
import subprocess
import sys
from pathlib import Path


@dataclasses.dataclass(frozen=True)
class ScreenshotPreset:
    name: str
    output_name: str
    center_hz: float
    sample_rate_hz: float
    gain_db: float
    marker_hz: float
    marker_bw_hz: float
    source_kind_index: int = 0
    device_args: str = ""
    channel: str = "0"
    antenna: str = ""
    fft_size: int = 2048
    settle_cycles: int = 32
    window_size: tuple[int, int] = (1600, 1050)


PRESETS: dict[str, ScreenshotPreset] = {
    "overview": ScreenshotPreset(
        name="overview",
        output_name="overview.png",
        center_hz=100_000_000.0,
        sample_rate_hz=2_400_000.0,
        gain_db=10.0,
        marker_hz=100_000_000.0,
        marker_bw_hz=200_000.0,
    ),
    "433-mhz": ScreenshotPreset(
        name="433-mhz",
        output_name="ism_433.png",
        center_hz=433_920_000.0,
        sample_rate_hz=2_400_000.0,
        gain_db=14.0,
        marker_hz=433_920_000.0,
        marker_bw_hz=300_000.0,
    ),
    "ism-433": ScreenshotPreset(
        name="ism-433",
        output_name="ism_433.png",
        center_hz=433_920_000.0,
        sample_rate_hz=2_400_000.0,
        gain_db=14.0,
        marker_hz=433_920_000.0,
        marker_bw_hz=300_000.0,
    ),
    "narrowband-focus": ScreenshotPreset(
        name="narrowband-focus",
        output_name="narrowband_focus.png",
        center_hz=100_150_000.0,
        sample_rate_hz=1_200_000.0,
        gain_db=8.0,
        marker_hz=100_150_000.0,
        marker_bw_hz=50_000.0,
        fft_size=4096,
        settle_cycles=40,
    ),
    "uhd-overview": ScreenshotPreset(
        name="uhd-overview",
        output_name="uhd_overview.png",
        center_hz=100_000_000.0,
        sample_rate_hz=2_000_000.0,
        gain_db=20.0,
        marker_hz=100_000_000.0,
        marker_bw_hz=200_000.0,
        source_kind_index=3,
        device_args="type=b200",
        channel="0",
        antenna="TX/RX",
        settle_cycles=20,
    ),
}

def _repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


def _import_gui():
    repo_root = _repo_root()
    sys.path.insert(0, str(repo_root / "python"))

    from PySide6 import QtCore, QtWidgets
    from sdr_signal_analyzer.gui import MainWindow, MarkerEntry

    return QtCore, QtWidgets, MainWindow, MarkerEntry


def _apply_preset(window, preset: ScreenshotPreset, MarkerEntry) -> None:
    window.resize(*preset.window_size)
    window._source_kind.setCurrentIndex(preset.source_kind_index)
    window._center_edit.setText(str(preset.center_hz))
    window._rate_edit.setText(str(preset.sample_rate_hz))
    window._gain_edit.setText(str(preset.gain_db))
    window._fft_edit.setText(str(preset.fft_size))
    window._device_args_edit.setText(preset.device_args)
    window._channel_edit.setText(preset.channel)
    if preset.antenna:
        window._antenna_edit.setText(preset.antenna)
    window.set_marker_entries(
        [
            MarkerEntry(
                "Marker 1",
                preset.marker_hz,
                preset.marker_bw_hz,
            )
        ]
    )


def _render_preset(preset: ScreenshotPreset, output_path: Path) -> None:
    QtCore, QtWidgets, MainWindow, MarkerEntry = _import_gui()

    app = QtWidgets.QApplication.instance() or QtWidgets.QApplication([])
    window = MainWindow()
    _apply_preset(window, preset, MarkerEntry)
    window.show()
    app.processEvents()
    window._toggle_stream()

    for _ in range(preset.settle_cycles):
        app.processEvents()
        QtCore.QThread.msleep(25)

    saved = window.grab().save(str(output_path))
    window._session.stop()
    window.close()
    app.processEvents()
    print(f"preset={preset.name} saved={saved} path={output_path}")
    if not saved:
        raise RuntimeError(f"Failed to save screenshot to {output_path}")


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Generate deterministic GUI screenshots from simulator scenes."
    )
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--preset",
        choices=sorted({name for name in PRESETS}),
        help="Named simulator scene to render.",
    )
    group.add_argument(
        "--all",
        action="store_true",
        help="Render all committed portfolio screenshots.",
    )
    parser.add_argument(
        "--output",
        help="Output file path for a single preset.",
    )
    parser.add_argument(
        "--output-dir",
        help="Output directory for --all. Defaults to docs/screenshots.",
    )
    return parser


def _write_single(preset_name: str, output: str | None) -> None:
    if output is None:
        raise SystemExit("--output is required with --preset.")
    preset = PRESETS[preset_name]
    output_path = Path(output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    _render_preset(preset, output_path)


def _write_all(output_dir: str | None) -> None:
    base_dir = Path(output_dir) if output_dir else _repo_root() / "docs/screenshots"
    base_dir.mkdir(parents=True, exist_ok=True)
    for preset_name in ("overview", "433-mhz", "narrowband-focus"):
        preset = PRESETS[preset_name]
        output_path = base_dir / preset.output_name
        completed = subprocess.run(
            [
                sys.executable,
                str(Path(__file__).resolve()),
                "--preset",
                preset_name,
                "--output",
                str(output_path),
            ],
            check=False,
            env=os.environ.copy(),
        )
        if completed.returncode != 0:
            raise SystemExit(completed.returncode)


def main() -> int:
    parser = _build_parser()
    args = parser.parse_args()

    os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

    if args.preset:
        _write_single(args.preset, args.output)
    else:
        _write_all(args.output_dir)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

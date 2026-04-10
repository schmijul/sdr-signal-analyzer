from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a deterministic GUI screenshot for the README."
    )
    parser.add_argument("--output", required=True)
    parser.add_argument("--center-hz", type=float, required=True)
    parser.add_argument("--sample-rate", type=float, required=True)
    parser.add_argument("--gain-db", type=float, required=True)
    parser.add_argument("--marker-hz", type=float, required=True)
    parser.add_argument("--marker-bw-hz", type=float, required=True)
    args = parser.parse_args()

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)

    sys.path.insert(0, str(Path("python").resolve()))

    from PySide6 import QtCore, QtWidgets
    from sdr_signal_analyzer.gui import MainWindow

    app = QtWidgets.QApplication([])
    window = MainWindow()
    window.resize(1600, 1050)
    window._center_edit.setText(str(args.center_hz))
    window._rate_edit.setText(str(args.sample_rate))
    window._gain_edit.setText(str(args.gain_db))
    window._marker_center_edit.setText(str(args.marker_hz))
    window._marker_bw_edit.setText(str(args.marker_bw_hz))
    window._update_marker()
    window.show()
    window._toggle_stream()
    for _ in range(24):
        app.processEvents()
        QtCore.QThread.msleep(25)
    saved = window.grab().save(str(output_path))
    print(f"saved={saved} path={output_path}")
    window._session.stop()
    os._exit(0 if saved else 1)


if __name__ == "__main__":
    raise SystemExit(main())

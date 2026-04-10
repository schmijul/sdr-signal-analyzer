#!/usr/bin/env python3

from __future__ import annotations

import os
import unittest

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

try:
    from PySide6 import QtWidgets
except ImportError:
    print("PySide6 not available; skipping GUI validation test.")
    raise SystemExit(77)

from sdr_signal_analyzer import SourceKind
from sdr_signal_analyzer.gui import MainWindow


def select_source_kind(window: MainWindow, kind: SourceKind) -> None:
    for index in range(window._source_kind.count()):
        if window._source_kind.itemData(index) == kind:
            window._source_kind.setCurrentIndex(index)
            return
    raise AssertionError(f"Source kind {kind!r} not found.")


class GuiValidationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls._app = QtWidgets.QApplication.instance() or QtWidgets.QApplication([])

    def setUp(self) -> None:
        self.window = MainWindow()

    def tearDown(self) -> None:
        self.window._session.stop()
        self.window.close()
        self._app.processEvents()

    def test_invalid_center_frequency_is_rejected(self) -> None:
        self.window._center_edit.setText("abc")
        self.window._toggle_stream()
        self.assertFalse(self.window._session.is_running())
        self.assertEqual(self.window._start_button.text(), "Start")
        self.assertEqual(
            self.window._status_label.text(),
            "Input error: Center Hz must be a number.",
        )

    def test_invalid_fft_size_is_rejected(self) -> None:
        self.window._fft_edit.setText("123")
        self.window._toggle_stream()
        self.assertFalse(self.window._session.is_running())
        self.assertEqual(
            self.window._status_label.text(),
            "Input error: FFT Size must be a power of two.",
        )

    def test_replay_requires_an_input_path(self) -> None:
        select_source_kind(self.window, SourceKind.REPLAY)
        self.window._input_path_edit.setText("")
        self.window._toggle_stream()
        self.assertFalse(self.window._session.is_running())
        self.assertEqual(
            self.window._status_label.text(),
            "Input error: Replay Input is required.",
        )

    def test_invalid_marker_input_is_rejected(self) -> None:
        self.window._marker_center_edit.setText("100000000")
        self.window._marker_bw_edit.setText("abc")
        self.window._update_marker()
        self.assertEqual(len(self.window._markers), 1)
        self.assertEqual(self.window._markers[0].center_frequency_hz, 100000000.0)
        self.assertEqual(self.window._markers[0].bandwidth_hz, 200000.0)
        self.assertEqual(
            self.window._status_label.text(),
            "Input error: Marker BW must be a number.",
        )


if __name__ == "__main__":
    unittest.main()

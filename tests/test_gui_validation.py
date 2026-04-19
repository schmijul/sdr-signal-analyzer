#!/usr/bin/env python3

from __future__ import annotations

import ast
import os
import time
import unittest
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")

try:
    from PySide6 import QtWidgets
    from sdr_signal_analyzer import SourceKind
    from sdr_signal_analyzer.gui import MainWindow, MarkerEditorDialog, MarkerEntry

    GUI_AVAILABLE = True
except ImportError:
    QtWidgets = None
    SourceKind = None
    MainWindow = None
    GUI_AVAILABLE = False


ROOT = Path(__file__).resolve().parents[1]
GUI_SOURCE = ROOT / "python" / "sdr_signal_analyzer" / "gui.py"
DEMO_SOURCE = ROOT / "python" / "sdr_signal_analyzer" / "demo.py"


def _docstring_from_source(path: Path, name: str) -> str | None:
    tree = ast.parse(path.read_text(encoding="utf-8"))
    for node in tree.body:
        if isinstance(node, (ast.ClassDef, ast.FunctionDef)) and node.name == name:
            return ast.get_docstring(node)
    return None


def select_source_kind(window: MainWindow, kind: SourceKind) -> None:
    for index in range(window._source_kind.count()):
        if window._source_kind.itemData(index) == kind:
            window._source_kind.setCurrentIndex(index)
            return
    raise AssertionError(f"Source kind {kind!r} not found.")


class GuiValidationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        if not GUI_AVAILABLE:
            raise unittest.SkipTest("PySide6 not available.")
        cls._app = QtWidgets.QApplication.instance() or QtWidgets.QApplication([])

    def setUp(self) -> None:
        if not GUI_AVAILABLE:
            self.skipTest("PySide6 not available.")
        self.window = MainWindow()

    def tearDown(self) -> None:
        if not GUI_AVAILABLE:
            return
        if getattr(self.window, "_marker_editor_dialog", None) is not None:
            self.window._marker_editor_dialog.close()
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

    def test_marker_editor_rejects_invalid_input(self) -> None:
        dialog = MarkerEditorDialog(
            [MarkerEntry("Marker 1", 100000000.0, 200000.0)],
            self.window,
        )
        dialog._table.item(0, 1).setText("abc")
        dialog.accept()
        self.assertEqual(dialog.result(), QtWidgets.QDialog.DialogCode.Rejected)
        self.assertEqual(
            dialog._message_label.text(),
            "Row 1 Center Hz must be a number.",
        )

    def test_marker_entries_can_be_updated_programmatically(self) -> None:
        self.window.set_marker_entries(
            [
                MarkerEntry("Primary", 100000000.0, 200000.0),
                MarkerEntry("Backup", 100250000.0, 50000.0),
            ]
        )
        self.assertEqual(len(self.window._marker_entries), 2)
        self.assertEqual(self.window._marker_entries[0].name, "Primary")
        self.assertEqual(self.window._markers[1].bandwidth_hz, 50000.0)
        self.assertEqual(
            self.window._marker_summary_label.text(),
            "2 markers configured",
        )

    def test_update_marker_opens_dialog(self) -> None:
        self.window._open_marker_editor()
        self._app.processEvents()
        self.assertIsNotNone(self.window._marker_editor_dialog)
        self.assertTrue(self.window._marker_editor_dialog.isVisible())

    def test_default_state_uses_simulator_profile(self) -> None:
        self.assertEqual(self.window._source_kind.currentData(), SourceKind.SIMULATOR)
        self.assertEqual(self.window._fft_edit.text(), "2048")
        self.assertEqual(self.window._start_button.text(), "Start")
        self.assertEqual(self.window._status_label.text(), "Idle")
        self.assertEqual(self.window._waterfall._rows, 220)
        self.assertEqual(self.window._peak_hold_reset_button.text(), "Reset Peak Hold")
        self.assertEqual(
            self.window._peak_hold_auto_reset_button.text(),
            "Peak Auto Reset: Off",
        )

    def test_peak_hold_reset_button_updates_status(self) -> None:
        self.window._reset_peak_hold()
        self.assertEqual(self.window._status_label.text(), "Peak hold reset")

    def test_peak_hold_auto_reset_toggle_starts_and_stops_timer(self) -> None:
        self.window._peak_hold_auto_reset_button.setChecked(True)
        self.assertTrue(self.window._peak_hold_auto_reset_timer.isActive())
        self.assertEqual(
            self.window._peak_hold_auto_reset_button.text(),
            "Peak Auto Reset: On",
        )
        self.assertEqual(
            self.window._status_label.text(),
            "Peak hold auto reset every 3s",
        )

        self.window._peak_hold_auto_reset_button.setChecked(False)
        self.assertFalse(self.window._peak_hold_auto_reset_timer.isActive())
        self.assertEqual(
            self.window._peak_hold_auto_reset_button.text(),
            "Peak Auto Reset: Off",
        )
        self.assertEqual(
            self.window._status_label.text(),
            "Peak hold auto reset disabled",
        )

    def test_replay_eof_surfaces_a_stopped_status(self) -> None:
        fixture_root = ROOT / "tests" / "fixtures"
        select_source_kind(self.window, SourceKind.REPLAY)
        self.window._input_path_edit.setText(str(fixture_root / "tone_cf32.sigmf-data"))
        self.window._metadata_path_edit.setText(str(fixture_root / "tone_cf32.sigmf-meta"))
        self.window._toggle_stream()
        self.assertTrue(self.window._session.is_running())

        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline:
            self._app.processEvents()
            if not self.window._session.is_running():
                break
            time.sleep(0.01)
        self.assertFalse(self.window._session.is_running())
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline and self.window._start_button.text() != "Start":
            self._app.processEvents()
            time.sleep(0.01)
        self.assertEqual(self.window._start_button.text(), "Start")
        self.assertIn("Session stopped:", self.window._status_label.text())
        self.assertIn("Replay file reached end of stream.", self.window._status_label.text())


class GuiDocstringTests(unittest.TestCase):
    def test_gui_public_entrypoints_and_widgets_are_documented(self) -> None:
        expectations = [
            (GUI_SOURCE, "MainWindow"),
            (GUI_SOURCE, "PlotCanvas"),
            (GUI_SOURCE, "WaterfallCanvas"),
            (GUI_SOURCE, "DetectionTable"),
            (GUI_SOURCE, "main"),
            (DEMO_SOURCE, "main"),
        ]
        for path, name in expectations:
            with self.subTest(name=name):
                self.assertTrue((_docstring_from_source(path, name) or "").strip(), name)


if __name__ == "__main__":
    unittest.main()

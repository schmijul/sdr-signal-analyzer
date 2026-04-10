from __future__ import annotations

import unittest

from sdr_signal_analyzer import export_jsonl
from sdr_signal_analyzer.demo import main as demo_main
from sdr_signal_analyzer.export import (
    JsonlMeasurementExporter,
    build_frame_record,
    build_metadata_record,
    write_jsonl_record,
)


class PythonDocstringTests(unittest.TestCase):
    def test_public_python_export_surface_has_docstrings(self) -> None:
        public_objects = [
            JsonlMeasurementExporter,
            build_metadata_record,
            build_frame_record,
            write_jsonl_record,
            export_jsonl,
            demo_main,
        ]

        for public_object in public_objects:
            with self.subTest(public_object=public_object):
                self.assertTrue(public_object.__doc__)
                self.assertTrue(public_object.__doc__.strip())

    def test_gui_entrypoint_has_docstring_when_gui_deps_are_available(self) -> None:
        try:
            from sdr_signal_analyzer.gui import main as gui_main
        except RuntimeError as exc:
            self.skipTest(str(exc))

        self.assertTrue(gui_main.__doc__)
        self.assertTrue(gui_main.__doc__.strip())


if __name__ == "__main__":
    unittest.main()

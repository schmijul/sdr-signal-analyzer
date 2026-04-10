#!/usr/bin/env python3

from __future__ import annotations

import subprocess
import sys
import unittest


class DemoCommandTests(unittest.TestCase):
    def test_demo_module_runs_and_prints_detections(self) -> None:
        completed = subprocess.run(
            [sys.executable, "-m", "sdr_signal_analyzer.demo", "--frames", "3"],
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("frame=", completed.stdout)
        self.assertIn("detections=", completed.stdout)


if __name__ == "__main__":
    unittest.main()

#!/usr/bin/env python3

from __future__ import annotations

import os
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

try:
    import PySide6  # noqa: F401
except ImportError:
    print("PySide6 not available; skipping screenshot asset test.")
    raise SystemExit(77)


class ScreenshotGeneratorTests(unittest.TestCase):
    def test_offscreen_screenshot_generation(self) -> None:
        root = Path(__file__).resolve().parents[1]
        script = root / "scripts" / "generate_portfolio_assets.py"
        env = os.environ.copy()
        env.setdefault("QT_QPA_PLATFORM", "offscreen")
        env["PYTHONPATH"] = str(root / "python")

        with tempfile.TemporaryDirectory() as tempdir:
            output_dir = Path(tempdir) / "screenshots"
            completed = subprocess.run(
                [
                    sys.executable,
                    str(script),
                    "--all",
                    "--output-dir",
                    str(output_dir),
                ],
                cwd=root,
                env=env,
                capture_output=True,
                text=True,
                check=False,
            )

            self.assertEqual(completed.returncode, 0, completed.stderr)
            for name in ("overview.png", "ism_433.png", "narrowband_focus.png"):
                path = output_dir / name
                self.assertTrue(path.exists(), f"missing screenshot: {path}")
                self.assertGreater(path.stat().st_size, 0, f"empty screenshot: {path}")


if __name__ == "__main__":
    unittest.main()

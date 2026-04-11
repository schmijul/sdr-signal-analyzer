#!/usr/bin/env python3

from __future__ import annotations

import subprocess
import sys
import unittest
from pathlib import Path


class ReleaseValidationScriptTests(unittest.TestCase):
    def test_dry_run_prints_expected_commands(self) -> None:
        root = Path(__file__).resolve().parents[1]
        script = root / "scripts" / "release_validate.py"
        completed = subprocess.run(
            [
                sys.executable,
                str(script),
                "--dry-run",
                "--skip-install-smoke",
            ],
            cwd=root,
            capture_output=True,
            text=True,
            check=False,
        )
        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("cmake -S . -B", completed.stdout)
        self.assertIn("ctest --test-dir", completed.stdout)
        self.assertIn("mkdocs build --strict", completed.stdout)
        self.assertIn("pyproject-build", completed.stdout)
        self.assertIn("twine check", completed.stdout)


if __name__ == "__main__":
    unittest.main()

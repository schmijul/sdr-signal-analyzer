#!/usr/bin/env python3

from __future__ import annotations

import os
import stat
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path


class CliWrapperTests(unittest.TestCase):
    def test_module_delegates_to_configured_binary(self) -> None:
        with tempfile.TemporaryDirectory() as tmpdir:
            binary = Path(tmpdir) / "fake-cli"
            binary.write_text(
                "#!/usr/bin/env python3\n"
                "import sys\n"
                "print('wrapped', ' '.join(sys.argv[1:]))\n",
                encoding="utf-8",
            )
            binary.chmod(binary.stat().st_mode | stat.S_IXUSR)

            env = os.environ.copy()
            env["SDR_ANALYZER_CLI"] = str(binary)
            python_root = str(Path(__file__).resolve().parents[1] / "python")
            env["PYTHONPATH"] = (
                python_root
                if not env.get("PYTHONPATH")
                else python_root + os.pathsep + env["PYTHONPATH"]
            )
            completed = subprocess.run(
                [
                    sys.executable,
                    "-m",
                    "sdr_signal_analyzer.cli",
                    "--source",
                    "simulator",
                    "--frames",
                    "2",
                ],
                capture_output=True,
                text=True,
                check=False,
                env=env,
            )

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertIn("wrapped --source simulator --frames 2", completed.stdout)


if __name__ == "__main__":
    unittest.main()

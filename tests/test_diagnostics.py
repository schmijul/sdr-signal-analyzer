#!/usr/bin/env python3

from __future__ import annotations

import time
import unittest

from sdr_signal_analyzer import AnalyzerSession, ProcessingConfig, SourceConfig, SourceKind


class DiagnosticsApiTests(unittest.TestCase):
    def test_failed_start_exposes_structured_diagnostics(self) -> None:
        source = SourceConfig()
        source.kind = SourceKind.REPLAY

        session = AnalyzerSession(source, ProcessingConfig())
        self.assertFalse(session.start())
        diagnostics = session.diagnostics()
        self.assertGreaterEqual(len(diagnostics), 1)
        self.assertEqual(diagnostics[-1].level, "error")
        self.assertEqual(diagnostics[-1].source_kind, "replay")
        self.assertGreater(diagnostics[-1].session_id, 0)
        self.assertIn(
            "Source configuration for source 'replay' failed",
            diagnostics[-1].message,
        )

    def test_drain_diagnostics_clears_buffer(self) -> None:
        source = SourceConfig()
        source.kind = SourceKind.SIMULATOR

        session = AnalyzerSession(source, ProcessingConfig())
        self.assertTrue(session.start(), session.last_error())
        deadline = time.monotonic() + 1.0
        while time.monotonic() < deadline and session.poll_snapshot() is None:
            time.sleep(0.02)
        startup = session.drain_diagnostics()
        self.assertTrue(any(entry.code == "session.started" for entry in startup))
        self.assertTrue(all(entry.session_id > 0 for entry in startup))
        self.assertEqual(session.diagnostics(), [])
        session.stop()
        shutdown = session.drain_diagnostics()
        self.assertTrue(any(entry.code == "session.worker_stopped" for entry in shutdown))


if __name__ == "__main__":
    unittest.main()

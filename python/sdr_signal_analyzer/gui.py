from __future__ import annotations

import sys
from typing import Optional

import numpy as np

from . import AnalyzerSession, ProcessingConfig, SourceConfig, SourceKind

try:
    import pyqtgraph as pg
    from PySide6 import QtCore, QtWidgets
except ImportError as exc:  # pragma: no cover - manual runtime check
    raise RuntimeError(
        "GUI dependencies are missing. Install with `pip install .[gui]` or add PySide6 and pyqtgraph."
    ) from exc


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("SDR Signal Analyzer")

        source = SourceConfig()
        source.kind = SourceKind.SIMULATOR
        processing = ProcessingConfig()
        self._session = AnalyzerSession(source, processing)

        central = QtWidgets.QWidget()
        layout = QtWidgets.QVBoxLayout(central)
        controls = QtWidgets.QHBoxLayout()

        self._center_edit = QtWidgets.QLineEdit("100000000")
        self._rate_edit = QtWidgets.QLineEdit("2400000")
        self._gain_edit = QtWidgets.QLineEdit("10")
        self._fft_edit = QtWidgets.QLineEdit(str(processing.fft_size))
        self._start_button = QtWidgets.QPushButton("Start")
        self._start_button.clicked.connect(self._toggle_stream)

        for label, widget in (
            ("Center Hz", self._center_edit),
            ("Sample Rate", self._rate_edit),
            ("Gain dB", self._gain_edit),
            ("FFT Size", self._fft_edit),
        ):
            controls.addWidget(QtWidgets.QLabel(label))
            controls.addWidget(widget)
        controls.addWidget(self._start_button)

        layout.addLayout(controls)

        self._spectrum_plot = pg.PlotWidget(title="Spectrum")
        self._average_plot = self._spectrum_plot.plot(pen=pg.mkPen("#ff6b35", width=2))
        self._peak_plot = self._spectrum_plot.plot(pen=pg.mkPen("#004e89", width=1))

        self._waterfall = pg.ImageView(view=pg.PlotItem())
        self._time_plot = pg.PlotWidget(title="Time Domain")
        self._time_i = self._time_plot.plot(pen=pg.mkPen("#2a9d8f", width=1))
        self._time_q = self._time_plot.plot(pen=pg.mkPen("#264653", width=1))
        self._time_mag = self._time_plot.plot(pen=pg.mkPen("#e76f51", width=2))

        self._status_label = QtWidgets.QLabel("Idle")

        layout.addWidget(self._spectrum_plot, stretch=2)
        layout.addWidget(self._waterfall, stretch=2)
        layout.addWidget(self._time_plot, stretch=1)
        layout.addWidget(self._status_label)
        self.setCentralWidget(central)

        self._waterfall_history = np.full((200, processing.fft_size), -140.0, dtype=np.float32)
        self._timer = QtCore.QTimer(self)
        self._timer.setInterval(30)
        self._timer.timeout.connect(self._poll_session)

    def closeEvent(self, event) -> None:  # noqa: N802
        self._session.stop()
        super().closeEvent(event)

    def _toggle_stream(self) -> None:
        if self._session.is_running():
            self._session.stop()
            self._timer.stop()
            self._start_button.setText("Start")
            self._status_label.setText("Stopped")
            return

        source = self._session.source_config()
        source.center_frequency_hz = float(self._center_edit.text())
        source.sample_rate_hz = float(self._rate_edit.text())
        source.gain_db = float(self._gain_edit.text())
        source.kind = SourceKind.SIMULATOR
        self._session.update_source_config(source)

        processing = self._session.processing_config()
        processing.fft_size = int(self._fft_edit.text())
        processing.display_samples = min(processing.fft_size, 2048)
        self._session.update_processing_config(processing)

        if not self._session.start():
            self._status_label.setText(f"Start failed: {self._session.last_error()}")
            return

        self._waterfall_history = np.full((200, processing.fft_size), -140.0, dtype=np.float32)
        self._timer.start()
        self._start_button.setText("Stop")
        self._status_label.setText("Running")

    def _poll_session(self) -> None:
        snapshot = self._session.poll_snapshot()
        if snapshot is None:
            return

        power = np.asarray(snapshot.spectrum.power_dbfs, dtype=np.float32)
        average = np.asarray(snapshot.spectrum.average_dbfs, dtype=np.float32)
        peak = np.asarray(snapshot.spectrum.peak_hold_dbfs, dtype=np.float32)
        freqs = (
            np.arange(power.size, dtype=np.float64) - (power.size / 2.0)
        ) * snapshot.spectrum.bin_resolution_hz + snapshot.spectrum.center_frequency_hz

        self._average_plot.setData(freqs, average)
        self._peak_plot.setData(freqs, peak)

        self._waterfall_history = np.roll(self._waterfall_history, -1, axis=0)
        self._waterfall_history[-1, :] = power
        self._waterfall.setImage(self._waterfall_history, autoLevels=False)

        time_i = np.asarray(snapshot.time_domain.i, dtype=np.float32)
        time_q = np.asarray(snapshot.time_domain.q, dtype=np.float32)
        time_mag = np.asarray(snapshot.time_domain.magnitude, dtype=np.float32)
        time_axis = np.arange(time_i.size)
        self._time_i.setData(time_axis, time_i)
        self._time_q.setData(time_axis, time_q)
        self._time_mag.setData(time_axis, time_mag)

        labels = ", ".join(
            f"{det.labels[0]} @ {det.center_frequency_hz / 1e6:.3f} MHz / {det.bandwidth_hz / 1e3:.1f} kHz"
            for det in snapshot.analysis.detections[:3]
        )
        self._status_label.setText(
            "Noise "
            f"{snapshot.analysis.noise_floor_dbfs:.1f} dBFS | "
            f"Peak {snapshot.analysis.strongest_peak_dbfs:.1f} dBFS | "
            f"{labels or 'no detections'}"
        )


def main() -> int:
    app = QtWidgets.QApplication(sys.argv)
    pg.setConfigOptions(antialias=True, background="#f5efe6", foreground="#122620")
    window = MainWindow()
    window.resize(1500, 980)
    window.show()
    return app.exec()

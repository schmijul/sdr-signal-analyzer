from __future__ import annotations

import sys
from typing import Sequence

from . import AnalyzerSession, Marker, ProcessingConfig, SourceConfig, SourceKind

try:
    from PySide6 import QtCore, QtGui, QtWidgets
except ImportError as exc:  # pragma: no cover - runtime guard
    raise RuntimeError(
        "GUI dependencies are missing. Install PySide6 to run the Qt frontend."
    ) from exc


def _clamp(value: float, lower: float, upper: float) -> float:
    return max(lower, min(upper, value))


def _format_hz(value: float) -> str:
    abs_value = abs(value)
    if abs_value >= 1e9:
        return f"{value / 1e9:.3f} GHz"
    if abs_value >= 1e6:
        return f"{value / 1e6:.3f} MHz"
    if abs_value >= 1e3:
        return f"{value / 1e3:.1f} kHz"
    return f"{value:.0f} Hz"


def _format_dbfs(value: float) -> str:
    return f"{value:.1f} dBFS"


class PlotCanvas(QtWidgets.QWidget):
    def __init__(
        self,
        title: str,
        y_min: float,
        y_max: float,
        *,
        show_x_labels: bool = True,
        show_legend: bool = True,
        parent: QtWidgets.QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self.setMinimumHeight(220)
        self._title = title
        self._y_min = y_min
        self._y_max = y_max
        self._show_x_labels = show_x_labels
        self._show_legend = show_legend
        self._x_values: list[float] = []
        self._series: list[tuple[str, QtGui.QColor, list[float]]] = []
        self._noise_floor: float | None = None
        self._annotations: list[tuple[float, str, QtGui.QColor]] = []

    def set_data(
        self,
        x_values: Sequence[float],
        series: Sequence[tuple[str, str | QtGui.QColor, Sequence[float]]],
        *,
        noise_floor: float | None = None,
        annotations: Sequence[tuple[float, str, str | QtGui.QColor]] = (),
    ) -> None:
        self._x_values = list(x_values)
        self._series = [
            (name, color if isinstance(color, QtGui.QColor) else QtGui.QColor(color), list(values))
            for name, color, values in series
        ]
        self._noise_floor = noise_floor
        self._annotations = [
            (position, label, color if isinstance(color, QtGui.QColor) else QtGui.QColor(color))
            for position, label, color in annotations
        ]
        self.update()

    def _plot_rect(self) -> QtCore.QRect:
        margin_left = 80
        margin_top = 32
        margin_right = 18
        margin_bottom = 30 if self._show_x_labels else 16
        return self.rect().adjusted(margin_left, margin_top, -margin_right, -margin_bottom)

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:  # noqa: N802
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.RenderHint.Antialiasing, True)
        painter.fillRect(self.rect(), QtGui.QColor("#111318"))

        plot_rect = self._plot_rect()
        if plot_rect.width() <= 1 or plot_rect.height() <= 1:
            return

        painter.setPen(QtGui.QPen(QtGui.QColor("#20252d"), 1))
        painter.drawRect(plot_rect)

        title_font = painter.font()
        title_font.setBold(True)
        title_font.setPointSize(title_font.pointSize() + 2)
        painter.setFont(title_font)
        painter.setPen(QtGui.QColor("#f7f7f2"))
        painter.drawText(12, 22, self._title)

        painter.setFont(QtGui.QFont())
        painter.setPen(QtGui.QColor("#48505d"))
        for index in range(1, 5):
            y = plot_rect.top() + int(plot_rect.height() * index / 5)
            painter.drawLine(plot_rect.left(), y, plot_rect.right(), y)

        painter.setPen(QtGui.QColor("#8f99a8"))
        y_ticks = 5
        for index in range(y_ticks + 1):
            ratio = index / y_ticks
            value = self._y_max - (self._y_max - self._y_min) * ratio
            y = plot_rect.top() + int(plot_rect.height() * ratio)
            painter.drawText(8, y + 4, 68, 16, QtCore.Qt.AlignmentFlag.AlignRight, _format_dbfs(value))
            painter.drawLine(plot_rect.left() - 4, y, plot_rect.left(), y)

        if not self._x_values or not self._series:
            painter.setPen(QtGui.QColor("#8f99a8"))
            painter.drawText(plot_rect, QtCore.Qt.AlignmentFlag.AlignCenter, "No data")
            return

        x_min = self._x_values[0]
        x_max = self._x_values[-1] if len(self._x_values) > 1 else self._x_values[0] + 1.0
        if x_max <= x_min:
            x_max = x_min + 1.0

        def x_to_pixel(value: float) -> float:
            return plot_rect.left() + (value - x_min) * plot_rect.width() / (x_max - x_min)

        def y_to_pixel(value: float) -> float:
            clamped = _clamp(value, self._y_min, self._y_max)
            return plot_rect.bottom() - (clamped - self._y_min) * plot_rect.height() / (self._y_max - self._y_min)

        if self._noise_floor is not None:
            noise_y = y_to_pixel(self._noise_floor)
            painter.setPen(QtGui.QPen(QtGui.QColor("#d08c60"), 1, QtCore.Qt.PenStyle.DashLine))
            painter.drawLine(plot_rect.left(), int(noise_y), plot_rect.right(), int(noise_y))
            painter.drawText(plot_rect.right() - 110, int(noise_y) - 4, f"noise {_format_dbfs(self._noise_floor)}")

        for index, (position, label, color) in enumerate(self._annotations[:8]):
            if position < x_min or position > x_max:
                continue
            x = int(x_to_pixel(position))
            painter.setPen(QtGui.QPen(color, 1, QtCore.Qt.PenStyle.DashLine))
            painter.drawLine(x, plot_rect.top(), x, plot_rect.bottom())
            painter.save()
            painter.translate(x + 4, plot_rect.top() + 12 + index * 14)
            painter.setPen(color)
            painter.drawText(0, 0, label)
            painter.restore()

        for series_index, (name, color, values) in enumerate(self._series):
            if not values:
                continue
            pen = QtGui.QPen(color, 2 if series_index == 0 else 1)
            painter.setPen(pen)
            path = QtGui.QPainterPath()
            count = min(len(values), len(self._x_values))
            if count == 0:
                continue
            first_x = x_to_pixel(self._x_values[0])
            first_y = y_to_pixel(values[0])
            path.moveTo(first_x, first_y)
            for index in range(1, count):
                path.lineTo(x_to_pixel(self._x_values[index]), y_to_pixel(values[index]))
            painter.drawPath(path)

        if self._show_legend:
            legend_x = plot_rect.right() - 210
            legend_y = plot_rect.top() + 16
            for name, color, _ in self._series:
                painter.setPen(QtGui.QPen(color, 3))
                painter.drawLine(legend_x, legend_y, legend_x + 18, legend_y)
                painter.setPen(QtGui.QColor("#d8dbe2"))
                painter.drawText(legend_x + 24, legend_y + 4, name)
                legend_y += 16

        if self._show_x_labels:
            painter.setPen(QtGui.QColor("#8f99a8"))
            painter.drawText(
                plot_rect.left(),
                plot_rect.bottom() + 18,
                plot_rect.width(),
                16,
                QtCore.Qt.AlignmentFlag.AlignHCenter,
                self._x_label_text(),
            )

    def _x_label_text(self) -> str:
        return "Frequency" if self._title == "Spectrum" else "Sample Index"


class WaterfallCanvas(QtWidgets.QWidget):
    def __init__(self, rows: int = 220, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(parent)
        self.setMinimumHeight(240)
        self._title = "Waterfall"
        self._rows = rows
        self._min_dbfs = -140.0
        self._max_dbfs = 0.0
        self._row_width = 0
        self._image = QtGui.QImage()
        self._palette = self._build_palette()

    def set_fft_size(self, fft_size: int) -> None:
        self._row_width = fft_size
        self._image = QtGui.QImage(fft_size, self._rows, QtGui.QImage.Format.Format_RGB32)
        self._image.fill(QtGui.QColor("#000000"))
        self.update()

    def append_row(self, values: Sequence[float]) -> None:
        if not values:
            return
        if self._row_width != len(values) or self._image.isNull():
            self.set_fft_size(len(values))

        new_image = QtGui.QImage(self._row_width, self._rows, QtGui.QImage.Format.Format_RGB32)
        new_image.fill(QtGui.QColor("#000000"))
        painter = QtGui.QPainter(new_image)
        painter.drawImage(
            QtCore.QRectF(0.0, 0.0, float(self._row_width), float(self._rows - 1)),
            self._image,
            QtCore.QRectF(0.0, 1.0, float(self._row_width), float(self._rows - 1)),
        )
        bottom_y = self._rows - 1
        for x, value in enumerate(values):
            painter.setPen(self._palette[self._color_index(value)])
            painter.drawPoint(x, bottom_y)
        painter.end()
        self._image = new_image
        self.update()

    def paintEvent(self, event: QtGui.QPaintEvent) -> None:  # noqa: N802
        painter = QtGui.QPainter(self)
        painter.setRenderHint(QtGui.QPainter.RenderHint.SmoothPixmapTransform, True)
        painter.fillRect(self.rect(), QtGui.QColor("#0e1015"))

        plot_rect = self.rect().adjusted(80, 32, -18, -24)
        painter.setPen(QtGui.QPen(QtGui.QColor("#20252d"), 1))
        painter.drawRect(plot_rect)
        painter.setPen(QtGui.QColor("#f7f7f2"))
        painter.drawText(12, 22, self._title)

        if self._image.isNull():
            painter.setPen(QtGui.QColor("#8f99a8"))
            painter.drawText(plot_rect, QtCore.Qt.AlignmentFlag.AlignCenter, "No waterfall data")
            return

        painter.drawImage(plot_rect, self._image)
        painter.setPen(QtGui.QColor("#8f99a8"))
        painter.drawText(
            plot_rect.left(),
            plot_rect.bottom() + 18,
            plot_rect.width(),
            16,
            QtCore.Qt.AlignmentFlag.AlignHCenter,
            "Frequency",
        )

    def _color_index(self, value: float) -> int:
        normalized = (value - self._min_dbfs) / (self._max_dbfs - self._min_dbfs)
        return int(_clamp(normalized, 0.0, 1.0) * (len(self._palette) - 1))

    @staticmethod
    def _build_palette() -> list[QtGui.QColor]:
        palette: list[QtGui.QColor] = []
        for index in range(256):
            t = index / 255.0
            if t < 0.25:
                local = t / 0.25
                r, g, b = 0.0, 0.0, 40.0 + 180.0 * local
            elif t < 0.5:
                local = (t - 0.25) / 0.25
                r, g, b = 0.0, 80.0 + 175.0 * local, 220.0 - 140.0 * local
            elif t < 0.75:
                local = (t - 0.5) / 0.25
                r, g, b = 20.0 + 215.0 * local, 255.0, 80.0 - 80.0 * local
            else:
                local = (t - 0.75) / 0.25
                r, g, b = 235.0 + 20.0 * local, 255.0 - 90.0 * local, 0.0 + 255.0 * local
            palette.append(QtGui.QColor(int(r), int(g), int(b)))
        return palette


class DetectionTable(QtWidgets.QTableWidget):
    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(0, 4, parent)
        self.setHorizontalHeaderLabels(["Frequency", "Bandwidth", "Power", "Labels"])
        self.horizontalHeader().setStretchLastSection(True)
        self.verticalHeader().setVisible(False)
        self.setAlternatingRowColors(True)
        self.setEditTriggers(QtWidgets.QAbstractItemView.EditTrigger.NoEditTriggers)
        self.setSelectionBehavior(QtWidgets.QAbstractItemView.SelectionBehavior.SelectRows)
        self.setMaximumHeight(220)

    def set_detections(self, detections: Sequence[object]) -> None:
        self.setRowCount(len(detections))
        for row, detection in enumerate(detections):
            values = [
                _format_hz(getattr(detection, "center_frequency_hz", 0.0)),
                _format_hz(getattr(detection, "bandwidth_hz", 0.0)),
                _format_dbfs(getattr(detection, "peak_power_dbfs", 0.0)),
                ", ".join(getattr(detection, "labels", [])),
            ]
            for column, value in enumerate(values):
                item = QtWidgets.QTableWidgetItem(value)
                if column == 0:
                    item.setTextAlignment(QtCore.Qt.AlignmentFlag.AlignRight | QtCore.Qt.AlignmentFlag.AlignVCenter)
                self.setItem(row, column, item)
        self.resizeColumnsToContents()


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle("SDR Signal Analyzer")

        source = SourceConfig()
        source.kind = SourceKind.SIMULATOR
        processing = ProcessingConfig()
        processing.display_samples = 2048
        self._session = AnalyzerSession(source, processing)
        self._markers: list[Marker] = [Marker()]
        self._markers[0].center_frequency_hz = source.center_frequency_hz
        self._markers[0].bandwidth_hz = 200_000.0

        central = QtWidgets.QWidget()
        layout = QtWidgets.QVBoxLayout(central)
        layout.setSpacing(10)

        controls = QtWidgets.QGridLayout()
        self._source_kind = QtWidgets.QComboBox()
        self._source_kind.addItem("Simulator", SourceKind.SIMULATOR)
        self._source_kind.addItem("Replay", SourceKind.REPLAY)
        self._source_kind.addItem("rtl_tcp", SourceKind.RTL_TCP)
        self._source_kind.addItem("UHD", SourceKind.UHD)
        self._source_kind.addItem("SoapySDR", SourceKind.SOAPY)
        self._source_kind.currentIndexChanged.connect(self._refresh_source_controls)
        self._center_edit = QtWidgets.QLineEdit("100000000")
        self._rate_edit = QtWidgets.QLineEdit("2400000")
        self._gain_edit = QtWidgets.QLineEdit("10")
        self._fft_edit = QtWidgets.QLineEdit(str(processing.fft_size))
        self._input_path_edit = QtWidgets.QLineEdit("")
        self._metadata_path_edit = QtWidgets.QLineEdit("")
        self._loop_checkbox = QtWidgets.QCheckBox("Loop Replay")
        self._host_edit = QtWidgets.QLineEdit("127.0.0.1")
        self._port_edit = QtWidgets.QLineEdit("1234")
        self._device_string_edit = QtWidgets.QLineEdit("")
        self._device_args_edit = QtWidgets.QLineEdit("")
        self._channel_edit = QtWidgets.QLineEdit("0")
        self._antenna_edit = QtWidgets.QLineEdit("")
        self._bandwidth_edit = QtWidgets.QLineEdit("")
        self._clock_source_edit = QtWidgets.QLineEdit("")
        self._time_source_edit = QtWidgets.QLineEdit("")
        self._marker_center_edit = QtWidgets.QLineEdit("100000000")
        self._marker_bw_edit = QtWidgets.QLineEdit("200000")
        self._start_button = QtWidgets.QPushButton("Start")
        self._start_button.clicked.connect(self._toggle_stream)
        self._marker_button = QtWidgets.QPushButton("Update Marker")
        self._marker_button.clicked.connect(self._update_marker)

        self._source_controls: list[tuple[QtWidgets.QLabel, QtWidgets.QWidget, set[SourceKind] | None]] = []

        def add_control(
            index: int,
            label_text: str,
            widget: QtWidgets.QWidget,
            visible_for: set[SourceKind] | None = None,
        ) -> None:
            row = index // 4
            column = (index % 4) * 2
            label = QtWidgets.QLabel(label_text)
            controls.addWidget(label, row, column)
            controls.addWidget(widget, row, column + 1)
            self._source_controls.append((label, widget, visible_for))

        add_control(0, "Source", self._source_kind)
        add_control(1, "Center Hz", self._center_edit)
        add_control(2, "Sample Rate", self._rate_edit)
        add_control(3, "Gain dB", self._gain_edit)
        add_control(4, "FFT Size", self._fft_edit)
        add_control(5, "Replay Input", self._input_path_edit, {SourceKind.REPLAY})
        add_control(6, "Replay Metadata", self._metadata_path_edit, {SourceKind.REPLAY})
        add_control(7, "rtl_tcp Host", self._host_edit, {SourceKind.RTL_TCP})
        add_control(8, "rtl_tcp Port", self._port_edit, {SourceKind.RTL_TCP})
        add_control(9, "Soapy Device", self._device_string_edit, {SourceKind.SOAPY})
        add_control(10, "UHD Device Args", self._device_args_edit, {SourceKind.UHD})
        add_control(11, "Channel", self._channel_edit, {SourceKind.UHD, SourceKind.SOAPY})
        add_control(12, "Antenna", self._antenna_edit, {SourceKind.UHD, SourceKind.SOAPY})
        add_control(13, "Bandwidth Hz", self._bandwidth_edit, {SourceKind.UHD, SourceKind.SOAPY})
        add_control(14, "Clock Source", self._clock_source_edit, {SourceKind.UHD})
        add_control(15, "Time Source", self._time_source_edit, {SourceKind.UHD})
        add_control(16, "Marker Hz", self._marker_center_edit)
        add_control(17, "Marker BW", self._marker_bw_edit)

        loop_label = QtWidgets.QLabel("Replay Loop")
        controls.addWidget(loop_label, 4, 0)
        controls.addWidget(self._loop_checkbox, 4, 1)
        self._source_controls.append((loop_label, self._loop_checkbox, {SourceKind.REPLAY}))

        controls.addWidget(self._start_button, 4, 2, 1, 2)
        controls.addWidget(self._marker_button, 4, 4, 1, 2)
        layout.addLayout(controls)

        self._spectrum_plot = PlotCanvas("Spectrum", -140.0, 0.0, show_x_labels=True, show_legend=True)
        self._waterfall = WaterfallCanvas(rows=220)
        self._time_plot = PlotCanvas("Time Domain", -1.5, 1.5, show_x_labels=True, show_legend=True)
        self._detection_table = DetectionTable()
        self._status_label = QtWidgets.QLabel("Idle")
        self._status_label.setStyleSheet("color: #d8dbe2; font-weight: 600;")

        layout.addWidget(self._spectrum_plot, stretch=3)
        layout.addWidget(self._waterfall, stretch=3)
        layout.addWidget(self._time_plot, stretch=2)
        layout.addWidget(self._detection_table, stretch=1)
        layout.addWidget(self._status_label)
        self.setCentralWidget(central)

        self._timer = QtCore.QTimer(self)
        self._timer.setInterval(40)
        self._timer.timeout.connect(self._poll_session)

        self._apply_palette()
        self._waterfall.set_fft_size(processing.fft_size)
        self._refresh_source_controls()

    def _apply_palette(self) -> None:
        self.setStyleSheet(
            """
            QWidget {
                background: #111318;
                color: #e8eaed;
                font-size: 12px;
            }
            QLineEdit, QTableWidget, QPushButton, QComboBox {
                background: #171b22;
                border: 1px solid #2a313d;
                border-radius: 6px;
                padding: 4px 6px;
            }
            QCheckBox {
                spacing: 8px;
            }
            QPushButton:hover {
                border-color: #4c5668;
            }
            QHeaderView::section {
                background: #1e2430;
                border: 0;
                padding: 4px 6px;
            }
            """
        )

    def closeEvent(self, event: QtGui.QCloseEvent) -> None:  # noqa: N802
        self._session.stop()
        super().closeEvent(event)

    def _refresh_source_controls(self) -> None:
        current_kind = self._source_kind.currentData()
        for label, widget, visible_for in self._source_controls:
            visible = visible_for is None or current_kind in visible_for
            label.setVisible(visible)
            widget.setVisible(visible)

    @staticmethod
    def _parse_int(text: str, default: int = 0) -> int:
        stripped = text.strip()
        return int(stripped) if stripped else default

    @staticmethod
    def _parse_float(text: str, default: float = 0.0) -> float:
        stripped = text.strip()
        return float(stripped) if stripped else default

    def _update_marker(self) -> None:
        marker = Marker()
        marker.center_frequency_hz = float(self._marker_center_edit.text())
        marker.bandwidth_hz = float(self._marker_bw_edit.text())
        self._markers = [marker]
        self._session.set_markers(self._markers)
        self._status_label.setText(
            f"Marker set at {_format_hz(marker.center_frequency_hz)} with {_format_hz(marker.bandwidth_hz)} span"
        )

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
        source.kind = self._source_kind.currentData()
        source.input_path = self._input_path_edit.text().strip()
        source.metadata_path = self._metadata_path_edit.text().strip()
        source.loop_playback = self._loop_checkbox.isChecked()
        source.network_host = self._host_edit.text().strip()
        source.network_port = self._parse_int(self._port_edit.text(), 1234)
        source.device_string = self._device_string_edit.text().strip()
        source.device_args = self._device_args_edit.text().strip()
        source.channel = max(0, self._parse_int(self._channel_edit.text(), 0))
        source.antenna = self._antenna_edit.text().strip()
        source.bandwidth_hz = self._parse_float(self._bandwidth_edit.text(), 0.0)
        source.clock_source = self._clock_source_edit.text().strip()
        source.time_source = self._time_source_edit.text().strip()
        if not self._session.update_source_config(source):
            self._status_label.setText(f"Source update failed: {self._session.last_error()}")
            return

        processing = self._session.processing_config()
        processing.fft_size = int(self._fft_edit.text())
        processing.display_samples = min(processing.fft_size, 2048)
        self._session.update_processing_config(processing)

        self._waterfall.set_fft_size(processing.fft_size)
        self._session.set_markers(self._markers)
        if not self._session.start():
            self._status_label.setText(f"Start failed: {self._session.last_error()}")
            return

        self._timer.start()
        self._start_button.setText("Stop")
        self._status_label.setText("Running")

    def _poll_session(self) -> None:
        snapshot = self._session.poll_snapshot()
        if snapshot is None:
            return

        spectrum = snapshot.spectrum
        detections = snapshot.analysis.detections
        markers = snapshot.analysis.marker_measurements

        x_values = list(
            (index - (len(spectrum.power_dbfs) / 2.0)) * spectrum.bin_resolution_hz + spectrum.center_frequency_hz
            for index in range(len(spectrum.power_dbfs))
        )
        self._spectrum_plot.set_data(
            x_values,
            [
                ("Average", "#ff7a45", spectrum.average_dbfs),
                ("Peak Hold", "#4ea3f1", spectrum.peak_hold_dbfs),
            ],
            noise_floor=snapshot.analysis.noise_floor_dbfs,
            annotations=[
                (
                    detection.center_frequency_hz,
                    f"{detection.labels[0]} {_format_hz(detection.bandwidth_hz)}",
                    "#d08c60",
                )
                for detection in detections[:6]
            ]
            + [
                (
                    marker.center_frequency_hz,
                    f"marker {_format_dbfs(marker.peak_power_dbfs)} avg {_format_dbfs(marker.average_power_dbfs)}",
                    "#f1b24a",
                )
                for marker in markers
            ],
        )

        self._waterfall.append_row(spectrum.power_dbfs)

        self._time_plot.set_data(
            list(range(len(snapshot.time_domain.i))),
            [
                ("I", "#4ea3f1", snapshot.time_domain.i),
                ("Q", "#3ecf8e", snapshot.time_domain.q),
                ("Magnitude", "#ff7a45", snapshot.time_domain.magnitude),
            ],
        )

        self._detection_table.set_detections(detections[:12])
        labels = ", ".join(
            f"{det.labels[0]} @ {_format_hz(det.center_frequency_hz)} / {_format_hz(det.bandwidth_hz)}"
            for det in detections[:3]
        )
        self._status_label.setText(
            "Noise "
            f"{_format_dbfs(snapshot.analysis.noise_floor_dbfs)} | "
            f"Peak {_format_dbfs(snapshot.analysis.strongest_peak_dbfs)} | "
            f"{labels or 'no detections'}"
        )


def main() -> int:
    app = QtWidgets.QApplication(sys.argv)
    window = MainWindow()
    window.resize(1600, 1050)
    window.show()
    return app.exec()

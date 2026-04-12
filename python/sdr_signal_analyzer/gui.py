from __future__ import annotations

from dataclasses import dataclass
import sys
from math import isfinite
from typing import Sequence

from . import AnalyzerSession, Marker, ProcessingConfig, SourceConfig, SourceKind

try:
    from PySide6 import QtCore, QtGui, QtWidgets
except ImportError as exc:  # pragma: no cover - runtime guard
    raise RuntimeError(
        "GUI dependencies are missing. Install PySide6 to run the Qt frontend."
    ) from exc


DEFAULT_WINDOW_TITLE = "SDR Signal Analyzer"
DEFAULT_WINDOW_WIDTH = 1600
DEFAULT_WINDOW_HEIGHT = 1050
DEFAULT_CONTROL_SPACING = 10
DEFAULT_PANEL_HEIGHT = 220
DEFAULT_WATERFALL_HEIGHT = 240
DEFAULT_WATERFALL_ROWS = 220
DEFAULT_TIMER_INTERVAL_MS = 40
DEFAULT_DISPLAY_SAMPLES = 2048
DEFAULT_FFT_SIZE = 2048
DEFAULT_SOURCE_CENTER_HZ = 100_000_000.0
DEFAULT_SAMPLE_RATE_HZ = 2_400_000.0
DEFAULT_GAIN_DB = 10.0
DEFAULT_MARKER_CENTER_HZ = 100_000_000.0
DEFAULT_MARKER_BANDWIDTH_HZ = 200_000.0
DEFAULT_REPLAY_HOST = "127.0.0.1"
DEFAULT_REPLAY_PORT = "1234"
DEFAULT_CHANNEL = "0"
DEFAULT_EMPTY_TEXT = ""
DEFAULT_PLOT_Y_MIN = -140.0
DEFAULT_PLOT_Y_MAX = 0.0
DEFAULT_TIME_Y_MIN = -1.5
DEFAULT_TIME_Y_MAX = 1.5
DEFAULT_PLOT_MARGIN_LEFT = 80
DEFAULT_PLOT_MARGIN_TOP = 32
DEFAULT_PLOT_MARGIN_RIGHT = 18
DEFAULT_PLOT_MARGIN_BOTTOM_WITH_LABELS = 30
DEFAULT_PLOT_MARGIN_BOTTOM_WITHOUT_LABELS = 16
DEFAULT_PLOT_Y_LABEL_WIDTH = 68
DEFAULT_PLOT_NOISE_LABEL_WIDTH = 110
DEFAULT_LEGEND_WIDTH = 210
DEFAULT_WATERFALL_MARGIN_LEFT = 80
DEFAULT_WATERFALL_MARGIN_TOP = 32
DEFAULT_WATERFALL_MARGIN_RIGHT = 18
DEFAULT_WATERFALL_MARGIN_BOTTOM = 24
DEFAULT_WATERFALL_COLOR_STEPS = 256
DEFAULT_MAX_ANNOTATIONS = 8
DEFAULT_MAX_DETECTION_ANNOTATIONS = 6
DEFAULT_MAX_DETECTION_ROWS = 12
DEFAULT_STATUS_DETECTIONS = 3
DEFAULT_MARKER_BANDWIDTH_HZ = 200_000.0


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


_STATUS_STYLE = "color: #d8dbe2; font-weight: 600;"
_STATUS_ERROR_STYLE = "color: #ff8a80; font-weight: 600;"


@dataclass(frozen=True)
class MarkerEntry:
    """Editable GUI representation of a marker."""

    name: str
    center_frequency_hz: float
    bandwidth_hz: float


class MarkerEditorDialog(QtWidgets.QDialog):
    """Modal editor for marker names and frequency spans."""

    def __init__(
        self,
        markers: Sequence[MarkerEntry],
        parent: QtWidgets.QWidget | None = None,
    ) -> None:
        super().__init__(parent)
        self.setWindowTitle("Edit Markers")
        self.resize(760, 360)
        self._markers = list(markers)

        self._table = QtWidgets.QTableWidget(0, 3, self)
        self._table.setHorizontalHeaderLabels(["Name", "Center Hz", "Bandwidth Hz"])
        self._table.horizontalHeader().setStretchLastSection(True)
        self._table.verticalHeader().setVisible(False)
        self._table.setAlternatingRowColors(True)
        self._table.setSelectionBehavior(
            QtWidgets.QAbstractItemView.SelectionBehavior.SelectRows
        )
        self._table.setSelectionMode(
            QtWidgets.QAbstractItemView.SelectionMode.ExtendedSelection
        )
        self._table.setEditTriggers(
            QtWidgets.QAbstractItemView.EditTrigger.DoubleClicked
            | QtWidgets.QAbstractItemView.EditTrigger.SelectedClicked
            | QtWidgets.QAbstractItemView.EditTrigger.EditKeyPressed
        )

        self._message_label = QtWidgets.QLabel("")
        self._message_label.setWordWrap(True)
        self._message_label.setStyleSheet(_STATUS_ERROR_STYLE)
        self._message_label.setVisible(False)

        self._add_button = QtWidgets.QPushButton("Add Row")
        self._add_button.clicked.connect(self._add_row)
        self._remove_button = QtWidgets.QPushButton("Remove Selected")
        self._remove_button.clicked.connect(self._remove_selected_rows)

        self._button_box = QtWidgets.QDialogButtonBox(
            QtWidgets.QDialogButtonBox.StandardButton.Ok
            | QtWidgets.QDialogButtonBox.StandardButton.Cancel
        )
        self._button_box.accepted.connect(self.accept)
        self._button_box.rejected.connect(self.reject)

        layout = QtWidgets.QVBoxLayout(self)
        layout.addWidget(
            QtWidgets.QLabel(
                "Edit marker rows directly. The name stays in the GUI; frequency values "
                "are forwarded to the session."
            )
        )
        layout.addWidget(self._table)

        toolbar = QtWidgets.QHBoxLayout()
        toolbar.addWidget(self._add_button)
        toolbar.addWidget(self._remove_button)
        toolbar.addStretch(1)
        layout.addLayout(toolbar)
        layout.addWidget(self._message_label)
        layout.addWidget(self._button_box)

        for marker in self._markers:
            self._add_row(marker)
        if not self._markers:
            self._add_row(
                MarkerEntry("Marker 1", DEFAULT_MARKER_CENTER_HZ, DEFAULT_MARKER_BANDWIDTH_HZ)
            )

    def _set_message(self, message: str = "") -> None:
        self._message_label.setText(message)
        self._message_label.setVisible(bool(message))

    @staticmethod
    def _new_item(
        text: str,
        *,
        alignment: QtCore.Qt.AlignmentFlag | None = None,
    ) -> QtWidgets.QTableWidgetItem:
        item = QtWidgets.QTableWidgetItem(text)
        item.setFlags(
            QtCore.Qt.ItemFlag.ItemIsSelectable
            | QtCore.Qt.ItemFlag.ItemIsEnabled
            | QtCore.Qt.ItemFlag.ItemIsEditable
        )
        if alignment is not None:
            item.setTextAlignment(alignment)
        return item

    def _add_row(self, marker: MarkerEntry | None = None) -> None:
        row = self._table.rowCount()
        self._table.insertRow(row)
        marker = marker or MarkerEntry(
            f"Marker {row + 1}",
            DEFAULT_MARKER_CENTER_HZ,
            DEFAULT_MARKER_BANDWIDTH_HZ,
        )
        self._table.setItem(row, 0, self._new_item(marker.name))
        self._table.setItem(
            row,
            1,
            self._new_item(
                f"{marker.center_frequency_hz:.6f}",
                alignment=QtCore.Qt.AlignmentFlag.AlignRight
                | QtCore.Qt.AlignmentFlag.AlignVCenter,
            ),
        )
        self._table.setItem(
            row,
            2,
            self._new_item(
                f"{marker.bandwidth_hz:.6f}",
                alignment=QtCore.Qt.AlignmentFlag.AlignRight
                | QtCore.Qt.AlignmentFlag.AlignVCenter,
            ),
        )

    def _remove_selected_rows(self) -> None:
        rows = sorted(
            {index.row() for index in self._table.selectionModel().selectedRows()},
            reverse=True,
        )
        for row in rows:
            self._table.removeRow(row)
        self._set_message()

    def _read_cell_text(self, row: int, column: int, label: str) -> str:
        item = self._table.item(row, column)
        if item is None:
            raise ValueError(f"Row {row + 1} {label} is required.")
        text = item.text().strip()
        if not text:
            raise ValueError(f"Row {row + 1} {label} is required.")
        return text

    def _collect_markers(self) -> list[MarkerEntry]:
        markers: list[MarkerEntry] = []
        for row in range(self._table.rowCount()):
            name = self._read_cell_text(row, 0, "Name")
            center = self._read_cell_text(row, 1, "Center Hz")
            bandwidth = self._read_cell_text(row, 2, "Bandwidth Hz")
            try:
                center_frequency_hz = float(center)
            except ValueError as exc:
                raise ValueError(f"Row {row + 1} Center Hz must be a number.") from exc
            try:
                bandwidth_hz = float(bandwidth)
            except ValueError as exc:
                raise ValueError(
                    f"Row {row + 1} Bandwidth Hz must be a number."
                ) from exc
            if not isfinite(center_frequency_hz) or center_frequency_hz <= 0.0:
                raise ValueError(f"Row {row + 1} Center Hz must be greater than 0.")
            if not isfinite(bandwidth_hz) or bandwidth_hz <= 0.0:
                raise ValueError(f"Row {row + 1} Bandwidth Hz must be greater than 0.")
            markers.append(MarkerEntry(name, center_frequency_hz, bandwidth_hz))
        return markers

    def markers(self) -> list[MarkerEntry]:
        """Return the validated marker rows."""

        return list(self._markers)

    def accept(self) -> None:  # noqa: D401
        try:
            self._markers = self._collect_markers()
        except ValueError as exc:
            self._set_message(str(exc))
            return
        super().accept()


class PlotCanvas(QtWidgets.QWidget):
    """Reusable canvas for spectrum and time-domain traces."""

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
        self.setMinimumHeight(DEFAULT_PANEL_HEIGHT)
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
            (
                name,
                color if isinstance(color, QtGui.QColor) else QtGui.QColor(color),
                list(values),
            )
            for name, color, values in series
        ]
        self._noise_floor = noise_floor
        self._annotations = [
            (
                position,
                label,
                color if isinstance(color, QtGui.QColor) else QtGui.QColor(color),
            )
            for position, label, color in annotations
        ]
        self.update()

    def _plot_rect(self) -> QtCore.QRect:
        margin_left = DEFAULT_PLOT_MARGIN_LEFT
        margin_top = DEFAULT_PLOT_MARGIN_TOP
        margin_right = DEFAULT_PLOT_MARGIN_RIGHT
        margin_bottom = (
            DEFAULT_PLOT_MARGIN_BOTTOM_WITH_LABELS
            if self._show_x_labels
            else DEFAULT_PLOT_MARGIN_BOTTOM_WITHOUT_LABELS
        )
        return self.rect().adjusted(
            margin_left, margin_top, -margin_right, -margin_bottom
        )

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
            painter.drawText(
                8,
                y + 4,
                DEFAULT_PLOT_Y_LABEL_WIDTH,
                16,
                QtCore.Qt.AlignmentFlag.AlignRight,
                _format_dbfs(value),
            )
            painter.drawLine(plot_rect.left() - 4, y, plot_rect.left(), y)

        if not self._x_values or not self._series:
            painter.setPen(QtGui.QColor("#8f99a8"))
            painter.drawText(plot_rect, QtCore.Qt.AlignmentFlag.AlignCenter, "No data")
            return

        x_min = self._x_values[0]
        x_max = (
            self._x_values[-1] if len(self._x_values) > 1 else self._x_values[0] + 1.0
        )
        if x_max <= x_min:
            x_max = x_min + 1.0

        def x_to_pixel(value: float) -> float:
            return plot_rect.left() + (value - x_min) * plot_rect.width() / (
                x_max - x_min
            )

        def y_to_pixel(value: float) -> float:
            clamped = _clamp(value, self._y_min, self._y_max)
            return plot_rect.bottom() - (clamped - self._y_min) * plot_rect.height() / (
                self._y_max - self._y_min
            )

        if self._noise_floor is not None:
            noise_y = y_to_pixel(self._noise_floor)
            painter.setPen(
                QtGui.QPen(QtGui.QColor("#d08c60"), 1, QtCore.Qt.PenStyle.DashLine)
            )
            painter.drawLine(
                plot_rect.left(), int(noise_y), plot_rect.right(), int(noise_y)
            )
            painter.drawText(
                plot_rect.right() - DEFAULT_PLOT_NOISE_LABEL_WIDTH,
                int(noise_y) - 4,
                f"noise {_format_dbfs(self._noise_floor)}",
            )

        for index, (position, label, color) in enumerate(
            self._annotations[:DEFAULT_MAX_ANNOTATIONS]
        ):
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
                path.lineTo(
                    x_to_pixel(self._x_values[index]), y_to_pixel(values[index])
                )
            painter.drawPath(path)

        if self._show_legend:
            legend_x = plot_rect.right() - DEFAULT_LEGEND_WIDTH
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
    """Waterfall view with a fixed-size rolling row buffer."""

    def __init__(
        self, rows: int = DEFAULT_WATERFALL_ROWS, parent: QtWidgets.QWidget | None = None
    ) -> None:
        super().__init__(parent)
        self.setMinimumHeight(DEFAULT_WATERFALL_HEIGHT)
        self._title = "Waterfall"
        self._rows = rows
        self._min_dbfs = DEFAULT_PLOT_Y_MIN
        self._max_dbfs = DEFAULT_PLOT_Y_MAX
        self._row_width = 0
        self._image = QtGui.QImage()
        self._palette = self._build_palette()

    def set_fft_size(self, fft_size: int) -> None:
        self._row_width = fft_size
        self._image = QtGui.QImage(
            fft_size, self._rows, QtGui.QImage.Format.Format_RGB32
        )
        self._image.fill(QtGui.QColor("#000000"))
        self.update()

    def append_row(self, values: Sequence[float]) -> None:
        if not values:
            return
        if self._row_width != len(values) or self._image.isNull():
            self.set_fft_size(len(values))

        new_image = QtGui.QImage(
            self._row_width, self._rows, QtGui.QImage.Format.Format_RGB32
        )
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

        plot_rect = self.rect().adjusted(
            DEFAULT_WATERFALL_MARGIN_LEFT,
            DEFAULT_WATERFALL_MARGIN_TOP,
            -DEFAULT_WATERFALL_MARGIN_RIGHT,
            -DEFAULT_WATERFALL_MARGIN_BOTTOM,
        )
        painter.setPen(QtGui.QPen(QtGui.QColor("#20252d"), 1))
        painter.drawRect(plot_rect)
        painter.setPen(QtGui.QColor("#f7f7f2"))
        painter.drawText(12, 22, self._title)

        if self._image.isNull():
            painter.setPen(QtGui.QColor("#8f99a8"))
            painter.drawText(
                plot_rect, QtCore.Qt.AlignmentFlag.AlignCenter, "No waterfall data"
            )
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
        for index in range(DEFAULT_WATERFALL_COLOR_STEPS):
            t = index / float(DEFAULT_WATERFALL_COLOR_STEPS - 1)
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
                r, g, b = (
                    235.0 + 20.0 * local,
                    255.0 - 90.0 * local,
                    0.0 + 255.0 * local,
                )
            palette.append(QtGui.QColor(int(r), int(g), int(b)))
        return palette


class DetectionTable(QtWidgets.QTableWidget):
    """Tabular view of detections and their labels."""

    def __init__(self, parent: QtWidgets.QWidget | None = None) -> None:
        super().__init__(0, 4, parent)
        self.setHorizontalHeaderLabels(["Frequency", "Bandwidth", "Power", "Labels"])
        self.horizontalHeader().setStretchLastSection(True)
        self.verticalHeader().setVisible(False)
        self.setAlternatingRowColors(True)
        self.setEditTriggers(QtWidgets.QAbstractItemView.EditTrigger.NoEditTriggers)
        self.setSelectionBehavior(
            QtWidgets.QAbstractItemView.SelectionBehavior.SelectRows
        )
        self.setMaximumHeight(DEFAULT_PANEL_HEIGHT)

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
                    item.setTextAlignment(
                        QtCore.Qt.AlignmentFlag.AlignRight
                        | QtCore.Qt.AlignmentFlag.AlignVCenter
                    )
                self.setItem(row, column, item)
        self.resizeColumnsToContents()


class MainWindow(QtWidgets.QMainWindow):
    """Main Qt frontend for live inspection and control."""

    def __init__(self) -> None:
        super().__init__()
        self.setWindowTitle(DEFAULT_WINDOW_TITLE)

        _source, processing = self._create_default_session_state()
        self._create_controls(processing)
        self._build_controls_layout()
        self._create_visualization_widgets()
        self._assemble_main_layout()
        self._create_timer()
        self._apply_palette()
        self._configure_validators()
        self._waterfall.set_fft_size(processing.fft_size)
        self._refresh_source_controls()

    def _create_default_session_state(self) -> tuple[SourceConfig, ProcessingConfig]:
        source = SourceConfig()
        source.kind = SourceKind.SIMULATOR

        processing = ProcessingConfig()
        processing.fft_size = DEFAULT_FFT_SIZE
        processing.display_samples = DEFAULT_DISPLAY_SAMPLES

        self._session = AnalyzerSession(source, processing)
        self._marker_entries = [
            MarkerEntry("Marker 1", source.center_frequency_hz, DEFAULT_MARKER_BANDWIDTH_HZ)
        ]
        self._markers = self._marker_entries_to_markers(self._marker_entries)
        return source, processing

    def _create_controls(self, processing: ProcessingConfig) -> None:
        self._source_kind = QtWidgets.QComboBox()
        self._source_kind.addItem("Simulator", SourceKind.SIMULATOR)
        self._source_kind.addItem("Replay", SourceKind.REPLAY)
        self._source_kind.addItem("rtl_tcp", SourceKind.RTL_TCP)
        self._source_kind.addItem("UHD", SourceKind.UHD)
        self._source_kind.addItem("SoapySDR", SourceKind.SOAPY)
        self._source_kind.currentIndexChanged.connect(self._refresh_source_controls)

        self._center_edit = QtWidgets.QLineEdit(str(DEFAULT_SOURCE_CENTER_HZ))
        self._rate_edit = QtWidgets.QLineEdit(str(DEFAULT_SAMPLE_RATE_HZ))
        self._gain_edit = QtWidgets.QLineEdit(str(DEFAULT_GAIN_DB))
        self._fft_edit = QtWidgets.QLineEdit(str(processing.fft_size))
        self._input_path_edit = QtWidgets.QLineEdit(DEFAULT_EMPTY_TEXT)
        self._metadata_path_edit = QtWidgets.QLineEdit(DEFAULT_EMPTY_TEXT)
        self._loop_checkbox = QtWidgets.QCheckBox("Loop Replay")
        self._host_edit = QtWidgets.QLineEdit(DEFAULT_REPLAY_HOST)
        self._port_edit = QtWidgets.QLineEdit(DEFAULT_REPLAY_PORT)
        self._device_string_edit = QtWidgets.QLineEdit(DEFAULT_EMPTY_TEXT)
        self._device_args_edit = QtWidgets.QLineEdit(DEFAULT_EMPTY_TEXT)
        self._channel_edit = QtWidgets.QLineEdit(DEFAULT_CHANNEL)
        self._antenna_edit = QtWidgets.QLineEdit(DEFAULT_EMPTY_TEXT)
        self._bandwidth_edit = QtWidgets.QLineEdit(DEFAULT_EMPTY_TEXT)
        self._clock_source_edit = QtWidgets.QLineEdit(DEFAULT_EMPTY_TEXT)
        self._time_source_edit = QtWidgets.QLineEdit(DEFAULT_EMPTY_TEXT)
        self._start_button = QtWidgets.QPushButton("Start")
        self._start_button.clicked.connect(self._toggle_stream)
        self._marker_button = QtWidgets.QPushButton("Update Marker")
        self._marker_button.clicked.connect(self._update_marker)
        self._marker_summary_label = QtWidgets.QLabel("")
        self._marker_summary_label.setObjectName("markerSummary")

    def _build_controls_layout(self) -> None:
        controls = QtWidgets.QGridLayout()
        self._source_controls: list[
            tuple[QtWidgets.QLabel, QtWidgets.QWidget, set[SourceKind] | None]
        ] = []

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
        add_control(
            6, "Replay Metadata", self._metadata_path_edit, {SourceKind.REPLAY}
        )
        add_control(7, "rtl_tcp Host", self._host_edit, {SourceKind.RTL_TCP})
        add_control(8, "rtl_tcp Port", self._port_edit, {SourceKind.RTL_TCP})
        add_control(9, "Soapy Device", self._device_string_edit, {SourceKind.SOAPY})
        add_control(10, "UHD Device Args", self._device_args_edit, {SourceKind.UHD})
        add_control(
            11, "Channel", self._channel_edit, {SourceKind.UHD, SourceKind.SOAPY}
        )
        add_control(
            12, "Antenna", self._antenna_edit, {SourceKind.UHD, SourceKind.SOAPY}
        )
        add_control(
            13, "Bandwidth Hz", self._bandwidth_edit, {SourceKind.UHD, SourceKind.SOAPY}
        )
        add_control(14, "Clock Source", self._clock_source_edit, {SourceKind.UHD})
        add_control(15, "Time Source", self._time_source_edit, {SourceKind.UHD})
        loop_label = QtWidgets.QLabel("Replay Loop")
        controls.addWidget(loop_label, 4, 0)
        controls.addWidget(self._loop_checkbox, 4, 1)
        self._source_controls.append(
            (loop_label, self._loop_checkbox, {SourceKind.REPLAY})
        )

        controls.addWidget(self._start_button, 4, 2, 1, 2)
        controls.addWidget(self._marker_summary_label, 4, 4)
        controls.addWidget(self._marker_button, 4, 5)
        self._controls_layout = controls
        self._refresh_marker_summary()

    def _create_visualization_widgets(self) -> None:
        self._spectrum_plot = PlotCanvas(
            "Spectrum", DEFAULT_PLOT_Y_MIN, DEFAULT_PLOT_Y_MAX, show_x_labels=True
        )
        self._waterfall = WaterfallCanvas(rows=DEFAULT_WATERFALL_ROWS)
        self._time_plot = PlotCanvas(
            "Time Domain", DEFAULT_TIME_Y_MIN, DEFAULT_TIME_Y_MAX, show_x_labels=True
        )
        self._detection_table = DetectionTable()
        self._status_label = QtWidgets.QLabel("Idle")
        self._status_label.setStyleSheet(_STATUS_STYLE)

    def _assemble_main_layout(self) -> None:
        central = QtWidgets.QWidget()
        layout = QtWidgets.QVBoxLayout(central)
        layout.setSpacing(DEFAULT_CONTROL_SPACING)
        layout.addLayout(self._controls_layout)
        layout.addWidget(self._spectrum_plot, stretch=3)
        layout.addWidget(self._waterfall, stretch=3)
        layout.addWidget(self._time_plot, stretch=2)
        layout.addWidget(self._detection_table, stretch=1)
        layout.addWidget(self._status_label)
        self.setCentralWidget(central)

    def _create_timer(self) -> None:
        self._timer = QtCore.QTimer(self)
        self._timer.setInterval(DEFAULT_TIMER_INTERVAL_MS)
        self._timer.timeout.connect(self._poll_session)

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
            QLabel#markerSummary {
                color: #9aa4b5;
                padding: 4px 0;
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
            widget.setEnabled(visible)

    @staticmethod
    def _is_power_of_two(value: int) -> bool:
        return value > 0 and (value & (value - 1)) == 0

    @staticmethod
    def _read_text(edit: QtWidgets.QLineEdit, label: str) -> str:
        text = edit.text().strip()
        if not text:
            raise ValueError(f"{label} is required.")
        return text

    @staticmethod
    def _read_int(
        edit: QtWidgets.QLineEdit,
        label: str,
        *,
        minimum: int | None = None,
        maximum: int | None = None,
    ) -> int:
        text = edit.text().strip()
        if not text:
            raise ValueError(f"{label} is required.")
        try:
            value = int(text)
        except ValueError as exc:
            raise ValueError(f"{label} must be an integer.") from exc
        if minimum is not None and value < minimum:
            raise ValueError(f"{label} must be at least {minimum}.")
        if maximum is not None and value > maximum:
            raise ValueError(f"{label} must be at most {maximum}.")
        return value

    @staticmethod
    def _read_float(
        edit: QtWidgets.QLineEdit,
        label: str,
        *,
        minimum: float | None = None,
        maximum: float | None = None,
        allow_empty: bool = False,
        default: float = 0.0,
    ) -> float:
        text = edit.text().strip()
        if not text:
            if allow_empty:
                return default
            raise ValueError(f"{label} is required.")
        try:
            value = float(text)
        except ValueError as exc:
            raise ValueError(f"{label} must be a number.") from exc
        if not isfinite(value):
            raise ValueError(f"{label} must be finite.")
        if minimum is not None and value < minimum:
            raise ValueError(f"{label} must be at least {minimum}.")
        if maximum is not None and value > maximum:
            raise ValueError(f"{label} must be at most {maximum}.")
        return value

    def _configure_validators(self) -> None:
        def positive_double() -> QtGui.QDoubleValidator:
            validator = QtGui.QDoubleValidator(0.0, 1.0e12, 6, self)
            validator.setNotation(
                QtGui.QDoubleValidator.Notation.StandardNotation
            )
            return validator

        def signed_double() -> QtGui.QDoubleValidator:
            validator = QtGui.QDoubleValidator(-200.0, 200.0, 2, self)
            validator.setNotation(
                QtGui.QDoubleValidator.Notation.StandardNotation
            )
            return validator

        self._center_edit.setValidator(positive_double())
        self._rate_edit.setValidator(positive_double())
        self._gain_edit.setValidator(signed_double())
        self._fft_edit.setValidator(QtGui.QIntValidator(1, 2_147_483_647, self))
        self._port_edit.setValidator(QtGui.QIntValidator(1, 65535, self))
        self._channel_edit.setValidator(
            QtGui.QIntValidator(0, 2_147_483_647, self)
        )
        self._bandwidth_edit.setValidator(positive_double())

    def _set_status(self, message: str, *, error: bool = False) -> None:
        self._status_label.setStyleSheet(
            _STATUS_ERROR_STYLE if error else _STATUS_STYLE
        )
        self._status_label.setText(message)

    def _marker_entries_to_markers(
        self, entries: Sequence[MarkerEntry]
    ) -> list[Marker]:
        markers: list[Marker] = []
        for entry in entries:
            marker = Marker()
            marker.center_frequency_hz = entry.center_frequency_hz
            marker.bandwidth_hz = entry.bandwidth_hz
            markers.append(marker)
        return markers

    @staticmethod
    def _validate_marker_entry(entry: MarkerEntry, index: int) -> MarkerEntry:
        name = entry.name.strip()
        if not name:
            raise ValueError(f"Marker row {index + 1} Name is required.")
        if not isfinite(entry.center_frequency_hz) or entry.center_frequency_hz <= 0.0:
            raise ValueError(f"Marker row {index + 1} Center Hz must be greater than 0.")
        if not isfinite(entry.bandwidth_hz) or entry.bandwidth_hz <= 0.0:
            raise ValueError(
                f"Marker row {index + 1} Bandwidth Hz must be greater than 0."
            )
        return MarkerEntry(name, entry.center_frequency_hz, entry.bandwidth_hz)

    def _refresh_marker_summary(self) -> None:
        count = len(getattr(self, "_marker_entries", ()))
        if count == 0:
            self._marker_summary_label.setText("No markers")
        elif count == 1:
            marker = self._marker_entries[0]
            self._marker_summary_label.setText(
                f"1 marker: {marker.name}"
            )
        else:
            self._marker_summary_label.setText(f"{count} markers configured")

    def set_marker_entries(self, entries: Sequence[MarkerEntry]) -> None:
        """Replace the editable marker list and sync it to the session."""

        self._marker_entries = [
            self._validate_marker_entry(entry, index)
            for index, entry in enumerate(entries)
        ]
        self._markers = self._marker_entries_to_markers(self._marker_entries)
        self._session.set_markers(self._markers)
        self._refresh_marker_summary()

    def _open_marker_editor(self) -> None:
        dialog = MarkerEditorDialog(self._marker_entries, self)
        if dialog.exec() != QtWidgets.QDialog.DialogCode.Accepted:
            return
        self.set_marker_entries(dialog.markers())
        if self._markers:
            summary = ", ".join(entry.name for entry in self._marker_entries[:3])
            self._set_status(
                f"Markers updated: {summary}"
                + (" ..." if len(self._marker_entries) > 3 else "")
            )
        else:
            self._set_status("Markers cleared")

    def _update_marker(self) -> None:
        self._open_marker_editor()

    def _report_input_error(self, message: str) -> None:
        self._set_status(f"Input error: {message}", error=True)

    def _format_diagnostic_message(self, prefix: str) -> str:
        diagnostics = self._session.diagnostics()
        if not diagnostics:
            return f"{prefix}: {self._session.last_error()}"
        latest = diagnostics[-1]
        return (
            f"{prefix}: {self._session.last_error()} "
            f"[{latest.component}:{latest.code}]"
        )

    def _build_source_config(self) -> SourceConfig:
        source = self._session.source_config()
        source.kind = self._source_kind.currentData()
        source.center_frequency_hz = self._read_float(
            self._center_edit, "Center Hz", minimum=0.0, maximum=1.0e12
        )
        if source.center_frequency_hz <= 0.0:
            raise ValueError("Center Hz must be greater than 0.")
        source.sample_rate_hz = self._read_float(
            self._rate_edit, "Sample Rate", minimum=0.0, maximum=1.0e12
        )
        if source.sample_rate_hz <= 0.0:
            raise ValueError("Sample Rate must be greater than 0.")
        source.gain_db = self._read_float(
            self._gain_edit, "Gain dB", minimum=-200.0, maximum=200.0
        )

        if source.kind == SourceKind.REPLAY:
            source.input_path = self._read_text(self._input_path_edit, "Replay Input")
            source.metadata_path = self._metadata_path_edit.text().strip()
            source.loop_playback = self._loop_checkbox.isChecked()
        elif source.kind == SourceKind.RTL_TCP:
            source.network_host = self._read_text(self._host_edit, "rtl_tcp Host")
            source.network_port = self._read_int(
                self._port_edit, "rtl_tcp Port", minimum=1, maximum=65535
            )
        elif source.kind in {SourceKind.UHD, SourceKind.SOAPY}:
            source.channel = self._read_int(
                self._channel_edit, "Channel", minimum=0
            )
            source.antenna = self._antenna_edit.text().strip()
            source.bandwidth_hz = self._read_float(
                self._bandwidth_edit,
                "Bandwidth Hz",
                minimum=0.0,
                maximum=1.0e12,
                allow_empty=True,
                default=0.0,
            )
            if source.kind == SourceKind.UHD:
                source.device_args = self._device_args_edit.text().strip()
                source.clock_source = self._clock_source_edit.text().strip()
                source.time_source = self._time_source_edit.text().strip()
            else:
                source.device_string = self._device_string_edit.text().strip()
        return source

    def _build_processing_config(self) -> ProcessingConfig:
        processing = self._session.processing_config()
        processing.fft_size = self._read_int(
            self._fft_edit, "FFT Size", minimum=2
        )
        if not self._is_power_of_two(processing.fft_size):
            raise ValueError("FFT Size must be a power of two.")
        processing.display_samples = min(processing.fft_size, DEFAULT_DISPLAY_SAMPLES)
        return processing

    def _toggle_stream(self) -> None:
        if self._session.is_running():
            self._session.stop()
            self._session.drain_diagnostics()
            self._timer.stop()
            self._start_button.setText("Start")
            self._set_status("Stopped")
            return

        try:
            source = self._build_source_config()
            processing = self._build_processing_config()
        except ValueError as exc:
            self._report_input_error(str(exc))
            return

        if not self._session.update_source_config(source):
            self._set_status(
                self._format_diagnostic_message("Source update failed"),
                error=True,
            )
            return

        self._session.update_processing_config(processing)

        self._waterfall.set_fft_size(processing.fft_size)
        self._session.set_markers(self._markers)
        if not self._session.start():
            self._set_status(
                self._format_diagnostic_message("Start failed"),
                error=True,
            )
            return

        self._session.drain_diagnostics()
        self._timer.start()
        self._start_button.setText("Stop")
        self._set_status("Running")

    def _poll_session(self) -> None:
        snapshot = self._session.poll_snapshot()
        if snapshot is None:
            if not self._session.is_running():
                diagnostics = self._session.drain_diagnostics()
                latest = diagnostics[-1] if diagnostics else None
                self._timer.stop()
                self._start_button.setText("Start")
                if latest is not None:
                    self._set_status(
                        f"Session stopped: {latest.message} "
                        f"[{latest.component}:{latest.code}]",
                        error=latest.level == "error",
                    )
                else:
                    last_error = self._session.last_error().strip()
                    message = f"Session stopped: {last_error}" if last_error else "Stopped"
                    self._set_status(message, error=bool(last_error))
            return

        self._session.drain_diagnostics()
        spectrum = snapshot.spectrum
        detections = snapshot.analysis.detections
        markers = snapshot.analysis.marker_measurements
        marker_names = [
            entry.name for entry in getattr(self, "_marker_entries", ())
        ]

        x_values = list(
            (index - (len(spectrum.power_dbfs) / 2.0)) * spectrum.bin_resolution_hz
            + spectrum.center_frequency_hz
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
                for detection in detections[:DEFAULT_MAX_DETECTION_ANNOTATIONS]
            ]
            + [
                (
                    marker.center_frequency_hz,
                    (
                        f"{marker_names[index] if index < len(marker_names) else f'Marker {index + 1}'} "
                        f"{_format_dbfs(marker.peak_power_dbfs)} "
                        f"avg {_format_dbfs(marker.average_power_dbfs)}"
                    ),
                    "#f1b24a",
                )
                for index, marker in enumerate(markers)
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

        self._detection_table.set_detections(detections[:DEFAULT_MAX_DETECTION_ROWS])
        labels = ", ".join(
            f"{det.labels[0]} @ {_format_hz(det.center_frequency_hz)} / {_format_hz(det.bandwidth_hz)}"
            for det in detections[:DEFAULT_STATUS_DETECTIONS]
        )
        self._set_status(
            "Noise "
            f"{_format_dbfs(snapshot.analysis.noise_floor_dbfs)} | "
            f"Peak {_format_dbfs(snapshot.analysis.strongest_peak_dbfs)} | "
            f"{labels or 'no detections'}"
        )


def main() -> int:
    """Launch the Qt frontend and return the Qt event-loop exit code."""
    app = QtWidgets.QApplication(sys.argv)
    window = MainWindow()
    window.resize(DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT)
    window.show()
    return app.exec()

import codecs
import dataclasses
import enum
import logging
import sys
import typing as t

from PyQt6 import QtCore, QtWidgets, uic
from PyQt6.QtGui import QColor

from console.connections.com_connection import COMConnection
from console.connections.connection import Connection, Encoding
from console.logging import MainWindowLogHandler

# TODO Add command history

MAIN_WINDOW_UI_PATH = './assets/main_window.ui'
DEFAULT_BAUD_RATE = 9600
DEFAULT_ENCODING = Encoding.ANSI
SETTINGS_FILEPATH = './settings.ini'

class ReadbackType(enum.IntEnum):
    READ_SIZE = 0
    READ_UNTIL = 1

@dataclasses.dataclass
class ReadbackMethod:
    type: ReadbackType
    data: t.Any

    @classmethod
    def read_line(cls) -> t.Self:
        return cls(ReadbackType.READ_UNTIL, '\n')

    @classmethod
    def read_size(cls, size: int) -> t.Self:
        return cls(ReadbackType.READ_SIZE, size)

    @classmethod
    def read_until(cls, char: str) -> t.Self:
        return cls(ReadbackType.READ_UNTIL, char)


class ReadbackThread(QtCore.QThread):
    message_received = QtCore.pyqtSignal(str)
    readback_error = QtCore.pyqtSignal(str)

    def __init__(self, connection: Connection, readback_method: ReadbackMethod) -> None:
        super().__init__()

        self.connection = connection
        self.readback_method = readback_method
        self.is_running = False

    def _receive_text(self) -> str:
        match self.readback_method.type:
            case ReadbackType.READ_SIZE:
                return self.connection.receive_text(self.readback_method.data)
            case ReadbackType.READ_UNTIL:
                return self.connection.receive_text_until(self.readback_method.data)
            case _:
                raise ValueError('Invalid readback method type.')

    def run(self) -> None:
        self.is_running = True

        while self.is_running:
            try:
                self.message_received.emit(self._receive_text())
            except Exception as e:
                # FIXME Handle valid exception
                self.readback_error.emit(str(e))

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self) -> None:
        super().__init__()

        self.button_connect: QtWidgets.QPushButton
        self.combo_baud_rate: QtWidgets.QComboBox
        self.combo_port: QtWidgets.QComboBox
        self.combo_encoding: QtWidgets.QComboBox
        self.combo_readback_type: QtWidgets.QComboBox
        self.label_status: QtWidgets.QLabel
        self.button_send: QtWidgets.QPushButton
        self.line_edit_input: QtWidgets.QLineEdit
        self.text_output: QtWidgets.QTextEdit
        self.widget_readback_data: QtWidgets.QStackedWidget
        self.line_edit_readback_terminator: QtWidgets.QLineEdit
        self.spin_readback_size: QtWidgets.QSpinBox
        self.button_set_readback: QtWidgets.QPushButton

        # TODO Selectable optional int input (accepting list of integers instead of the message)
        self.use_int_input: bool = True

        self.readback_thread: ReadbackThread | None = None

        uic.load_ui.loadUi(MAIN_WINDOW_UI_PATH, self)

        self.settings = QtCore.QSettings(
            SETTINGS_FILEPATH,
            QtCore.QSettings.Format.IniFormat)
        self.connection: Connection | None = None

        # setup connection label
        palette = self.label_status.palette()
        palette.setColor(self.label_status.backgroundRole(), QColor('gray'))
        self.label_status.setPalette(palette)

        self.encoding = DEFAULT_ENCODING

        # setup readback
        self.button_set_readback.clicked.connect(self.set_readback_clicked_cb)

        for _value in ReadbackType:
            self.combo_readback_type.addItem(_value.name, _value)

        self.combo_readback_type.setCurrentIndex(
            self.combo_readback_type.findData(
                ReadbackType(self.settings.value(
                    'readback_type',
                    ReadbackType.READ_SIZE.value,
                    int))))

        # setup logger
        self.logger = logging.getLogger(__name__)
        self.logger.setLevel(logging.DEBUG)
        self.logger.addHandler(MainWindowLogHandler(self.text_output))

        # setup send button
        self.button_send.clicked.connect(self.send_action_cb)

        # setup connection status label
        self.set_connection_status('DISCONNECTED')

        # setup connect button
        self.button_connect.clicked.connect(self.connect_clicked_cb)

        # setup baud rate combo box
        for baud_rate in COMConnection.get_available_baud_rates():
            self.combo_baud_rate.addItem(str(baud_rate), baud_rate)

        self.combo_baud_rate.setCurrentIndex(
            self.combo_baud_rate.findData(
                self.settings.value(
                    'baud_rate',
                    DEFAULT_BAUD_RATE,
                    int)))
        self.combo_baud_rate.currentIndexChanged.connect(
            self.baud_rate_changed_cb)

        # setup port combo box
        com_ports = COMConnection.get_available_ports()
        if len(com_ports) != 0:
            for port in com_ports:
                self.combo_port.addItem(port, port)

            default_port: str | None = self.settings.value('port', '', str)
            if default_port:
                self.combo_port.setCurrentIndex(
                    self.combo_port.findData(default_port))
        else:
            self.combo_port.addItem('No COM devices connected.', None)
            self.button_connect.setEnabled(False)

        self.combo_port.currentIndexChanged.connect(self.port_changed_cb)

        # setup encoding combo box
        self.combo_encoding.currentIndexChanged.connect(self.encoding_changed_cb)
        for value in Encoding:
            self.combo_encoding.addItem(value.name, value.value)

        default_encoding: str | None = self.settings.value('encoding', DEFAULT_ENCODING.value, str)
        if default_encoding:
            self.combo_encoding.setCurrentIndex(self.combo_encoding.findData(default_encoding))

    def set_connection_status(self, status: str) -> None:
        self.label_status.setText(status)

    def set_connected_status(self, is_connected: bool) -> None:
        status_str = 'CONNECTED' if is_connected else 'DISCONNECTED'
        self.label_status.setText(status_str)

        connect_btn_str = 'Disconnect' if is_connected else 'Connect'
        self.button_connect.setText(connect_btn_str)

        self.button_send.setEnabled(is_connected)
        self.combo_baud_rate.setEnabled(not is_connected)
        self.combo_port.setEnabled(not is_connected)

        if is_connected:
            assert self.connection is not None, 'Connection should be estabilished by now'

            palette = self.label_status.palette()
            palette.setColor(self.label_status.backgroundRole(), QColor('green'))
            self.label_status.setPalette(palette)

            self.button_connect.clicked.disconnect(self.connect_clicked_cb)
            self.button_connect.clicked.connect(self.disconnect_clicked_cb)
            self.line_edit_input.returnPressed.connect(self.send_action_cb)

            readback_method = ReadbackMethod(
                self.combo_readback_type.currentData(),
                self.get_current_readback_data())

            self.readback_thread = ReadbackThread(
                self.connection,
                readback_method)
            self.readback_thread.message_received.connect(self.message_received_cb)
            self.readback_thread.start()
        else:
            palette = self.label_status.palette()
            palette.setColor(self.label_status.backgroundRole(), QColor('gray'))
            self.label_status.setPalette(palette)

            self.button_connect.clicked.disconnect(self.disconnect_clicked_cb)
            self.button_connect.clicked.connect(self.connect_clicked_cb)
            self.line_edit_input.returnPressed.disconnect(self.send_action_cb)

            if self.readback_thread is not None:
                self.readback_thread.is_running = False
                self.readback_thread.wait()
                self.readback_thread.message_received.disconnect(self.message_received_cb)

                self.readback_thread = None

    def get_current_readback_data(self) -> t.Any:
        current_index = self.widget_readback_data.currentIndex()
        match current_index:
            case 0:
                return self.spin_readback_size.value()
            case 1:
                return self.line_edit_readback_terminator.text()

        raise RuntimeError('Invalid readback data widget set.')

    def send_bytes(self, text: str, data: bytes) -> None:
        assert self.connection is not None, 'Connection is not estabilished'

        sent_bytes = self.connection.send(data)

        if sent_bytes is None:
            self.logger.error('Failed to sent COM message.')
        elif sent_bytes != len(data):
            self.logger.warning('Not all bytes sent (%d/%d bytes).', sent_bytes, len(data))
        else:
            self.logger.info(
                'Host -> %s (%d bytes)',
                text,
                len(data))

    def set_readback_method(self) -> None:
        readback_type: ReadbackType = self.combo_readback_type.currentData()
        readback_data = self.get_current_readback_data()

        if isinstance(readback_data, str):
            readback_data = codecs.decode(readback_data, 'unicode-escape')

        if self.readback_thread is not None:
            self.readback_thread.readback_method = ReadbackMethod(readback_type, readback_data)

        self.settings.setValue('readback_type', readback_type.value)
        self.settings.setValue('readback_data', readback_data)
        self.widget_readback_data.setCurrentIndex(readback_type.value)

    @QtCore.pyqtSlot()
    def set_readback_clicked_cb(self) -> None:
        self.set_readback_method()

    @QtCore.pyqtSlot(int)
    def encoding_changed_cb(self, index: int) -> None:
        encoding = self.combo_encoding.itemData(index)
        self.settings.setValue('encoding', encoding)

        self.encoding = Encoding(encoding)
        if self.connection is not None:
            self.connection.encoding = Encoding(encoding)

    @QtCore.pyqtSlot(str)
    def message_received_cb(self, msg: str) -> None:
        self.logger.info('Device -> "%s" (%d chars)', msg, len(msg))

    @QtCore.pyqtSlot()
    def send_action_cb(self) -> None:
        command_text = self.line_edit_input.text()
        self.line_edit_input.clear()

        try:
            data = bytearray(int(x.strip()) for x in command_text.split(','))
        except ValueError as e:
            self.logger.error('Failed to encode int message %s: %s.', command_text, e)
            return

        self.send_bytes(command_text, data)

    @QtCore.pyqtSlot()
    def _send_action_cb(self) -> None:
        command_text = self.line_edit_input.text()
        self.line_edit_input.clear()

        self.send_bytes(command_text, command_text.encode('ascii'))

    @QtCore.pyqtSlot(int)
    def baud_rate_changed_cb(self, _: int) -> None:
        baud_rate = int(self.combo_baud_rate.currentData())
        self.settings.setValue('baud_rate', baud_rate)

    @QtCore.pyqtSlot(int)
    def port_changed_cb(self, _: int) -> None:
        port: str | None = self.combo_port.currentData()
        if port is None:
            return

        self.settings.setValue('port', port)

    @QtCore.pyqtSlot(str)
    def readback_error_cb(self, message: str) -> None:
        self.logger.error('Readback error: %s', message)

    @QtCore.pyqtSlot()
    def connect_clicked_cb(self) -> None:
        port: str = self.combo_port.currentData()
        baud_rate: int = self.combo_baud_rate.currentData()

        try:
            self.connection = COMConnection(port, baud_rate, self.encoding)
        except Exception as e:
            self.logger.error('Failed to connect to COM device: %s', e)
            return

        self.set_connected_status(True)

        self.logger.info(
            'Connected to port %s, using baud rate %d.',
            port,
            baud_rate)

    @QtCore.pyqtSlot()
    def disconnect_clicked_cb(self) -> None:
        assert self.connection is not None, 'Connection is not estabilished'

        self.connection.close()
        self.connection = None

        self.set_connected_status(False)

        self.logger.info('Disconnected from COM port.')

if __name__ == '__main__':
    app = QtWidgets.QApplication(sys.argv)
    window = MainWindow()

    window.show()
    app.exec()

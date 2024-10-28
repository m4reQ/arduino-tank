import ctypes as ct
import logging
import sys

import serial  # type: ignore
from PyQt6 import QtCore, QtWidgets, uic

# TODO Add command history

MAX_COM_PORTS = 256
MAIN_WINDOW_UI_PATH = './assets/main_window.ui'
BAUD_RATES = (
    50,
    75,
    110,
    134,
    150,
    200,
    300,
    600,
    1200,
    1800,
    2400,
    4800,
    9600,
    19200,
    38400,
    57600,
    115200)
DEFAULT_BAUD_RATE = 9600
SETTINGS_FILEPATH = './settings.ini'

class MainWindowFormatter(logging.Formatter):
    COLOR_MAP = {
        logging.DEBUG: 'gray',
        logging.INFO: 'black',
        logging.WARNING: 'orange',
        logging.ERROR: 'red'}

    def __init__(self) -> None:
        super().__init__('[%(levelname)s] [%(asctime)s] %(message)s')

    def format(self, record: logging.LogRecord) -> str:
        main_text = super().format(record)
        return f'<span style="color:{self.COLOR_MAP[record.levelno]}">{main_text}</span>'

class MainWindowLogHandler(logging.Handler):
    def __init__(self, output: QtWidgets.QTextEdit) -> None:
        super().__init__(0)

        self.output = output
        self.setFormatter(MainWindowFormatter())

    def emit(self, record: logging.LogRecord) -> None:
        self.output.insertHtml(self.format(record))
        self.output.insertPlainText('\n')

class ReadbackThread(QtCore.QThread):
    message_received = QtCore.pyqtSignal(str)

    def __init__(self, _serial: serial.Serial) -> None:
        super().__init__()
        
        self._serial = _serial
        self.is_running = False
    
    def run(self) -> None:
        self.is_running = True

        while self.is_running:
            # TODO Multiple readback methods
            try:
                data = self._serial.readline()
            except Exception: # FIXME Handle valid exception
                # ignore exception when device disconnected
                break
            
            # TODO Selectable encoding
            decoded = data.decode('ansi')

            self.message_received.emit(decoded)

class MainWindow(QtWidgets.QMainWindow):
    def __init__(self) -> None:
        super().__init__()

        self.button_connect: QtWidgets.QPushButton
        self.combo_baud_rate: QtWidgets.QComboBox
        self.combo_port: QtWidgets.QComboBox
        self.frame_status_indicator: QtWidgets.QFrame
        self.label_status: QtWidgets.QLabel
        self.button_send: QtWidgets.QPushButton
        self.line_edit_input: QtWidgets.QLineEdit
        self.text_output: QtWidgets.QTextEdit

        # TODO Selectable optional int input (accepting list of integers instead of the message)
        self.use_int_input: bool = True

        self.readback_thread: ReadbackThread | None = None

        uic.load_ui.loadUi(MAIN_WINDOW_UI_PATH, self)

        self.settings = QtCore.QSettings(
            SETTINGS_FILEPATH,
            QtCore.QSettings.Format.IniFormat)
        self.serial: serial.Serial | None = None

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
        for baud_rate in BAUD_RATES:
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
        com_ports = get_connected_com_ports()
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
            self.button_connect.clicked.disconnect(self.connect_clicked_cb)
            self.button_connect.clicked.connect(self.disconnect_clicked_cb)
            self.line_edit_input.returnPressed.connect(self.send_action_cb)

            self.readback_thread = ReadbackThread(self.serial)
            self.readback_thread.message_received.connect(self.message_received_cb)
            self.readback_thread.start()
        else:
            self.button_connect.clicked.disconnect(self.disconnect_clicked_cb)
            self.button_connect.clicked.connect(self.connect_clicked_cb)
            self.line_edit_input.returnPressed.disconnect(self.send_action_cb)

            if self.readback_thread is not None:
                self.readback_thread.is_running = False
                self.readback_thread.wait()
                self.readback_thread.message_received.disconnect(self.message_received_cb)

                self.readback_thread = None
    
    def send_bytes(self, text: str, data: bytes) -> None:
        sent_bytes = self.serial.write(data)
        if sent_bytes is None:
            self.logger.error('Failed to sent COM message.')
        elif sent_bytes != len(data):
            self.logger.warning('Not all bytes sent (%d/%d bytes).', sent_bytes, len(data))
        else:
            self.logger.info(
                'Host -> %s (%d bytes)',
                text,
                len(data))
    
    @QtCore.pyqtSlot(str)
    def message_received_cb(self, msg: str) -> None:
        self.logger.info('Device -> "%s" (%d chars)', msg, len(msg))

    @QtCore.pyqtSlot()
    def send_action_cb(self) -> None:
        command_text = self.line_edit_input.text()

        try:
            data = bytearray(int(x.strip()) for x in command_text.split(','))
        except ValueError as e:
            self.logger.error('Failed to encode int message %s: %s.', command_text, e)
            return

        self.line_edit_input.clear()

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

    @QtCore.pyqtSlot()
    def connect_clicked_cb(self) -> None:
        port: str | None = self.combo_port.currentData()
        baud_rate: int = self.combo_baud_rate.currentData()

        assert port is not None, 'Port cannot be None'

        try:
            self.serial = serial.Serial(port, baud_rate)
        except serial.SerialException as e:
            self.logger.error('Failed to connect to COM device: %s', e)
            return

        self.set_connected_status(True)

        self.logger.info(
            'Connected to port %s, using baud rate %d.',
            port,
            baud_rate)

    @QtCore.pyqtSlot()
    def disconnect_clicked_cb(self) -> None:
        assert self.serial is not None, 'Serial device cannot be None'

        self.serial.close()
        self.serial = None

        self.set_connected_status(False)

        self.logger.info('Disconnected from COM port.')

def get_connected_com_ports() -> list[str]:
    com_ports = (ct.c_ulong * MAX_COM_PORTS)()
    com_ports_found = ct.c_ulong(0)

    try:
        GetCommPorts(com_ports, MAX_COM_PORTS, ct.pointer(com_ports_found))
    except RuntimeError:
        return []

    result = list[str]()
    for i in range(com_ports_found.value):
        result.append(f'COM{com_ports[i]}')

    return result

def get_comm_ports_errcheck(result: ct.c_ulong, *_) -> ct.c_ulong:
    if result != 0:
        raise RuntimeError('Failed to retrieve connected COM ports.')

    return result

GetCommPorts = ct.WinDLL('KernelBase.dll').GetCommPorts # type: ignore
GetCommPorts.restype = ct.c_ulong
GetCommPorts.argtypes = (
    ct.POINTER(ct.c_ulong),
    ct.c_ulong,
    ct.POINTER(ct.c_ulong))
GetCommPorts.errcheck = get_comm_ports_errcheck # type: ignore

if __name__ == '__main__':
    app = QtWidgets.QApplication(sys.argv)
    window = MainWindow()

    window.show()
    app.exec()

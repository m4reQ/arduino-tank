import dataclasses
import socket
import threading
import time
import typing as t

from tank_control.average import Average
from tank_control.communication.command import Command, Opcode
from tank_control.communication.controller import ControllerBase
from tank_control.communication.result import RESULT_HEADER_SIZE, Result
from tank_control.timer import Timer


@dataclasses.dataclass
class _SentCommandEntry:
    command: Command
    send_timestamp: float

class BluetoothController(ControllerBase):
    def __init__(self,
                 bt_address: str,
                 bt_channel: int,
                 message_received_cb: t.Callable[[Result, Command], t.Any]) -> None:
        super().__init__(message_received_cb)

        self.bt_address = bt_address
        self.bt_channel = bt_channel

        self._commands_history = dict[int, _SentCommandEntry]()

        self._latency = Average(10)
        self._command_compile_time = Average(10)

        self._socket: socket.socket | None = None
        self._receiver_thread: threading.Thread | None = None

        self._is_running = False

    def send_command(self, opcode: Opcode, *args: int) -> None:
        if self._socket is None:
            raise RuntimeError('Controller not started.')

        command = Command(opcode, *args)
        code = self._compile_command(command)

        self._commands_history[command.id] = _SentCommandEntry(command, time.perf_counter())
        self._socket.sendall(code)

    def shutdown(self) -> None:
        self._is_running = False

        if self._socket is not None:
            self._socket.close()
            self._socket.shutdown(socket.SHUT_RDWR)
            self._socket = None

        if self._receiver_thread is not None:
            self._receiver_thread.join()
            self._receiver_thread = None

    def start(self):
        if self._receiver_thread is not None or self._socket is not None:
            raise RuntimeError('Controller is already running.')

        self._socket = socket.socket(
            socket.AF_BLUETOOTH,
            socket.SOCK_STREAM,
            socket.BTPROTO_RFCOMM)
        self._socket.connect((self.bt_address, self.bt_channel))

        self._is_running = True
        self._should_accept_commands = True

        self._latency.reset()
        self._command_compile_time.reset()

        self._receiver_thread = threading.Thread(target=self._receiver_thread_main)
        self._receiver_thread.start()

    @property
    def latency(self) -> float:
        return self._latency.value

    @property
    def command_compile_time(self) -> float:
        return self._command_compile_time.value

    def _receiver_thread_main(self) -> None:
        while self._is_running:
            try:
                header_data = self._socket.recv(RESULT_HEADER_SIZE)
            except socket.error:
                continue

            if len(header_data) < RESULT_HEADER_SIZE:
                continue

            result = Result.from_header(header_data)
            if result.data_length > 0:
                result.data = self._socket.recv(result.data_length)

            command_entry = self._commands_history.pop(result.id)
            self._latency.update(time.perf_counter() - command_entry.send_timestamp)

            self.result_callback(result, command_entry.command)

    def _compile_command(self, command: Command) -> bytes:
        with Timer() as timer:
            code = command.compile()

        self._command_compile_time.update(timer.elapsed_s)

        return code

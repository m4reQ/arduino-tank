import typing as t

from tank_control.average import Average
from tank_control.communication.command import Command, Opcode
from tank_control.communication.controller import ControllerBase
from tank_control.communication.result import Result, Status
from tank_control.timer import Timer


class TestController(ControllerBase):
    def __init__(self, message_received_cb: t.Callable[[Result, Command], t.Any]) -> None:
        super().__init__(message_received_cb)

        self._command_compile_time = Average(10)

        self._is_running = False

    def send_command(self, opcode: Opcode, *args: int) -> None:
        if not self._is_running:
            raise RuntimeError('Controller is not started')

        command = Command(opcode, *args)
        code = self._compile_command(command)
        result = Result(
            command.opcode,
            command.id,
            Status.SUCCESS,
            len(code),
            code)

        self.message_reseived_cb(result, command)

    def shutdown(self) -> None:
        self._is_running = False

    def start(self):
        self._is_running = True

    @property
    def latency(self) -> float:
        return 0.0

    @property
    def command_compile_time(self) -> float:
        return self._command_compile_time.value

    def _compile_command(self, command: Command) -> bytes:
        with Timer() as timer:
            code = command.compile()

        self._command_compile_time.update(timer.elapsed_s)

        return code

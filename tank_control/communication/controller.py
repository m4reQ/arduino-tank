import abc
import typing as t

from tank_control.communication.command import Command
from tank_control.communication.result import Result


class ControllerBase(abc.ABC):
    def __init__(self, message_received_cb: t.Callable[[Result, Command], t.Any]) -> None:
        self.message_reseived_cb = message_received_cb

    @abc.abstractmethod
    def start(self) -> None: ...

    @abc.abstractmethod
    def shutdown(self) -> None: ...

    @abc.abstractmethod
    def send_command(self, command: Command) -> None: ...

    @property
    @abc.abstractmethod
    def latency(self) -> float: ...

    @property
    @abc.abstractmethod
    def command_compile_time(self) -> float: ...

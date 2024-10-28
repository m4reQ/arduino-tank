import abc
import enum


class Encoding(enum.StrEnum):
    ANSI = 'ansi'
    UTF8 = 'utf-8'
    UTF16 = 'utf-16'


class Connection(abc.ABC):
    def __init__(self, encoding: Encoding) -> None:
        self.encoding = encoding

    def decode(self, data: bytes) -> str:
        return data.decode(self.encoding)

    def encode(self, text: str) -> bytes:
        return text.encode(self.encoding)

    def send_text(self, text: str) -> int:
        return self.send(self.encode(text))

    def receive_text(self, length: int) -> str:
        return self.decode(self.receive(length))

    def receive_text_until(self, char: str) -> str:
        assert len(char) == 1, 'Termination string has to be a single character'

        return self.decode(self.receive_until(ord(char)))

    @abc.abstractmethod
    def close(self) -> None: ...

    @abc.abstractmethod
    def send(self, data: bytes) -> int: ...

    @abc.abstractmethod
    def receive(self, length: int) -> bytes: ...

    @abc.abstractmethod
    def receive_until(self, byte: int) -> bytes: ...

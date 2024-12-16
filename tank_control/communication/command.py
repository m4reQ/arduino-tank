import enum
import random
import struct
import typing as t

MAX_ARGS_COUNT = 16
UINT64_MAX = (1 << 64) - 1

class Opcode(enum.IntEnum):
    MOVE = 1
    STOP = 2
    PRINT_LCD = 3
    GET_SENSOR_STATE = 4
    MOVE_HEAD_SENSOR = 5

class Command:
    def __init__(self, opcode: Opcode, *args: t.SupportsInt) -> None:
        assert len(args) <= MAX_ARGS_COUNT, 'Max command arguments count exceeded'

        self.opcode = opcode
        self.args = args
        self.id = random.randint(0, UINT64_MAX)
        self._format_str = f'>BQB{len(self.args)}s'

    def compile(self) -> bytes:
        return struct.pack(
            self._format_str,
            self.opcode.value,
            self.id,
            len(self.args),
            bytes(int(x) for x in self.args))

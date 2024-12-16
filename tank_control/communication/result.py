import dataclasses
import enum
import struct
import typing as t

from tank_control.communication.command import Opcode

RESULT_HEADER_FORMAT = '>BQBB'
RESULT_HEADER_SIZE = struct.calcsize(RESULT_HEADER_FORMAT)

class Status(enum.IntEnum):
    SUCCESS = 0
    INVALID_OPCODE = 1
    INVALID_ARGS_COUNT = 2
    INVALID_STATE = 3

@dataclasses.dataclass
class Result:
    opcode: Opcode
    id: int
    status: Status
    data_length: int
    data: bytes = b''

    @classmethod
    def from_header(cls, header: bytes) -> t.Self:
        opcode, id, status, data_length = struct.unpack(RESULT_HEADER_FORMAT, header)
        return cls(
            opcode,
            id,
            status,
            data_length)

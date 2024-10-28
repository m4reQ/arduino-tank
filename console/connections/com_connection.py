import ctypes as ct

import serial  # type: ignore[import-untyped]

from console.connections.connection import Connection, Encoding

MAX_COM_PORTS = 256

class COMConnection(Connection):
    @staticmethod
    def get_available_baud_rates() -> tuple[int, ...]:
        return serial.Serial.BAUDRATES

    @staticmethod
    def get_available_ports() -> tuple[str, ...]:
        com_ports = (ct.c_ulong * MAX_COM_PORTS)()
        com_ports_found = ct.c_ulong(0)

        try:
            GetCommPorts(com_ports, MAX_COM_PORTS, ct.pointer(com_ports_found))
        except RuntimeError:
            return tuple[str]()

        return tuple(f'COM{com_ports[i]}' for i in range(com_ports_found.value))

    def __init__(self, port: str, baud_rate: int, encoding: Encoding) -> None:
        super().__init__(encoding)

        self.serial = serial.Serial(port, baud_rate)

    def send(self, data: bytes) -> int:
        return self.serial.write(data)

    def receive(self, length: int) -> bytes:
        return self.serial.read(length)

    def receive_until(self, byte: int) -> bytes:
        return self.serial.read_until(byte)

    def close(self) -> None:
        self.serial.cancel_read()
        self.serial.close()

def get_comm_ports_errcheck(result: ct.c_ulong, *_) -> ct.c_ulong:
    if result != 0:
        raise RuntimeError('Failed to retrieve connected COM ports.')

    return result

GetCommPorts = ct.WinDLL('KernelBase.dll').GetCommPorts
GetCommPorts.restype = ct.c_ulong
GetCommPorts.argtypes = (
    ct.POINTER(ct.c_ulong),
    ct.c_ulong,
    ct.POINTER(ct.c_ulong))
GetCommPorts.errcheck = get_comm_ports_errcheck # type: ignore[assignment]

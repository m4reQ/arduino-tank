import enum


class Direction(enum.IntEnum):
    BACKWARD = 0b00
    FORWARD = 0b11
    LEFT = 0b01
    RIGHT = 0b10
    CURRENT = 0b11111111

class Sensor(enum.IntEnum):
    LEFT = 0
    RIGHT = 1
    REAR = 2
    HEAD = 3

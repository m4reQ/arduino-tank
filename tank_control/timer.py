import time
import typing as t


class Timer:
    def __init__(self) -> None:
        self._start = 0
        self._end = 0

    def __enter__(self) -> t.Self:
        self._start = time.perf_counter_ns()
        return self

    def __exit__(self, *_) -> None:
        self._end = time.perf_counter_ns()

    @property
    def elapsed_ns(self) -> int:
        return self._end - self._start

    @property
    def elapsed_s(self) -> float:
        return self.elapsed_ns / (10 ** 9)

    @property
    def elapsed_us(self) -> float:
        return self.elapsed_ns / (10 ** 3)

    @property
    def elapsed_ms(self) -> float:
        return self.elapsed_ns / (10 ** 6)

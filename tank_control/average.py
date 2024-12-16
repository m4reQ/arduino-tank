from collections import deque


class Average:
    def __init__(self, queue_size: int) -> None:
        self.data = deque[float](maxlen=queue_size)

    def update(self, value: float) -> None:
        self.data.append(value)

    def reset(self) -> None:
        self.data.clear()

    @property
    def value(self) -> float:
        return sum(self.data) / max(1, len(self.data))

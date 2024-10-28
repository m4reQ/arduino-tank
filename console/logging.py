import logging

from PyQt6.QtWidgets import QTextEdit


class MainWindowFormatter(logging.Formatter):
    COLOR_MAP = {
        logging.DEBUG: 'gray',
        logging.INFO: 'black',
        logging.WARNING: 'orange',
        logging.ERROR: 'red'}

    def __init__(self) -> None:
        super().__init__('[%(levelname)s] [%(asctime)s] %(message)s')

    def format(self, record: logging.LogRecord) -> str:
        main_text = super().format(record)
        return f'<span style="color:{self.COLOR_MAP[record.levelno]}">{main_text}</span>'

class MainWindowLogHandler(logging.Handler):
    def __init__(self, output: QTextEdit) -> None:
        super().__init__(0)

        self.output = output
        self.setFormatter(MainWindowFormatter())

    def emit(self, record: logging.LogRecord) -> None:
        self.output.insertHtml(self.format(record))
        self.output.insertPlainText('\n')

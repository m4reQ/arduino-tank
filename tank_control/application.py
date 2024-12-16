import dataclasses
import typing as t

import pygame as pg

from tank_control.communication.bluetooth_controller import BluetoothController
from tank_control.communication.command import Command, Opcode
from tank_control.communication.result import Result
from tank_control.communication.test_controller import TestController
from tank_control.enums import Direction, Sensor

_HEAD_SENSOR_MOVE_SPEED = 60.0
_HEAD_SENSOR_DIR_MIN = -90.0
_HEAD_SENSOR_DIR_MAX = 90.0
_DIRECTION_KEY_MAP = {
    pg.K_w: Direction.FORWARD,
    pg.K_s: Direction.BACKWARD,
    pg.K_a: Direction.LEFT,
    pg.K_d: Direction.RIGHT}
_MODEL_SCALE = 0.6
_ARROW_SIZE = 48
_ARROW_SPACING = 5

@dataclasses.dataclass
class DirectionArrow:
    img: pg.Surface
    overlay_img: pg.Surface
    pos: tuple[int, int]
    is_active: bool = False

@dataclasses.dataclass
class SensorIndicator:
    img: pg.Surface
    pos: tuple[int, int]
    is_active: bool = False

class Application:
    def __init__(self,
                 window_size: tuple[int, int],
                 bt_address: str,
                 bt_channel: int) -> None:
        pg.init()

        self.window = pg.display.set_mode(window_size, vsync=False)
        self.font = pg.font.SysFont('Cascadia Mono', 24)
        self.font_small = pg.font.SysFont('Cascadia Mono', 14)
        self.clock = pg.time.Clock()
        # self.controller = BluetoothController(
        #     bt_address,
        #     bt_channel,
        #     self._on_command_result)
        self.controller = TestController(self._on_command_result)

        self.frametime = 0

        self.head_sensor_angle = 0.0
        self.head_sensor_dir = 0
        self.head_sensor_dist = 0
        self.sensor_query_timeout = 0.0

        self.head_sensor_img = pg.transform.smoothscale_by(
            pg.image.load('./assets/images/head_sensor.png').convert_alpha(),
            (_MODEL_SCALE, _MODEL_SCALE))
        self.head_sensor_img_rotated = self.head_sensor_img

        self.track_img = pg.transform.smoothscale_by(
            pg.image.load('./assets/images/track.png').convert_alpha(),
            (_MODEL_SCALE, _MODEL_SCALE))

        self.hull_img = pg.transform.smoothscale_by(
            pg.image.load('./assets/images/hull.png').convert_alpha(),
            (_MODEL_SCALE, _MODEL_SCALE))

        arrow_img = pg.transform.smoothscale(
            pg.image.load('./assets/images/arrow.png').convert_alpha(),
            (_ARROW_SIZE, _ARROW_SIZE))
        arrow_overlay_img = pg.transform.smoothscale(
            pg.image.load('./assets/images/arrow_overlay.png').convert_alpha(),
            (_ARROW_SIZE, _ARROW_SIZE))
        sensor_indicator_img = pg.transform.smoothscale_by(
            pg.image.load('./assets/images/sensor_indicator.png').convert_alpha(),
            (_MODEL_SCALE, _MODEL_SCALE))
        self.sensor_state = {
            Sensor.LEFT: SensorIndicator(
                sensor_indicator_img,
                (256 + 13 - self.hull_img.get_width() // 2, 256 - 7 - self.hull_img.get_height() // 2)),
            Sensor.RIGHT: SensorIndicator(
                sensor_indicator_img,
                (256 - 12 + self.hull_img.get_width() // 2 - sensor_indicator_img.get_width(), 256 - 7 - self.hull_img.get_height() // 2)),
            Sensor.REAR: SensorIndicator(
                pg.transform.rotate(sensor_indicator_img, 180),
                (256 - sensor_indicator_img.get_width() // 2, 256 - 7 + self.hull_img.get_height() // 2))}
        self.direction_state = {
            Direction.LEFT: DirectionArrow(
                pg.transform.rotate(arrow_img, 90.0),
                pg.transform.rotate(arrow_overlay_img, 90.0),
                (_ARROW_SPACING, 512 - _ARROW_SPACING - _ARROW_SIZE)),
            Direction.BACKWARD: DirectionArrow(
                pg.transform.rotate(arrow_img, 180.0),
                pg.transform.rotate(arrow_overlay_img, 180.0),
                (_ARROW_SPACING * 2 + _ARROW_SIZE, 512 - _ARROW_SPACING - _ARROW_SIZE)),
            Direction.RIGHT: DirectionArrow(
                pg.transform.rotate(arrow_img, 270.0),
                pg.transform.rotate(arrow_overlay_img, 270.0),
                (_ARROW_SPACING + (_ARROW_SIZE + _ARROW_SPACING) * 2, 512 - _ARROW_SPACING - _ARROW_SIZE)),
            Direction.FORWARD: DirectionArrow(
                arrow_img,
                arrow_overlay_img,
                (_ARROW_SPACING * 2 + _ARROW_SIZE, 512 - (_ARROW_SPACING + _ARROW_SIZE) * 2))}

    def run(self) -> None:
        self.controller.start()

        should_run = True
        while should_run:
            self.frametime = self.clock.get_time()
            frametime_s = self.frametime / 1000.0

            pg.display.set_caption(f'ZiutekTech controller')

            for event in pg.event.get():
                if event.type == pg.KEYDOWN:
                    self._handle_key_down(event)
                elif event.type == pg.KEYUP:
                    self._handle_key_up()
                elif event.type == pg.QUIT:
                    should_run = False
                elif event.type == pg.USEREVENT:
                    self._handle_command_result(event.dict['result'], event.dict['origin'])

            head_sensor_rotation_changed = False
            pressed = pg.key.get_pressed()
            if pressed[pg.K_q]:
                self.head_sensor_angle = clamp(
                    self.head_sensor_angle + frametime_s * _HEAD_SENSOR_MOVE_SPEED,
                    _HEAD_SENSOR_DIR_MIN,
                    _HEAD_SENSOR_DIR_MAX)
                head_sensor_rotation_changed = True
            elif pressed[pg.K_e]:
                self.head_sensor_angle = clamp(
                    self.head_sensor_angle - frametime_s * _HEAD_SENSOR_MOVE_SPEED,
                    _HEAD_SENSOR_DIR_MIN,
                    _HEAD_SENSOR_DIR_MAX)
                head_sensor_rotation_changed = True

            if head_sensor_rotation_changed:
                self.controller.send_command(
                    Opcode.MOVE_HEAD_SENSOR,
                    int(self.head_sensor_angle + 90.0))

            if self.sensor_query_timeout >= 0.1:
                self.controller.send_command(Opcode.GET_SENSOR_STATE)
                self.sensor_query_timeout -= 0.1

            self.sensor_query_timeout += frametime_s

            self._draw()

            pg.display.flip()
            self.clock.tick()

        self.controller.shutdown()

    def _draw(self) -> None:
        self.window.fill((0, 0, 0))

        hull_pos = (
            self.window.get_width() // 2 - self.hull_img.get_width() // 2,
            self.window.get_height() // 2 - self.hull_img.get_height() // 2)

        self.window.blit(
            self.track_img,
            (hull_pos[0] - self.track_img.get_width() // 2, hull_pos[1]))

        self.window.blit(
            self.track_img,
            (hull_pos[0] - self.track_img.get_width() // 2 + self.hull_img.get_width(), hull_pos[1]))

        self.window.blit(
            self.hull_img,
            hull_pos)

        for sensor_indicator in self.sensor_state.values():
            if sensor_indicator.is_active:
                self.window.blit(sensor_indicator.img, sensor_indicator.pos)

        head_sensor_pos = (
            hull_pos[0] + self.hull_img.get_width() // 2 - self.head_sensor_img_rotated.get_width() // 2,
            hull_pos[1] + self.head_sensor_img.get_height() - self.head_sensor_img_rotated.get_height() // 2)

        self.window.blit(
            self.head_sensor_img_rotated,
            head_sensor_pos)

        latency_text = self.font.render(
            f'Latency: {(self.controller.latency * 1000.0):.3f} ms',
            True,
            (0, 255, 0))
        compile_time_text = self.font.render(
            f'Command compile time: {(self.controller.command_compile_time * 1000.0):.3f} ms',
            True,
            (0, 255, 0))

        text_x = self.window.get_width() - max(latency_text.get_width(), compile_time_text.get_width()) - 10

        self.window.blit(
            latency_text,
            (text_x, self.window.get_height() - latency_text.get_height() - 10))
        self.window.blit(
            compile_time_text,
            (text_x, self.window.get_height() - latency_text.get_height() - compile_time_text.get_height() - 10))

        heading_text = self.font_small.render(
            f'heading: {self.head_sensor_dir} deg',
            True,
            (0, 255, 0))
        distance_text = self.font_small.render(
            f'dist: {self.head_sensor_dist} mm',
            True,
            (0, 255, 0))

        heading_pos = (
            hull_pos[0] + self.hull_img.get_width() // 2,
            hull_pos[1] - 40)
        distance_pos = (
            hull_pos[0] + self.hull_img.get_width() // 2,
            hull_pos[1] + heading_text.get_height() - 40)
        self.window.blit(
            heading_text,
            heading_pos)
        self.window.blit(
            distance_text,
            distance_pos)

        frametime_text = self.font.render(
            f'Frametime: {self.frametime} ms',
            True,
            (0, 255, 0))
        self.window.blit(
            frametime_text,
            (10, 10))

        for dir_arrow in self.direction_state.values():
            self.window.blit(dir_arrow.img, dir_arrow.pos)

            if dir_arrow.is_active:
                self.window.blit(dir_arrow.overlay_img, dir_arrow.pos)

    def _on_command_result(self, result: Result, origin: Command) -> None:
        pg.event.post(pg.event.Event(pg.USEREVENT, {'result': result, 'origin': origin}))

    def _handle_command_result(self, result: Result, origin: Command) -> None:
        if result.opcode == Opcode.MOVE:
            self.direction_state[origin.args[0]].is_active = True
        elif result.opcode == Opcode.STOP:
            for direction in self.direction_state:
                self.direction_state[direction].is_active = False
        elif result.opcode == Opcode.MOVE_HEAD_SENSOR:
            self.head_sensor_dir = int(origin.args[0]) - 90
            self.head_sensor_img_rotated = pg.transform.rotozoom(self.head_sensor_img, self.head_sensor_dir, 1.0)
        elif result.opcode == Opcode.GET_SENSOR_STATE:
            self.sensor_state[Sensor.LEFT].is_active = not bool(result.data[0])
            self.sensor_state[Sensor.RIGHT].is_active = not bool(result.data[1])
            self.sensor_state[Sensor.REAR].is_active = not bool(result.data[2])
            self.head_sensor_dist = result.data[3]

        # print(f'[RESULT] Opcode: {result.opcode}, Status: {result.status}, Data ({len(result.data)} bytes): {result.data!r}')

    def _handle_key_down(self, event: pg.event.Event) -> None:
        if event.key in _DIRECTION_KEY_MAP:
            self.controller.send_command(
                Opcode.MOVE,
                _DIRECTION_KEY_MAP[event.key],
                255)
        elif event.key == pg.K_1:
            self.controller.send_command(Opcode.GET_SENSOR_STATE)

    def _handle_key_up(self) -> None:
        self.controller.send_command(Opcode.STOP)

TValue = t.TypeVar('TValue', int, float)
def clamp(x: TValue, _min: TValue, _max: TValue):
    return min(max(_min, x), _max)

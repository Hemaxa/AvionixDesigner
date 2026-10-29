"""
Загрузка объектов в ПЛИС по UART и интерактивное управление "полётом" над картой.

Единый файл, собранный из модулей: main, sending, vbDataLoader, vbXML,
vbDataWorks, CompilePackets, map_viewer.

Запуск:
    python fpga_project.py --port COM4 --xml Test.xml --map BigMap.png
    python fpga_project.py --skip-setup        # без загрузки XML, сразу управление

Зависимости: pyserial, keyboard, opencv-python, numpy. Python >= 3.10 (match).
Библиотеке keyboard под Linux нужны права root.

Заметки, оставшиеся от старого main.py:
    * left/right для layer3d в XML:
        ...0386F5 -> right,  ...034000 -> left
"""
from __future__ import annotations

import argparse
import logging
import math
import time
import xml.etree.ElementTree as ET
from collections import deque

import cv2
import keyboard
import numpy as np
import serial

logger = logging.getLogger("fpga")

# =============================================================================
#  НАСТРОЙКИ
# =============================================================================
DEFAULT_PORT = "COM4"
LOAD_BAUDRATE = 921600      # скорость при загрузке объектов
CONTROL_BAUDRATE = 115200   # скорость в режиме интерактивного управления
DEFAULT_XML = "Test.xml"
DEFAULT_MAP = "BigMap.png"

STATIC_CHUNK_SIZE = 1024    # размер куска данных staticgroup за один пакет

# =============================================================================
#  ПРОТОКОЛ: упаковка пакетов
#  Пакет = MAGIC | длина(2, BE) | тип(1) | полезная нагрузка | контрольная сумма(4, BE)
#  Длина = len(тип + нагрузка) + 4 (размер контрольной суммы).
# =============================================================================
MAGIC = b"SAIN"             # 0x53 0x41 0x49 0x4E
FCS_SIZE = 4

MSG_MODIFY_ROT = 0x02       # данные памяти rotationobject
MSG_PARAMS = 0x03           # параметры объекта
MSG_MODIFY_STATIC = 0x04    # данные памяти staticgroup
MSG_LAYER3D = 0x06          # положение для layer3d


def fletcher32(data: bytes) -> int:
    """Контрольная сумма пакета.

    Внимание: это байтовый вариант (a, b по модулю 2**16), а не классический
    Fletcher-32 над 16-битными словами. Должен совпадать с реализацией в ПЛИС.
    """
    a = 0
    b = 0
    for byte in data:
        a = (a + byte) & 0xFFFF
        b = (a + b) & 0xFFFF
    return (b << 16) | a


def _be(value: int, size: int, name: str) -> bytes:
    """Целое -> big-endian байты с понятной ошибкой при переполнении."""
    if not 0 <= value < 1 << (8 * size):
        raise ValueError(f"{name}={value} не помещается в {size} байт(а)")
    return value.to_bytes(size, "big")


def build_packet(msg_type: int, payload: bytes) -> bytes:
    """Собирает пакет: MAGIC | длина | тип | нагрузка | контрольная сумма."""
    body = bytes([msg_type]) + bytes(payload)
    head = MAGIC + _be(len(body) + FCS_SIZE, 2, "длина пакета") + body
    return head + fletcher32(head).to_bytes(FCS_SIZE, "big")


def _memory_payload(mem_id: int, address: int, data: bytes) -> bytes:
    return _be(mem_id, 1, "mem_id") + _be(address, 2, "address") + _be(len(data), 2, "length") + bytes(data)


def pack_modification(mem_id: int, address: int, data: bytes) -> bytes:
    """Пакет заполнения памяти rotationobject."""
    return build_packet(MSG_MODIFY_ROT, _memory_payload(mem_id, address, data))


def pack_modification_static(mem_id: int, address: int, data: bytes) -> bytes:
    """Пакет заполнения памяти staticgroup."""
    return build_packet(MSG_MODIFY_STATIC, _memory_payload(mem_id, address, data))


def pack_parameters(obj_id: int, data: bytes) -> bytes:
    """Пакет параметров (iparam) объекта с номером ``obj_id``."""
    return build_packet(MSG_PARAMS, _be(obj_id, 2, "obj_id") + _be(len(data), 2, "length") + bytes(data))


def pack_layer3d(data: bytes) -> bytes:
    """Пакет положения/стороны для layer3d."""
    return build_packet(MSG_LAYER3D, data)


# =============================================================================
#  ШАБЛОНЫ ПОЛЕЙ iparam (запасной вариант, если в XML нет секции <parameters>)
# =============================================================================
OBJECT_TEMPLATES = {
    "layer3d": {
        "enb": {"offset": 0, "size": 1},
        "x0": {"offset": 1, "size": 12},
        "y0": {"offset": 13, "size": 12},
    },
    "rectangle": {
        "enb": {"offset": 0, "size": 1},
        "color": {"offset": 1, "size": 24},
        "colorb": {"offset": 25, "size": 24},
        "x0": {"offset": 49, "size": 12},
        "y0": {"offset": 61, "size": 12},
        "w": {"offset": 73, "size": 12},
        "h": {"offset": 85, "size": 12},
        "a": {"offset": 97, "size": 12},
    },
    "rectangle_a": {
        "enb": {"offset": 0, "size": 1},
        "color": {"offset": 1, "size": 24},
        "colorb": {"offset": 25, "size": 24},
        "alpha": {"offset": 49, "size": 3},
        "x0": {"offset": 52, "size": 12},
        "y0": {"offset": 64, "size": 12},
        "w": {"offset": 76, "size": 12},
        "h": {"offset": 88, "size": 12},
        "a": {"offset": 100, "size": 12},
    },
    "rotationobject": {
        "enb": {"offset": 0, "size": 1},
        "color": {"offset": 1, "size": 24},
        "xrot": {"offset": 25, "size": 12},
        "yrot": {"offset": 37, "size": 12},
        "top": {"offset": 49, "size": 12},
        "left": {"offset": 61, "size": 12},
        "bottom": {"offset": 73, "size": 12},
        "right": {"offset": 85, "size": 12},
        "sq": {"offset": 97, "size": 8},
        "sin": {"offset": 105, "size": 18},
        "cos": {"offset": 123, "size": 18},
    },
    "staticgroup": {
        "enb": {"offset": 0, "size": 1},
        "color": {"offset": 1, "size": 24},
        "x": {"offset": 25, "size": 12},
        "y": {"offset": 37, "size": 12},
        "w": {"offset": 49, "size": 12},
        "h": {"offset": 61, "size": 12},
        "addr": {"offset": 73, "size": 16},
    },
}

_SIGNED_FIELDS = {"top", "left", "bottom", "right"}
_COLOR_FIELDS = {"color", "colorb"}
_SIMPLE_TYPES = {"rectangle", "rectangle_a", "aviahorizont", "scale"}


def to_signed(value: int, size: int) -> int:
    """Беззнаковое значение размера ``size`` бит -> знаковое (дополнительный код)."""
    return value - (1 << size) if value & (1 << (size - 1)) else value


def _clean(text: str | None) -> str:
    """Убирает все пробельные символы из текста XML-узла."""
    return "".join((text or "").split())


# =============================================================================
#  XML
# =============================================================================
class VbXML:
    """Читает структуру объектов из XML-файла и умеет менять поля iparam."""

    def __init__(self, filename: str):
        self.filename = filename

    def change_val(self, old_hex: str, new_value: int, object_type: str, value_type: str) -> str:
        """Меняет одно поле в hex-строке iparam.

        Значение обрезается маской по размеру поля (для отрицательных чисел
        получится дополнительный код). Длина строки сохраняется.
        """
        try:
            spec = OBJECT_TEMPLATES[object_type][value_type]
        except KeyError:
            raise ValueError(f"Неизвестное поле {value_type!r} для объекта {object_type!r}") from None
        offset, size = spec["offset"], spec["size"]
        mask = (1 << size) - 1
        value = int(old_hex, 16)
        value = (value & ~(mask << offset)) | ((new_value & mask) << offset)
        length = len(old_hex)
        return format(value, "X")[-length:].zfill(length)

    def get_dict(self) -> dict:
        """Строит словарь со структурой XML-файла.

        Returns:
            dict с ключами ``background``, ``templates`` (если есть),
            ``objects`` и ``3d`` (если есть <layer3d>).
        """
        root = ET.parse(self.filename).getroot()
        objects: list[dict] = []
        param_id = 0
        mem_id = 0
        result: dict = {
            "background": {"width": int(root.attrib["width"]), "height": int(root.attrib["height"])}
        }

        for child in root:
            if child.tag == "parameters":
                result["templates"] = {
                    tpl.tag: {
                        field.tag: {k: int(v) for k, v in field.attrib.items()} for field in tpl
                    }
                    for tpl in child
                }
                continue

            for node in child:
                tag = node.tag

                if tag in _SIMPLE_TYPES:
                    for sub in node:
                        if sub.tag == "init":
                            objects.append({
                                "type": tag, "iparams": [_clean(sub.text)], "data": "",
                                "auxiliary": {"startParamIndex": param_id, "paramCount": 1},
                            })
                            param_id += 1

                elif tag == "rotationobject":
                    init = None
                    for sub in node:
                        if sub.tag == "init":
                            init = _clean(sub.text)
                        elif sub.tag == "data":
                            if init is None:
                                raise ValueError("rotationobject: <data> встретился раньше <init>")
                            objects.append({
                                "type": tag, "iparams": [init],
                                "data": _clean(sub.text).replace(",", ""),
                                "auxiliary": {"mem_id": mem_id, "startParamIndex": param_id, "paramCount": 1},
                            })
                            mem_id += 1
                    param_id += 1

                elif tag == "staticgroup":
                    inits: list[str] = []
                    for sub in node:
                        if sub.tag == "init":
                            inits.append(_clean(sub.text))
                        elif sub.tag == "data":
                            objects.append({
                                "type": tag, "iparams": inits,
                                "data": _clean(sub.text).replace(",", ""),
                                "auxiliary": {"mem_id": mem_id, "startParamIndex": param_id,
                                              "paramCount": len(inits)},
                            })
                            mem_id += 1
                    param_id += len(inits)

                elif tag == "layer3d":
                    for sub in node:
                        if sub.tag == "init":
                            result["3d"] = {"type": tag, "iparams": _clean(sub.text)}

                else:
                    logger.warning("Неизвестный тег в XML пропущен: <%s>", tag)

        result["objects"] = objects
        return result


# =============================================================================
#  Подготовка данных объектов
# =============================================================================
class VbDataWorks:
    """Достаёт из словаря XML параметры и данные памяти в формате для отправки."""

    def __init__(self, xml_dict: dict):
        self.xml_dict = xml_dict
        self.object_list = xml_dict["objects"]

    # ---- доступ к полям iparam -------------------------------------------
    def _field_spec(self, type_: str, name: str) -> tuple[int, int]:
        spec = self.xml_dict.get("templates", {}).get(type_, {}).get(name)
        if spec is None:
            spec = OBJECT_TEMPLATES.get(type_, {}).get(name)
        if spec is None:
            raise KeyError(f"Нет описания поля {name!r} для объекта {type_!r}")
        return spec["offset"], spec["size"]

    def _get_value(self, iparam_hex: str, type_: str, name: str, signed: bool = False) -> int:
        offset, size = self._field_spec(type_, name)
        value = (int(iparam_hex, 16) >> offset) & ((1 << size) - 1)
        return to_signed(value, size) if signed else value

    def get_current_values(self, index: int) -> dict:
        """Текущие значения всех полей iparam объекта (словарь имя -> значение)."""
        obj = self.object_list[index]
        iparam, type_ = obj["iparams"][0], obj["type"]
        names = self.xml_dict.get("templates", {}).get(type_) or OBJECT_TEMPLATES[type_]
        values = {}
        for name in names:
            value = self._get_value(iparam, type_, name, signed=name in _SIGNED_FIELDS)
            values[name] = hex(value) if name in _COLOR_FIELDS else value
        return values

    # ---- layer3d ----------------------------------------------------------
    def get_data_layer3d(self) -> str:
        """Сторона layer3d по x0 из XML: "left" (x0 < 512) или "right"."""
        layer = self.xml_dict.get("3d")
        if layer is None:
            raise ValueError("В XML нет секции <layer3d>")
        return "left" if self._get_value(layer["iparams"], "layer3d", "x0") < 512 else "right"

    # ---- параметры и память ----------------------------------------------
    def get_iparam_list(self, index: int) -> list[bytes]:
        """Список параметров объекта (каждый элемент - bytes)."""
        return [bytes.fromhex(p) for p in self.object_list[index]["iparams"]]

    def get_data_static(self, index: int) -> bytearray:
        """Данные памяти staticgroup: каждый hex-символ становится отдельным байтом (0x0N)."""
        return bytearray(int(c, 16) for c in self.object_list[index]["data"])

    def get_data_rot(self, index: int) -> list[bytes]:
        """Блоки памяти rotationobject (по одному 31-байтному блоку на квадрат 8x8)."""
        obj = self.object_list[index]
        iparam, type_ = obj["iparams"][0], obj["type"]
        data_mem = [int(c) for c in obj["data"]]

        left = self._get_value(iparam, type_, "left", signed=True)
        right = self._get_value(iparam, type_, "right", signed=True)
        top = self._get_value(iparam, type_, "top", signed=True)
        bottom = self._get_value(iparam, type_, "bottom", signed=True)
        width, height = right - left, bottom - top

        table = self._make_table(data_mem, height, width)
        blocks = self._make_blocks(width, height, table)

        result = []
        for block in blocks:
            # каждое 3-битное значение разворачивается; 81*3 = 243 бита + 5 нулей = 31 байт
            bits = "".join(format(v, "03b")[::-1] for v in block) + "0" * 5
            result.append(int(bits[::-1], 2).to_bytes(len(bits) // 8, "big"))
        return result

    @staticmethod
    def _make_table(data: list[int], height: int, width: int) -> list[list[int]]:
        """Список -> таблица height x width."""
        if len(data) < width * height:
            raise ValueError(f"Данных {len(data)} меньше, чем width*height = {width * height}")
        return [data[row * width:(row + 1) * width] for row in range(height)]

    @staticmethod
    def _make_blocks(width: int, height: int, table: list[list[int]]) -> list[list[int]]:
        """Режет таблицу на блоки 9x9 (9-я строка/столбец - копия 8-й), блок = список из 81."""
        blocks_w = -(-width // 8)
        blocks_h = -(-height // 8)
        pack: list[list[int]] = [[] for _ in range(blocks_w * blocks_h)]
        for bz in range(blocks_h):
            for line in range(9):
                row = (7 if line == 8 else line) + 8 * bz
                for sq in range(blocks_w):
                    for col_in_block in range(9):
                        col = (7 if col_in_block == 8 else col_in_block) + 8 * sq
                        inside = row < height and col < width
                        pack[sq + blocks_w * bz].append(table[row][col] if inside else 0)
        return pack


# =============================================================================
#  Загрузка в ПЛИС
# =============================================================================
class VbDataLoader:
    """Загружает объекты, параметры и данные памяти в ПЛИС через открытый ``serial.Serial``."""

    def __init__(self, ser: serial.Serial, xml_dict: dict):
        self.ser = ser
        self.xml_dict = xml_dict
        self.object_list = xml_dict["objects"]
        self.data_works = VbDataWorks(xml_dict)
        self.ser.write(b"\x00")  # нулевой байт в начале сеанса (как в исходном коде)

    def load_obj_iparam(self, index: int, data: list[bytes], params: list[int] | None = None) -> None:
        """Загружает параметры объекта.

        Args:
            index: номер объекта в XML.
            data: список параметров (bytes).
            params: какие элементы ``data`` отправить; None/пусто - все.
        """
        aux = self.object_list[index]["auxiliary"]
        obj_ids = list(range(aux["startParamIndex"], aux["startParamIndex"] + aux["paramCount"]))
        for k in (params if params else range(len(data))):
            self.ser.write(pack_parameters(obj_ids[k], data[k]))
            logger.debug("Параметр %s объекта %s загружен", obj_ids[k], index)

    def load_obj_data(self, index: int, data: bytes, address: int) -> None:
        """Загружает кусок памяти объекта по адресу ``address``."""
        obj = self.object_list[index]
        mem_id = obj["auxiliary"]["mem_id"]
        if obj["type"] == "rotationobject":
            packet = pack_modification(mem_id, address, data)
        elif obj["type"] == "staticgroup":
            packet = pack_modification_static(mem_id, address, data)
        else:
            raise ValueError(f"Объект {index} типа {obj['type']!r} не имеет памяти")
        self.ser.write(packet)

    def load_obj(self, index: int) -> None:
        """Загружает один объект целиком."""
        obj_type = self.object_list[index]["type"]
        works = self.data_works
        match obj_type:
            case "rectangle" | "rectangle_a" | "aviahorizont" | "scale":
                self.load_obj_iparam(index, works.get_iparam_list(index))
            case "rotationobject":
                self.load_obj_iparam(index, works.get_iparam_list(index))
                for k, block in enumerate(works.get_data_rot(index)):
                    self.load_obj_data(index, block, k)
            case "staticgroup":
                self.load_obj_iparam(index, works.get_iparam_list(index))
                data = works.get_data_static(index)
                for k in range(0, len(data), STATIC_CHUNK_SIZE):
                    self.load_obj_data(index, data[k:k + STATIC_CHUNK_SIZE], k)
            case _:
                logger.warning("Объект %s: неизвестный тип %r, пропущен", index, obj_type)
                return
        logger.info("Объект %s (%s) загружен", index, obj_type)

    def load_objects(self) -> None:
        """Загружает все объекты XML-файла."""
        logger.info("Всего объектов: %s", len(self.object_list))
        for i in range(len(self.object_list)):
            self.load_obj(i)
        logger.info("Кадр загружен")

    # В исходном коде left/right были в комментарии: left -> 0x01, right -> 0x00,
    # а в docstring Setup3D - наоборот (0 - left, 1 - right). СВЕРИТЬ с ПЛИС.
    LAYER3D_SIDE = {"left": b"\x01", "right": b"\x00"}

    def load_3d(self, side: str | None = None) -> None:
        """Выбирает сторону layer3d: "left"/"right"; если None - берётся из XML."""
        side = side or self.data_works.get_data_layer3d()
        try:
            payload = self.LAYER3D_SIDE[side]
        except KeyError:
            raise ValueError(f"side должен быть 'left' или 'right', получено {side!r}") from None
        self.ser.write(pack_layer3d(payload))


def setup_all(ser: serial.Serial, filename: str) -> None:
    """Загружает все объекты из XML-файла."""
    VbDataLoader(ser, VbXML(filename).get_dict()).load_objects()


def setup_obj(ser: serial.Serial, filename: str, index: int) -> None:
    """Загружает один объект с номером ``index``."""
    VbDataLoader(ser, VbXML(filename).get_dict()).load_obj(index)


def setup_3d(ser: serial.Serial, filename: str, side: str | None = None) -> None:
    """Устанавливает сторону layer3d ("left"/"right"; None - по XML)."""
    VbDataLoader(ser, VbXML(filename).get_dict()).load_3d(side)


def change_param(ser: serial.Serial, filename: str, index: int, new_value: int,
                 obj_type: str, value_type: str) -> None:
    """Меняет поле ``value_type`` у iparam объекта ``index`` и отправляет его в ПЛИС.

    Пример: change_param(ser, "Test.xml", 6, 0, "staticgroup", "enb")
    """
    xml = VbXML(filename)
    xml_dict = xml.get_dict()
    iparam = xml_dict["objects"][index]["iparams"][0]
    changed = [bytes.fromhex(xml.change_val(iparam, new_value, obj_type, value_type))]
    VbDataLoader(ser, xml_dict).load_obj_iparam(index, changed)


# =============================================================================
#  ПОЛЁТ: преобразование координат и упаковка 150-битного слова
#  Карта: 256x256 тайлов, 1 тайл = 1024 суб-ед., 1 точка = 64 суб-ед.
#  Окно QDR: 128x128 тайлов. Координата x/y - 18 бит (0..262143).
# =============================================================================
COURSE_MODULUS = 1 << 19                 # курс: 19 бит
COURSE_SCALE = COURSE_MODULUS / 360.0    # единиц ПЛИС на градус
HEIGHT_SCALE = 4                         # единиц ПЛИС на метр (2 бита мантиссы)
HEIGHT_RAW_MIN, HEIGHT_RAW_MAX = -65536, 65535   # 17-битный знаковый диапазон
MAX_HEIGHT_M = HEIGHT_RAW_MAX / HEIGHT_SCALE

COORD_MASK = 0x7FFFF                     # 18 бит
SUBUNITS_PER_TILE_SHIFT = 10             # 1 тайл = 1 << 10 суб-ед.
SUBUNITS_PER_POINT = 64
METERS_PER_POINT = 32.0                  # из формулы исходного кода; СВЕРИТЬ

SPEED_MAX_KMH = 1800.0                   # предел шкалы (15 бит при шаге 1/16 км/ч)
SPEED_WRAP_KMH = 100.0                   # при превышении предела скорость сбрасывается сюда
SPEED_STEP_KMH = 50.0                    # прибавка скорости за одно нажатие 'b'


def course_to_raw(angle_deg: float) -> int:
    """Угол в градусах -> 19-битное значение с заворотом на полном обороте."""
    return round(angle_deg * COURSE_SCALE) % COURSE_MODULUS


def meters_to_height_raw(meters: float) -> int:
    """Высота в метрах -> 17-битный дополнительный код."""
    raw = max(HEIGHT_RAW_MIN, min(HEIGHT_RAW_MAX, round(meters * HEIGHT_SCALE)))
    return raw & 0x1FFFF


def move_by_speed(speed_kmh: float, dt: float) -> float:
    """Перемещение за ``dt`` секунд при скорости ``speed_kmh``, в суб-единицах."""
    distance_m = speed_kmh / 3.6 * dt
    return distance_m / METERS_PER_POINT * SUBUNITS_PER_POINT


# ============================================================
# УПАКОВКА 150-БИТНОГО СЛОВА ПОЛОЖЕНИЯ
# Согласовано с SlowParameters (после расширения):
#     Xpos    [18:0]    19 бит   0..524287  (512 тайлов * 1024 суб-ед.)
#     Ypos    [37:19]   19 бит
#     Course  [56:38]   19 бит   0..524287  (полный оборот 360°)
#     Height  [73:57]   17 бит   знаковое (доп. код)
#     [149:74]          резерв, 0
# Передача: 19 байт little-endian, байт[0] = младшие 8 бит слова,
#           приходит первым. В RTL horizont[7:0] = байт[0] = Xpos[7:0].
# ============================================================
COORD_BITS  = 19
COURSE_MASK = 0x7FFFF        # 19 бит, как и было
HEIGHT_MASK = 0x1FFFF        # 17 бит, как и было
SUBUNITS_PER_TILE_SHIFT = 10 # 1 тайл = 1024 суб-ед., не меняем

# Предел тайла по одной оси теперь 2**19 / 2**10 = 512 тайлов.
MAP_TILES_MAX = (1 << COORD_BITS) >> SUBUNITS_PER_TILE_SHIFT    # 512

def pack_150bit(y_raw: int, x_raw: int, course: int, height: int,
                yscreen: int = 0, high_bits: int | None = None) -> bytes:
    """Упаковывает положение в 19 байт (152 бита, старшие 2 = 0).

    Поля совпадают с SlowParameters:
        x       19 бит  [18:0]
        y       19 бит  [37:19]
        course  19 бит  [56:38]
        height  17 бит  [73:57]
        yscreen 10 бит  [83:74]

    high_bits — дополнительные старшие биты слова (маска исключает биты 0..73),
    по умолчанию 0.
    """
    bits  = (x_raw  & COORD_MASK)  << 0
    bits |= (y_raw  & COORD_MASK)  << 19
    bits |= (course & COURSE_MASK) << 38
    bits |= (height & HEIGHT_MASK) << 57
    bits |= (yscreen & 0x3FF)      << 74

    if high_bits:
        high_mask = ((1 << 150) - 1) & ~((1 << 84) - 1)
        bits |= high_bits & high_mask

    return bits.to_bytes(19, "little")

# =============================================================================
#  ШКАЛА СКОРОСТИ (SPEEDSCALE RTL, src/shkala/SPEEDSCALE.v)
#  Отправляется как SetObject #10 (пакет параметров). Включается флагом
#  send_scale в interactive_control.
#
#  Раскладка 150-битного слова:
#    [0]        enable       1 = рисовать шкалу
#    [12:1]     x_pos        X верхнего левого угла оверлея
#    [24:13]    y_pos        Y верхнего левого угла оверлея
#    [39:25]    speed_ref    round(speed_kmh * 16), 1/16 км/ч
#    [55:40]    prediction   16 бит, знаковое (Q16.16, старшие 16 бит)
#    [87:56]    log_ref      делитель в ln()-отношении
#    [119:88]   speed_max    опорное значение для keff
#    [127:120]  alpha        прозрачность оверлея (0..255)
#    [128]      compression  разрешить сжатие
#    [149:129]  reserved     обязаны быть 0
# =============================================================================
SCALE_SETTINGS = {
    "enable": 1,
    "x_pos": 591,            # X_SCALE
    "y_pos": 140,            # Y_POINTER
    "speed_kmh": 1000.0,     # стартовая скорость (потом перезаписывается)
    "prediction": 0,
    "log_ref": 0x00164CCD,
    "speed_max": 0x00746666,
    "alpha": 0xFF,
    "compression": 1,
}
SCALE_OBJECT_ID = 10


def pack_scale_params(cfg: dict, speed_kmh: float | None = None) -> bytes:
    """Упаковывает параметры шкалы в 19 байт."""
    if speed_kmh is None:
        speed_kmh = cfg["speed_kmh"]
    speed_ref = int(round(float(speed_kmh) * 16.0)) & 0x7FFF
    bits = (cfg["enable"] & 0x1) << 0
    bits |= (cfg["x_pos"] & 0xFFF) << 1
    bits |= (cfg["y_pos"] & 0xFFF) << 13
    bits |= speed_ref << 25
    bits |= (cfg["prediction"] & 0xFFFF) << 40
    bits |= (cfg["log_ref"] & 0xFFFFFFFF) << 56
    bits |= (cfg["speed_max"] & 0xFFFFFFFF) << 88
    bits |= (cfg["alpha"] & 0xFF) << 120
    bits |= (cfg["compression"] & 0x1) << 128
    return bits.to_bytes(19, "little")


def send_scale_settings(ser: serial.Serial, speed_kmh: float,
                        cfg: dict = SCALE_SETTINGS, object_id: int = SCALE_OBJECT_ID) -> None:
    """Отправляет параметры шкалы с текущей скоростью."""
    ser.write(pack_parameters(object_id, pack_scale_params(cfg, speed_kmh)))


# =============================================================================
#  ВИЗУАЛИЗАЦИЯ КАРТЫ
# =============================================================================
class MapViewer:
    """Окно OpenCV: карта, окно QDR, сектор обзора, самолёт и трек."""

    WINDOW_SIZE = (1000, 800)
    WINDOW_NAME = "FPGA QDR MAP"

    # Параметры камеры из ПЛИС
    FOV_WIDTH_PX = 1024
    FOV_FOCUS = 0.5
    FOV_PIXEL_SIZE = 0.25387
    FOV_DEPTH = 1024
    FOV_DEPTH_MAIN = 1008        # "красная" дуга основной дальности
    FOV_DEPTH_PER_TILE = 16.0    # 1 тайл = 16 единиц глубины
    FOV_ARC_STEPS = 64

    PLANE_SHAPE = [(18, 0), (-9, 9), (-6, 0), (-9, -9)]

    def __init__(self, image_path: str, map_tiles: int = 256, window_tiles: int = 128):
        self.map_tiles = map_tiles
        self.window_tiles = window_tiles
        self.map_img = cv2.imread(image_path)
        if self.map_img is None:
            raise FileNotFoundError(image_path)

        self.map_h, self.map_w = self.map_img.shape[:2]
        self.tile_px_x = self.map_w / map_tiles
        self.tile_px_y = self.map_h / map_tiles
        self.window_w, self.window_h = self.WINDOW_SIZE

        # Масштаб и центрирование
        self.scale = min(self.window_w / self.map_w, self.window_h / self.map_h)
        self.offset_x = (self.map_w - self.window_w / self.scale) / 2
        self.offset_y = (self.map_h - self.window_h / self.scale) / 2

        # Состояние (позиция в тайлах, дробная)
        self.x = self.y = 0.0
        self.course = 0.0
        self.speed_ms = 0.0
        self.speed_kmh = 0.0
        self.path: deque[tuple[float, float]] = deque(maxlen=500)
        self.debug = False
        self.show_fov = True

        # Кэш отмасштабированной подложки: пересобирается только при зуме/сдвиге
        self._base = None
        self._base_key = None

        # Мышь
        self.drag = False
        self.start_x = self.start_y = 0
        self.start_ox = self.start_oy = 0.0

        cv2.namedWindow(self.WINDOW_NAME, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(self.WINDOW_NAME, self.window_w, self.window_h)
        cv2.setMouseCallback(self.WINDOW_NAME, self._on_mouse)

    # ---- публичный интерфейс ----------------------------------------------
    def set_debug(self, state: bool = True) -> None:
        self.debug = state

    def update(self, x_tiles: float, y_tiles: float, course_deg: float,
               speed_ms: float, speed_kmh: float) -> None:
        """Обновляет состояние и перерисовывает окно. Координаты - в тайлах (дробные)."""
        self.x, self.y = x_tiles, y_tiles
        self.course = course_deg
        self.speed_ms = speed_ms
        self.speed_kmh = speed_kmh
        self.path.append((x_tiles, y_tiles))
        self._draw()

    def close(self) -> None:
        cv2.destroyWindow(self.WINDOW_NAME)

    # ---- вспомогательное ---------------------------------------------------
    def _to_screen(self, tx: float, ty: float) -> tuple[int, int]:
        """Координаты в тайлах -> пиксели окна."""
        return (int((tx * self.tile_px_x - self.offset_x) * self.scale),
                int((ty * self.tile_px_y - self.offset_y) * self.scale))

    def _base_image(self):
        key = (self.scale, self.offset_x, self.offset_y)
        if key != self._base_key:
            m = np.float32([
                [self.scale, 0, -self.offset_x * self.scale],
                [0, self.scale, -self.offset_y * self.scale],
            ])
            img = cv2.warpAffine(self.map_img, m, (self.window_w, self.window_h))
            # затемнение области вне карты
            mask = np.zeros((self.window_h, self.window_w), dtype=np.uint8)
            x0, y0 = self._to_screen(0, 0)
            x1, y1 = self._to_screen(self.map_tiles, self.map_tiles)
            cv2.rectangle(mask, (max(0, x0), max(0, y0)),
                          (min(self.window_w, x1), min(self.window_h, y1)), 255, -1)
            self._base = cv2.bitwise_and(img, img, mask=mask)
            self._base_key = key
        return self._base

    def _draw(self) -> None:
        img = self._base_image().copy()
        if self.debug:
            self._draw_grid(img)
        self._draw_qdr(img)
        if self.show_fov:
            self._draw_fov_sector(img)
        self._draw_track(img)
        self._draw_plane(img)
        self._draw_info(img)
        cv2.imshow(self.WINDOW_NAME, img)
        cv2.waitKey(1)

    # ---- отрисовка элементов ----------------------------------------------
    def _fov_angles(self) -> tuple[float, float]:
        """Левый и правый углы сектора обзора относительно курса, рад."""
        k = self.FOV_PIXEL_SIZE / (self.FOV_FOCUS * self.FOV_WIDTH_PX)
        return math.atan(k * -511.5), math.atan(k * 512.5)

    def _draw_fov_sector(self, img) -> None:
        dist = self.FOV_DEPTH / self.FOV_DEPTH_PER_TILE
        rel_l, rel_r = self._fov_angles()
        course = math.radians(self.course)
        l_ang, r_ang = rel_l + course, rel_r + course
        center = self._to_screen(self.x, self.y)

        def arc(rad: float) -> list[tuple[int, int]]:
            n = self.FOV_ARC_STEPS
            pts = []
            for i in range(n + 1):
                a = l_ang + (r_ang - l_ang) * i / n
                pts.append(self._to_screen(self.x + math.cos(a) * rad, self.y + math.sin(a) * rad))
            return pts

        arc_pts = arc(dist)
        overlay = img.copy()
        cv2.fillPoly(overlay, [np.array([center] + arc_pts, np.int32)], (0, 255, 0))
        cv2.addWeighted(overlay, 0.15, img, 0.85, 0, img)
        cv2.polylines(img, [np.array(arc_pts, np.int32)], False, (0, 255, 0), 2)
        cv2.line(img, center, arc_pts[0], (0, 255, 0), 1)
        cv2.line(img, center, arc_pts[-1], (0, 255, 0), 1)

        red_pts = arc(dist * self.FOV_DEPTH_MAIN / self.FOV_DEPTH)
        cv2.polylines(img, [np.array(red_pts, np.int32)], False, (0, 0, 255), 2)
        fov_deg = math.degrees(rel_r - rel_l)
        cv2.putText(img, f"FOV {fov_deg:.1f} deg / depth {self.FOV_DEPTH}", (20, 160),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (220, 220, 220), 1)

    def _draw_qdr(self, img) -> None:
        half = self.window_tiles // 2
        ox = (int(self.x) - half) % self.map_tiles
        oy = (int(self.y) - half) % self.map_tiles

        def split(start: int, length: int) -> list[tuple[int, int]]:
            end = start + length
            return [(start, end)] if end <= self.map_tiles else [(start, self.map_tiles), (0, end - self.map_tiles)]

        for x1, x2 in split(ox, self.window_tiles):
            for y1, y2 in split(oy, self.window_tiles):
                cv2.rectangle(img, self._to_screen(x1, y1), self._to_screen(x2, y2), (0, 255, 0), 1)

    def _draw_plane(self, img) -> None:
        sx, sy = self._to_screen(self.x, self.y)
        a = math.radians(self.course)
        pts = []
        for dx, dy in self.PLANE_SHAPE:
            rx = dx * math.cos(a) - dy * math.sin(a)
            ry = dx * math.sin(a) + dy * math.cos(a)
            pts.append([int(sx + rx), int(sy + ry)])
        pts = np.array(pts, np.int32)
        cv2.fillPoly(img, [pts], (0, 0, 255))
        cv2.polylines(img, [pts], True, (255, 255, 255), 2)

    def _draw_track(self, img) -> None:
        # Карта замкнутая: при переходе через край трек разрываем, чтобы не было линии через всю карту
        segments: list[list[tuple[int, int]]] = [[]]
        prev = None
        half_map = self.map_tiles / 2
        for x, y in self.path:
            if prev is not None and (abs(x - prev[0]) > half_map or abs(y - prev[1]) > half_map):
                segments.append([])
            segments[-1].append(self._to_screen(x, y))
            prev = (x, y)
        for seg in segments:
            if len(seg) > 1:
                cv2.polylines(img, [np.array(seg, dtype=np.int32)], False, (0, 0, 255), 1)

    def _draw_grid(self, img) -> None:
        x0, y0 = max(0, self._to_screen(0, 0)[0]), max(0, self._to_screen(0, 0)[1])
        x1, y1 = self._to_screen(self.map_tiles, self.map_tiles)
        x1, y1 = min(self.window_w, x1), min(self.window_h, y1)
        for i in range(self.map_tiles + 1):
            x, y = self._to_screen(i, i)
            if 0 <= x <= self.window_w:
                cv2.line(img, (x, y0), (x, y1), (100, 100, 100), 1)
            if 0 <= y <= self.window_h:
                cv2.line(img, (x0, y), (x1, y), (100, 100, 100), 1)

    def _draw_info(self, img) -> None:
        lines = [
            f"Tile {int(self.x)},{int(self.y)}",
            f"Course {self.course:.1f}",
            f"Speed {self.speed_ms:.1f} m/s",
            f"Speed {self.speed_kmh:.1f} km/h",
        ]
        y = 25
        for text in lines:
            cv2.putText(img, text, (20, y), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
            y += 30

    # ---- мышь --------------------------------------------------------------
    def _on_mouse(self, event, x, y, flags, param) -> None:
        if event == cv2.EVENT_LBUTTONDOWN:
            self.drag = True
            self.start_x, self.start_y = x, y
            self.start_ox, self.start_oy = self.offset_x, self.offset_y
        elif event == cv2.EVENT_MOUSEMOVE and self.drag:
            self.offset_x = self.start_ox - (x - self.start_x) / self.scale
            self.offset_y = self.start_oy - (y - self.start_y) / self.scale
        elif event == cv2.EVENT_LBUTTONUP:
            self.drag = False
        elif event == cv2.EVENT_MOUSEWHEEL:
            self.scale = max(0.1, min(20, self.scale * (1.25 if flags > 0 else 0.8)))
            mx, my = x / self.scale + self.offset_x, y / self.scale + self.offset_y
            self.offset_x, self.offset_y = mx - x / self.scale, my - y / self.scale


# =============================================================================
#  ИНТЕРАКТИВНОЕ УПРАВЛЕНИЕ
# =============================================================================
class _KeyEdge:
    """Срабатывает один раз на нажатие клавиши (а не пока она удерживается)."""

    def __init__(self):
        self._prev: dict[str, bool] = {}

    def pressed(self, key: str) -> bool:
        now = keyboard.is_pressed(key)
        was = self._prev.get(key, False)
        self._prev[key] = now
        return now and not was


def interactive_control(ser: serial.Serial, map_image: str = DEFAULT_MAP,
                        x_start_tile: int = 0, y_start_tile: int = 0,
                        height_start_m: float = 16000.0, course_start_deg: float = 0.0,
                        step_forward: int = 8, step_side: int = 8,
                        speed_kmh: float = 1000.0,
                        step_height_m: float = 100.0, step_course_deg: float = 1.0,
                        yscreen: int = 1023,
                        update_period: float = 0.05, send_scale: bool = False) -> None:
    """Управление положением окна на карте.

    Ручной режим: стрелки - движение, H/G - высота, K/L - курс.
    Автопилот (F): движение по курсу со скоростью ``speed_kmh``;
    стрелки вверх/вниз - высота, влево/вправо - курс, B - прибавить скорость.
    Esc - выход.

    ``step_forward``/``step_side`` - шаг в суб-единицах за одно обновление, поэтому
    ручная скорость зависит от ``update_period`` (период цикла, с).
    """
    viewer = MapViewer(map_image, map_tiles=512,window_tiles=128)
    viewer.set_debug(True)  # отладочная сетка

    print("Карта 256x256 тайлов (1 тайл = 1024 суб-ед., 1 точка = 64 суб-ед.), окно QDR 128x128 тайлов")
    print("Управление: стрелки, H/G - высота, K/L - курс, F - автопилот, B - ускорение, Esc - выход")

    x_raw = (x_start_tile << SUBUNITS_PER_TILE_SHIFT) & COORD_MASK
    y_raw = (y_start_tile << SUBUNITS_PER_TILE_SHIFT) & COORD_MASK
    height_m = min(height_start_m, MAX_HEIGHT_M)
    course_deg = course_start_deg % 360.0
    auto_mode = False
    keys = _KeyEdge()

    print(f"Старт: тайл ({x_start_tile},{y_start_tile}) raw=0x{x_raw:05X},0x{y_raw:05X} "
          f"H={height_m:.1f} м курс={course_deg:.1f}°")

    prev_time = time.perf_counter()
    last_print = 0.0
    try:
        while not keyboard.is_pressed("esc"):
            now = time.perf_counter()
            dt = now - prev_time
            prev_time = now

            if keys.pressed("f"):
                auto_mode = not auto_mode
                print("\n*** АВТОПИЛОТ ВКЛЮЧЁН ***" if auto_mode else "\n*** Ручное управление ***")

            rad = math.radians(course_deg)
            cos_c, sin_c = math.cos(rad), math.sin(rad)

            if not auto_mode:
                move_fwd = keyboard.is_pressed("up") - keyboard.is_pressed("down")
                move_side = keyboard.is_pressed("right") - keyboard.is_pressed("left")
                dx = round(step_forward * move_fwd * cos_c - step_side * move_side * sin_c)
                dy = round(step_forward * move_fwd * sin_c + step_side * move_side * cos_c)
                if keyboard.is_pressed("h"):
                    height_m += step_height_m
                if keyboard.is_pressed("g"):
                    height_m -= step_height_m
                if keyboard.is_pressed("k"):
                    course_deg += step_course_deg
                if keyboard.is_pressed("l"):
                    course_deg -= step_course_deg
            else:
                step = move_by_speed(speed_kmh, dt)
                dx, dy = round(step * cos_c), round(step * sin_c)
                if keyboard.is_pressed("up"):
                    height_m += step_height_m
                if keyboard.is_pressed("down"):
                    height_m -= step_height_m
                if keyboard.is_pressed("left"):
                    course_deg -= step_course_deg
                if keyboard.is_pressed("right"):
                    course_deg += step_course_deg
                if keys.pressed("b"):
                    speed_kmh += SPEED_STEP_KMH
            if speed_kmh > SPEED_MAX_KMH:
                speed_kmh = SPEED_WRAP_KMH
            speed_ms = speed_kmh / 3.6

            x_raw = (x_raw + dx) & COORD_MASK
            y_raw = (y_raw + dy) & COORD_MASK
            height_m = max(0.0, min(height_m, MAX_HEIGHT_M))
            course_deg %= 360.0

            packet = pack_150bit(y_raw, x_raw, course_to_raw(course_deg), meters_to_height_raw(height_m), yscreen)
            ser.write(pack_layer3d(packet))
            if send_scale:
                send_scale_settings(ser, speed_kmh)

            tile_x = (x_raw >> SUBUNITS_PER_TILE_SHIFT) & 0x1FF
            tile_y = (y_raw >> SUBUNITS_PER_TILE_SHIFT) & 0x1FF
            if now - last_print >= 0.1:
                last_print = now
                point_x = (x_raw >> 6) & 0xF
                point_y = (y_raw >> 6) & 0xF
                print(f"\r[{'АВТО' if auto_mode else 'РУЧН'}] X=0x{x_raw:05X} Y=0x{y_raw:05X} "
                      f"tile=({tile_x},{tile_y}) pt=({point_x},{point_y}) H={height_m:.1f} м "
                      f"C={course_deg:.1f}° V={speed_ms:.1f} м/с ({speed_kmh:.1f} км/ч)   ",
                      end="", flush=True)

            viewer.update(x_raw / (1 << SUBUNITS_PER_TILE_SHIFT), y_raw / (1 << SUBUNITS_PER_TILE_SHIFT),
                          course_deg, speed_ms, speed_kmh)

            #pause = update_period - (time.perf_counter() - now)
            #if pause > 0:
            #    time.sleep(pause)
    finally:
        viewer.close()
        print("\nУправление завершено.")


# =============================================================================
#  ТОЧКА ВХОДА
# =============================================================================
def main() -> None:
    parser = argparse.ArgumentParser(description="Загрузка XML в ПЛИС и интерактивное управление полётом")
    parser.add_argument("--port", default=DEFAULT_PORT)
    parser.add_argument("--xml", default=DEFAULT_XML)
    parser.add_argument("--map", default=DEFAULT_MAP)
    parser.add_argument("--skip-setup", action="store_true", help="не загружать объекты из XML")
    parser.add_argument("--send-scale", action="store_true", help="отправлять параметры шкалы скорости")
    parser.add_argument("-v", "--verbose", action="store_true")
    args = parser.parse_args()

    logging.basicConfig(level=logging.DEBUG if args.verbose else logging.INFO,
                        format="%(asctime)s %(levelname)s %(message)s")

    # Один порт на всё время работы: загрузка на быстрой скорости, затем управление
    with serial.Serial(args.port, LOAD_BAUDRATE) as ser:
        if not args.skip_setup:
            setup_all(ser, args.xml)
            ser.flush()
        ser.baudrate = CONTROL_BAUDRATE

        # Примеры других операций:
        #   change_param(ser, args.xml, 6, 0, "staticgroup", "enb")
        #   setup_3d(ser, args.xml, "right")
        #   setup_obj(ser, args.xml, 5)

        interactive_control(
            ser,
            map_image=args.map,
            x_start_tile=0,
            y_start_tile=0,
            height_start_m=16000.0,
            course_start_deg=45.0,
            step_forward=8,
            step_side=8,
            speed_kmh=1400.0,
            step_height_m=200.0,
            update_period=0.02,
            step_course_deg=0.05,
            yscreen = 1023,
            send_scale=args.send_scale,
        )


if __name__ == "__main__":
    main()
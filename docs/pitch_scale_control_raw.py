
#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""UART-пульт настройки PITCH_SCALE для tube_graph.

Зависимость: pip install pyserial
Запуск:      py -3 pitch_scale_control_raw.py
"""
from __future__ import annotations

import argparse
import json
import queue
import threading
import time
import tkinter as tk
from tkinter import ttk, filedialog, messagebox
from tkinter.scrolledtext import ScrolledText

MAGIC = bytes((0x53, 0x41, 0x49, 0x4E))
BAUDS = (9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600)


def fletcher32(data: bytes) -> int:
    a = b = 0
    for byte in data:
        a = (a + byte) & 0xFFFF
        b = (a + b) & 0xFFFF
    return (b << 16) | a


def make_packet(param_id: int, word: int) -> bytes:
    """Пакет 0x03 с одним параметром PITCH_SCALE."""
    payload = word.to_bytes(8, "big")
    data = bytearray(
        MAGIC + bytes((0x03,)) + param_id.to_bytes(2, "big")
        + len(payload).to_bytes(2, "big") + payload
    )
    size = len(data) - len(MAGIC) + 4
    data[4:4] = size.to_bytes(2, "big")
    data += fletcher32(data).to_bytes(4, "big")
    return bytes(data)


def parse_iparam(text: str) -> int:
    value = text.strip().replace("_", "").replace(" ", "")

    if not value:
        raise ValueError("Введите iparam")

    if value.lower().startswith("150'h"):
        value = value[5:]
    elif value.lower().startswith("0x"):
        value = value[2:]

    try:
        word = int(value, 16)
    except ValueError as exc:
        raise ValueError("iparam должен быть шестнадцатеричным числом") from exc

    if word < 0 or word >= (1 << 150):
        raise ValueError("iparam превышает 150 бит")

    return word


def make_raw_packet(param_id: int, word: int) -> bytes:
    """Пакет 0x03 с полным iparam: 19 байт."""
    if not 0 <= param_id <= 0xFFFF:
        raise ValueError("ID param[] должен быть 0..65535")

    payload = word.to_bytes(19, "big")
    data = bytearray(
        MAGIC + bytes((0x03,)) + param_id.to_bytes(2, "big")
        + len(payload).to_bytes(2, "big") + payload
    )

    size = len(data) - len(MAGIC) + 4
    data[4:4] = size.to_bytes(2, "big")
    data += fletcher32(data).to_bytes(4, "big")

    return bytes(data)


def signed_q9_7(value: float, name: str) -> int:
    scaled = round(value * 128)

    if not -32768 <= scaled <= 32767:
        raise ValueError(f"{name}: значение вне диапазона signed Q9.7")

    return scaled & 0xFFFF


def pack_pitch_scale(
    enable: int,
    mode: int,
    cx: int,
    cy: int,
    pitch: float,
    rotation: float
) -> int:
    """Карта iparam:
    enable[0], mode[1], X[13:2], Y[25:14],
    pitch[41:26], rotation[57:42].
    Углы — signed Q9.7 в градусах.
    """
    if enable not in (0, 1) or mode not in (0, 1):
        raise ValueError("enable и scale_mode должны быть 0 или 1")

    if not 0 <= cx <= 4095 or not 0 <= cy <= 4095:
        raise ValueError("center_x и center_y должны быть в диапазоне 0..4095")

    if not -90 <= pitch <= 90:
        raise ValueError("pitch_angle должен быть в диапазоне -90..+90 градусов")

    if not -180 <= rotation <= 180:
        raise ValueError("rotation_angle должен быть в диапазоне -180..+180 градусов")

    word = 0
    word |= enable
    word |= mode << 1
    word |= cx << 2
    word |= cy << 14
    word |= signed_q9_7(pitch, "pitch_angle") << 26
    word |= signed_q9_7(rotation, "rotation_angle") << 42

    return word


class SerialLink:
    def __init__(self):
        self.ser = None
        self.rx = queue.Queue()
        self.stop = threading.Event()
        self.thread = None

    @property
    def is_open(self):
        return self.ser is not None and self.ser.is_open

    def open(self, port: str, baud: int):
        import serial

        self.close()

        self.ser = serial.Serial(
            port,
            baudrate=baud,
            bytesize=8,
            parity="N",
            stopbits=1,
            timeout=0.05
        )

        self.stop.clear()
        self.thread = threading.Thread(target=self._reader, daemon=True)
        self.thread.start()

    def _reader(self):
        while not self.stop.is_set():
            try:
                chunk = self.ser.read(256)

                if chunk:
                    self.rx.put(("rx", chunk))

            except Exception as exc:
                self.rx.put(("err", str(exc)))
                return

    def write(self, packet: bytes):
        self.ser.write(packet)
        self.ser.flush()

    def close(self):
        self.stop.set()

        if self.thread and self.thread.is_alive():
            self.thread.join(timeout=0.3)

        if self.ser:
            try:
                self.ser.close()
            except Exception:
                pass

        self.ser = None


SLIDER_SPECS = [
    ("center_x",       "center_x — центр X, px",       0, 4095, 1,   True, 300),
    ("center_y",       "center_y — центр Y, px",       0, 4095, 1,   True, 300),
    ("pitch_angle",    "pitch_angle — тангаж, °",    -90,   90, 0.5, False, 300),
    ("rotation_angle", "rotation_angle — поворот, °", -180, 180, 1, False, 300),
]

SLIDER_DEFAULTS = {
    "center_x": 960,
    "center_y": 540,
    "pitch_angle": 0.0,
    "rotation_angle": 0.0
}


def run_gui(args):
    root = tk.Tk()
    root.title("PITCH_SCALE — настройка параметров")
    root.minsize(860, 600)

    link = SerialLink()

    state = {
        "packet": None,
        "after": None
    }

    vars_: dict[str, tk.DoubleVar] = {}

    def write_log(message):
        log.configure(state="normal")
        log.insert("end", time.strftime("%H:%M:%S ") + message + "\n")
        log.see("end")
        log.configure(state="disabled")

    def set_text(widget, value):
        widget.delete("1.0", "end")
        widget.insert("1.0", value)

    def refresh_ports():
        try:
            from serial.tools import list_ports
            ports = [p.device for p in list_ports.comports()]
        except Exception:
            ports = []

        port_cb["values"] = ports

        if ports and port_var.get() not in ports:
            port_var.set(ports[0])

        write_log("Порты: " + (", ".join(ports) if ports else "не найдены"))

    def get_packet():
        param_id = int(id_var.get().strip(), 0)

        if not 0 <= param_id <= 0xFFFF:
            raise ValueError("ID param[] должен быть 0..65535")

        cx = int(round(vars_["center_x"].get()))
        cy = int(round(vars_["center_y"].get()))
        pitch = float(vars_["pitch_angle"].get())
        rotation = float(vars_["rotation_angle"].get())

        mode = int(mode_var.get().split("—")[0].strip())

        word = pack_pitch_scale(
            int(enable_var.get()),
            mode,
            cx,
            cy,
            pitch,
            rotation
        )

        return param_id, word, make_packet(param_id, word)

    def send(log_tx=False):
        if state["packet"] is None:
            return

        if not link.is_open:
            return

        try:
            link.write(state["packet"])

            if log_tx:
                write_log(
                    "TX: " + " ".join(f"{b:02X}" for b in state["packet"])
                )

        except Exception as exc:
            write_log("Ошибка UART: " + str(exc))
            disconnect()

    def rebuild():
        state["after"] = None

        try:
            param_id, word, packet = get_packet()

        except ValueError as exc:
            state["packet"] = None
            status_var.set("Ошибка: " + str(exc))
            set_text(iparam_text, "")
            set_text(packet_text, "")
            return

        state["packet"] = packet

        status_var.set(
            f"Корректно: запись в param[{param_id}], 8 байт данных"
        )

        set_text(iparam_text, f"150'h{word:038x}")
        set_text(packet_text, " ".join(f"{b:02X}" for b in packet))

        # Автоматическая передача при изменении параметров.
        send()

    def update_now():
        """Немедленное обновление при перемещении ползунка."""
        if state["after"] is not None:
            root.after_cancel(state["after"])
            state["after"] = None

        rebuild()

    def schedule_rebuild():
        """Обновление после ручного ввода."""
        if state["after"] is not None:
            root.after_cancel(state["after"])

        state["after"] = root.after(30, rebuild)

    def send_raw_iparam():
        try:
            param_id = int(id_var.get().strip(), 0)

            word = parse_iparam(raw_iparam_var.get())
            packet = make_raw_packet(param_id, word)

            set_text(iparam_text, f"150'h{word:038x}")
            set_text(packet_text, " ".join(f"{b:02X}" for b in packet))

            status_var.set(
                f"Ручной iparam: {word.bit_length()} значащих бит, "
                f"19 байт, param[{param_id}]"
            )

            if not link.is_open:
                write_log("iparam сформирован, но UART не подключён")
                return

            link.write(packet)

            write_log(
                "TX iparam: " + " ".join(f"{b:02X}" for b in packet)
            )

        except Exception as exc:
            messagebox.showerror("Отправка iparam", str(exc))
            write_log("Ошибка iparam: " + str(exc))

    def connect():
        try:
            link.open(port_var.get().strip(), int(baud_var.get()))

            conn_btn.configure(
                text="Отключить",
                command=disconnect
            )

            conn_lbl.configure(
                text=f"Подключено: {port_var.get()}",
                foreground="#070"
            )

            write_log(
                f"Открыт {port_var.get()} @ {baud_var.get()} 8N1"
            )

        except Exception as exc:
            messagebox.showerror("UART", str(exc))

    def disconnect():
        link.close()

        conn_btn.configure(
            text="Подключить",
            command=connect
        )

        conn_lbl.configure(
            text="Отключено",
            foreground="#a00"
        )

        write_log("Порт закрыт")

    def save_json():
        try:
            param_id, _, _ = get_packet()

            data = {
                "scale": "PITCH_SCALE",
                "param_id": param_id,
                "enable": int(enable_var.get()),
                "scale_mode": int(mode_var.get().split("—")[0]),
                **{k: v.get() for k, v in vars_.items()}
            }

            path = filedialog.asksaveasfilename(
                defaultextension=".json",
                filetypes=(("JSON", "*.json"),)
            )

            if path:
                with open(path, "w", encoding="utf-8") as f:
                    json.dump(data, f, ensure_ascii=False, indent=2)

                write_log("Сохранено: " + path)

        except Exception as exc:
            messagebox.showerror("Сохранение", str(exc))

    def load_json():
        path = filedialog.askopenfilename(
            filetypes=(("JSON", "*.json"),)
        )

        if not path:
            return

        try:
            with open(path, encoding="utf-8") as f:
                data = json.load(f)

            id_var.set(str(data.get("param_id", args.id)))
            enable_var.set(int(data.get("enable", 1)))

            scale_mode = int(data.get("scale_mode", 0))

            mode_var.set(
                f"{scale_mode} — " +
                ("Линейная" if scale_mode == 0 else "Логарифмическая")
            )

            for key in vars_:
                if key in data:
                    try:
                        vars_[key].set(float(data[key]))
                    except (TypeError, ValueError):
                        pass

            rebuild()
            write_log("Загружено: " + path)

        except Exception as exc:
            messagebox.showerror("Загрузка", str(exc))

    def poll_rx():
        try:
            while True:
                kind, payload = link.rx.get_nowait()

                write_log(
                    ("RX: " if kind == "rx" else "UART error: ") +
                    (
                        " ".join(f"{b:02X}" for b in payload)
                        if kind == "rx"
                        else payload
                    )
                )

                if kind == "err":
                    disconnect()

        except queue.Empty:
            pass

        root.after(60, poll_rx)

    # ---------------------------------------------------------
    # Верхняя панель
    # ---------------------------------------------------------

    top = ttk.LabelFrame(root, text="Выбор шкалы и соединение")
    top.pack(fill="x", padx=8, pady=7)

    ttk.Label(top, text="Шкала:").grid(
        row=0, column=0, padx=(7, 3), pady=6
    )

    scale_var = tk.StringVar(value="PITCH_SCALE")

    scale_cb = ttk.Combobox(
        top,
        textvariable=scale_var,
        width=18,
        values=("PITCH_SCALE",),
        state="readonly"
    )
    scale_cb.grid(row=0, column=1, padx=3)

    ttk.Label(top, text="ID param[]:").grid(
        row=0, column=2, padx=(14, 3)
    )

    id_var = tk.StringVar(value=str(args.id))

    ttk.Entry(
        top,
        textvariable=id_var,
        width=7
    ).grid(row=0, column=3, padx=3)

    ttk.Label(top, text="Порт:").grid(
        row=0, column=4, padx=(14, 3)
    )

    port_var = tk.StringVar(value=args.port)

    port_cb = ttk.Combobox(
        top,
        textvariable=port_var,
        width=12
    )
    port_cb.grid(row=0, column=5, padx=3)

    ttk.Button(
        top,
        text="Обновить",
        command=refresh_ports
    ).grid(row=0, column=6, padx=3)

    ttk.Label(top, text="Baud:").grid(
        row=0, column=7, padx=(10, 3)
    )

    baud_var = tk.StringVar(value=str(args.baud))

    ttk.Combobox(
        top,
        textvariable=baud_var,
        width=9,
        values=tuple(map(str, BAUDS))
    ).grid(row=0, column=8, padx=3)

    conn_btn = ttk.Button(top, text="Подключить")
    conn_btn.grid(row=0, column=9, padx=8)

    conn_lbl = ttk.Label(
        top,
        text="Отключено",
        foreground="#a00"
    )
    conn_lbl.grid(row=0, column=10, padx=4)

    # ---------------------------------------------------------
    # Параметры PITCH_SCALE
    # ---------------------------------------------------------

    body = ttk.LabelFrame(root, text="Параметры PITCH_SCALE")
    body.pack(fill="x", padx=8, pady=5)

    body.columnconfigure(1, weight=1)

    enable_var = tk.IntVar(value=1)

    ttk.Label(
        body,
        text="enable — включение шкалы"
    ).grid(row=0, column=0, sticky="w", padx=8, pady=5)

    ttk.Checkbutton(
        body,
        variable=enable_var,
        command=schedule_rebuild
    ).grid(row=0, column=1, sticky="w", padx=5)

    mode_var = tk.StringVar(value="0 — Линейная")

    ttk.Label(
        body,
        text="scale_mode — закон расположения"
    ).grid(row=1, column=0, sticky="w", padx=8, pady=5)

    ttk.Combobox(
        body,
        textvariable=mode_var,
        state="readonly",
        width=28,
        values=("0 — Линейная", "1 — Логарифмическая")
    ).grid(row=1, column=1, sticky="w", padx=5)

    mode_var.trace_add(
        "write",
        lambda *_: schedule_rebuild()
    )

    def add_slider(
        row, key, label, minimum, maximum,
        resolution, integer, length
    ):
        ttk.Label(
            body,
            text=label
        ).grid(row=row, column=0, sticky="w", padx=8, pady=3)

        var = tk.DoubleVar(value=SLIDER_DEFAULTS[key])
        vars_[key] = var

        entry_var = tk.StringVar(value="0")

        entry = ttk.Entry(
            body,
            textvariable=entry_var,
            width=9
        )
        entry.grid(
            row=row,
            column=2,
            sticky="w",
            padx=(8, 8),
            pady=3
        )

        syncing = {"busy": False}

        def fmt(v: float) -> str:
            return str(int(round(v))) if integer else f"{v:g}"

        def slider_changed(value=None):
            if syncing["busy"]:
                return

            syncing["busy"] = True

            try:
                v = var.get()

                if integer:
                    v = round(v)

                entry_var.set(fmt(v))

            finally:
                syncing["busy"] = False

            # Передача сразу при движении ползунка.
            update_now()

        def entry_changed(*_):
            if syncing["busy"]:
                return

            text = entry_var.get().strip().replace(",", ".")

            if not text:
                return

            try:
                v = float(text)
            except ValueError:
                return

            v = max(minimum, min(maximum, v))

            if integer:
                v = round(v)

            syncing["busy"] = True

            try:
                var.set(v)
            finally:
                syncing["busy"] = False

            schedule_rebuild()

        scale = tk.Scale(
            body,
            from_=minimum,
            to=maximum,
            resolution=resolution,
            orient="horizontal",
            showvalue=False,
            variable=var,
            length=length,
            command=slider_changed,
            highlightthickness=0
        )

        scale.grid(
            row=row,
            column=1,
            sticky="ew",
            padx=5,
            pady=3
        )

        entry_var.trace_add("write", entry_changed)

        # Начальное значение без отправки.
        entry_var.set(fmt(var.get()))

        return var

    for i, spec in enumerate(SLIDER_SPECS):
        add_slider(2 + i, *spec)

    # ---------------------------------------------------------
    # Сформированный iparam и UART-пакет
    # ---------------------------------------------------------

    out = ttk.LabelFrame(
        root,
        text="Сформированный iparam[149:0] и UART-пакет"
    )

    out.pack(
        fill="both",
        expand=True,
        padx=8,
        pady=6
    )

    status_var = tk.StringVar(value="")

    ttk.Label(
        out,
        textvariable=status_var
    ).pack(anchor="w", padx=8, pady=5)

    ttk.Label(
        out,
        text="iparam (150 бит):"
    ).pack(anchor="w", padx=8)

    iparam_text = tk.Text(
        out,
        height=2,
        wrap="word"
    )
    iparam_text.pack(fill="x", padx=8, pady=3)

    ttk.Label(
        out,
        text="Пакет TX:"
    ).pack(anchor="w", padx=8)

    packet_text = tk.Text(
        out,
        height=4,
        wrap="word"
    )
    packet_text.pack(fill="x", padx=8, pady=3)

    # ---------------------------------------------------------
    # Ручной ввод iparam
    # ---------------------------------------------------------

    raw_frame = ttk.Frame(out)
    raw_frame.pack(fill="x", padx=8, pady=(5, 2))

    ttk.Label(
        raw_frame,
        text="Ввести iparam вручную (150'h / hex):"
    ).pack(side="left")

    raw_iparam_var = tk.StringVar(
        value="150'h00000000000000000000000000000000000000"
    )

    raw_iparam_entry = ttk.Entry(
        raw_frame,
        textvariable=raw_iparam_var
    )
    raw_iparam_entry.pack(
        side="left",
        fill="x",
        expand=True,
        padx=6
    )

    raw_send_btn = ttk.Button(
        raw_frame,
        text="Отправить iparam",
        command=send_raw_iparam
    )
    raw_send_btn.pack(side="left")

    # ---------------------------------------------------------
    # Кнопки управления
    # ---------------------------------------------------------

    controls = ttk.Frame(out)
    controls.pack(fill="x", padx=8, pady=6)

    send_btn = ttk.Button(
        controls,
        text="Отправить",
        command=lambda: send(log_tx=True)
    )
    send_btn.pack(side="left")

    ttk.Button(
        controls,
        text="Сохранить настройки",
        command=save_json
    ).pack(side="right")

    ttk.Button(
        controls,
        text="Загрузить настройки",
        command=load_json
    ).pack(side="right", padx=5)

    # ---------------------------------------------------------
    # Журнал
    # ---------------------------------------------------------

    log_frame = ttk.LabelFrame(root, text="Журнал")
    log_frame.pack(
        fill="both",
        expand=True,
        padx=8,
        pady=(0, 8)
    )

    log = ScrolledText(
        log_frame,
        height=6,
        state="disabled"
    )
    log.pack(fill="both", expand=True, padx=5, pady=5)

    # ---------------------------------------------------------
    # Инициализация
    # ---------------------------------------------------------

    conn_btn.configure(command=connect)

    id_var.trace_add(
        "write",
        lambda *_: schedule_rebuild()
    )

    root.protocol(
        "WM_DELETE_WINDOW",
        lambda: (link.close(), root.destroy())
    )

    refresh_ports()
    rebuild()
    poll_rx()

    root.mainloop()


def main():
    parser = argparse.ArgumentParser(description=__doc__)

    parser.add_argument("--port", default="COM4")
    parser.add_argument("--baud", type=int, default=115200)

    parser.add_argument(
        "--id",
        type=lambda x: int(x, 0),
        default=0,
        help="индекс param[] для PITCH_SCALE в tube_graph"
    )

    args = parser.parse_args()
    run_gui(args)


if __name__ == "__main__":
    main()

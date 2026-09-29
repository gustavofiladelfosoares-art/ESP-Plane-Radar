#!/usr/bin/env python3
"""Talk to the Plane Radar over USB serial (debug keys handled in src/main.cpp).

  device_tool.py log [seconds]            print the serial log
  device_tool.py keys <chars> [seconds]   send debug keys, then print the log
  device_tool.py shot <out.png> [page]    capture the frame buffer as a PNG
                                          (page 0-4 switches first)

--port defaults to the first /dev/cu.usbmodem*. Pass --swap if the portal's
"Corrigir cores" option is on (pages are then drawn with red/blue swapped).
"""

import argparse
import glob
import sys
import time

import serial
from PIL import Image, ImageDraw


def open_port(port):
    port = port or (sorted(glob.glob("/dev/cu.usbmodem*")) or [None])[0]
    if port is None:
        sys.exit("no /dev/cu.usbmodem* port found — is the board plugged in?")
    return serial.Serial(port, 115200, timeout=0.2)


def read_for(s, seconds):
    end = time.time() + seconds
    out = b""
    while time.time() < end:
        out += s.read(4096)
    return out


def capture(s, swap, timeout=6.0):
    s.reset_input_buffer()
    s.write(b"s")
    buf = b""
    end = time.time() + timeout
    while time.time() < end and b"@@END" not in buf:
        buf += s.read(65536)
    start = buf.find(b"@@SHOT 240 240\n")
    if start < 0:
        raise RuntimeError("no screenshot header in serial data")
    raw = buf[start + len(b"@@SHOT 240 240\n"):][: 240 * 240 * 2]
    if len(raw) < 240 * 240 * 2:
        raise RuntimeError(f"short screenshot ({len(raw)} bytes)")
    img = Image.new("RGB", (240, 240))
    px = img.load()
    for i in range(240 * 240):
        v = (raw[2 * i] << 8) | raw[2 * i + 1]
        r = ((v >> 11) & 31) * 255 // 31
        g = ((v >> 5) & 63) * 255 // 63
        b = (v & 31) * 255 // 31
        px[i % 240, i // 240] = (b, g, r) if swap else (r, g, b)
    mask = Image.new("L", (240, 240), 0)
    ImageDraw.Draw(mask).ellipse((0, 0, 239, 239), fill=255)
    out = Image.new("RGB", (240, 240), (0, 0, 0))
    out.paste(img, (0, 0), mask)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["log", "keys", "shot"])
    ap.add_argument("args", nargs="*")
    ap.add_argument("--port")
    ap.add_argument("--swap", action="store_true")
    a = ap.parse_args()
    s = open_port(a.port)
    if a.cmd == "log":
        secs = float(a.args[0]) if a.args else 5
        sys.stdout.write(read_for(s, secs).decode("utf-8", "replace"))
    elif a.cmd == "keys":
        s.write(a.args[0].encode())
        secs = float(a.args[1]) if len(a.args) > 1 else 2
        sys.stdout.write(read_for(s, secs).decode("utf-8", "replace"))
    elif a.cmd == "shot":
        if len(a.args) > 1:
            s.write(a.args[1].encode())
            read_for(s, 2.5)  # let the entry animation finish
        capture(s, a.swap).save(a.args[0])
        print(f"saved {a.args[0]}")


if __name__ == "__main__":
    main()

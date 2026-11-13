"""Build the Meow Box RSP1 splash and a matching APNG preview from the UI art."""
import itertools

import re
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
W, H = 320, 172
ART = (ROOT / "src/clay_art.h").read_text()

def pixels(name):
    return list(map(int, re.search(rf"{name}\[\] = \{{(.*?)\}};", ART, re.S)[1].split(",")[:-1]))

def rgb(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)

def channels(color):
    return ((color >> 11) * 255 // 31, ((color >> 5) & 63) * 255 // 63, (color & 31) * 255 // 31)


def rounded_palette(color, palette):
    r, g, b = channels(color)
    return min(palette, key=lambda item: sum((a - b) ** 2 for a, b in zip((r, g, b), channels(item))))

ROOM = pixels("kClayRoom")
CAT = pixels("kClayKitten")
MOUTH = pixels("kClayKittenMouth")
CAT_W, CAT_H = 56, 80
room_palette = [rgb(*c) for c in [(13, 7, 17), (26, 12, 24), (44, 18, 28),
    (66, 27, 33), (91, 40, 31), (124, 59, 29), (171, 87, 36), (246, 168, 69), (255, 225, 140)]]
cat_palette = [rgb(*c) for c in [(31, 21, 41), (56, 46, 73), (81, 67, 98),
    (113, 94, 115), (151, 125, 132), (195, 163, 145), (234, 201, 171),
    (73, 101, 147), (122, 77, 57), (167, 106, 78)]]
glyphs = {
    "M": ["10001","11011","10101","10101","10001","10001","10001"],
    "E": ["11111","10000","10000","11110","10000","10000","11111"],
    "O": ["01110","10001","10001","10001","10001","10001","01110"],
    "W": ["10001","10001","10001","10101","10101","11011","10001"],
    "B": ["11110","10001","10001","11110","10001","10001","11110"],
    "X": ["10001","10001","01010","00100","01010","10001","10001"],
}

def frame(light, center, floor, meow=False, title=False):
    # The firmware has room for 24 KiB of runs, so use a deliberately limited palette.
    result = [0] * (W * H)
    for y in range(H):
        sy = min((y // 8) * 8 + 4, H - 1)
        for x in range(0, W, 20):
            color = ROOM[sy * W + min(x + 10, W - 1)]
            r, g, b = channels(color)
            out = rounded_palette(rgb(int(r * light), int(g * light), int(b * light)), room_palette)
            start = y * W + x
            result[start:start + min(20, W - x)] = [out] * min(20, W - x)
    if center is not None:
        for y in range(CAT_H):
            step = 1 if title and 18 <= y < 52 else 2 if title else 4
            for x in range(0, CAT_W, step):
                sx = min(x + step // 2, CAT_W - 1)
                col = CAT[y * CAT_W + sx]
                if not col:
                    continue
                if meow and 17 <= sx < 40 and 33 <= y < 48:
                    col = MOUTH[(y - 33) * 23 + sx - 17] or col
                start = (floor - CAT_H + y) * W + center - CAT_W // 2 + x
                result[start:start + min(step, CAT_W - x)] = [rounded_palette(col, cat_palette)] * min(step, CAT_W - x)
    if title:
        for i, letter in enumerate("MEOW BOX"):
            for y, line in enumerate(glyphs.get(letter, [])):
                for x, cell in enumerate(line):
                    if cell == "1":
                        for dy in (0, 1):
                            start = (12 + 2 * y + dy) * W + 10 + i * 12 + 2 * x
                            result[start:start + 2] = [rgb(255, 240, 214)] * 2
    return result

def write_png_animation(path, frames):
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))
    png = b"\x89PNG\r\n\x1a\n"
    png += chunk(b"IHDR", struct.pack(">2I5B", W, H, 8, 2, 0, 0, 0))
    png += chunk(b"acTL", struct.pack(">2I", len(frames), 0))
    sequence = 0
    for index, (delay, colors) in enumerate(frames):
        png += chunk(b"fcTL", struct.pack(">5I2H2B", sequence, W, H, 0, 0, delay, 1000, 0, 0))
        sequence += 1
        raw = b"".join(b"\0" + bytes(c for col in colors[y:y + W] for c in channels(col))
                       for y in range(0, W * H, W))
        compressed = zlib.compress(raw)
        if index == 0:
            png += chunk(b"IDAT", compressed)
        else:
            png += chunk(b"fdAT", struct.pack(">I", sequence) + compressed)
            sequence += 1
    path.write_bytes(png + chunk(b"IEND", b""))

frames = [
    (170, [room_palette[0]] * (W * H)),
    (170, frame(.40, None, 0)),
    (120, frame(.90, 130, 135)),
    (580, frame(.90, 160, 153, meow=True, title=True)),
]
payload = bytearray()
for delay, colors in frames:
    runs = [(sum(1 for _ in group), color) for color, group in itertools.groupby(colors)]
    payload += struct.pack("<II", delay, len(runs))
    for length, color in runs:
        payload += struct.pack("<HH", length, color)
data = struct.pack("<4s7I", b"RSP1", len(payload) + 32, zlib.crc32(payload),
                   sum(delay for delay, _ in frames), W, H, len(frames), 0) + payload
if len(data) > 24576:
    raise SystemExit(f"Splash exceeds 24 KiB: {len(data)} bytes")
rows = [", ".join(f"0x{byte:02x}" for byte in data[i:i+24]) for i in range(0, len(data), 24)]
(ROOT / "src/meow_splash.h").write_text(
    "// Generated by tools/make_splash.py.\n#pragma once\n#include <stdint.h>\n"
    + "alignas(4) static const uint8_t kMeowSplash[] = {\n    "
    + ",\n    ".join(rows) + "\n};\n")
(ROOT / "build").mkdir(exist_ok=True)
(ROOT / "build/meow-startup.rsp").write_bytes(data)
write_png_animation(ROOT / "assets/meow-startup.png", frames)
print(f"{len(data)} bytes, {len(frames)} frames, {sum(delay for delay, _ in frames)} ms")

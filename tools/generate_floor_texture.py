#!/usr/bin/env python3
"""Generates assets/textures/arena_floor.png: an original procedural floor panel
(dark metal plate with a bevelled border and a faint grid). Standard library only."""
import struct
import sys
import zlib
from pathlib import Path

SIZE = 128


def pixel(x, y):
    edge = min(x, y, SIZE - 1 - x, SIZE - 1 - y)
    base = 58
    if edge < 3:
        value = base + 40 - edge * 8          # bright bevel
    elif edge < 6:
        value = base - 22                      # dark groove
    else:
        value = base + ((x * 7 + y * 13) % 9) - 4   # subtle brushed noise
        if (x % 32 == 0) or (y % 32 == 0):
            value += 10                        # faint panel grid
    value = max(0, min(255, value))
    return (value, value + 6, value + 14, 255)


def main():
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parent.parent / "assets/textures/arena_floor.png"
    rows = b"".join(b"\x00" + b"".join(bytes(pixel(x, y)) for x in range(SIZE)) for y in range(SIZE))

    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", SIZE, SIZE, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(rows, 9)) + chunk(b"IEND", b"")
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(png)
    print(f"wrote {out}")


if __name__ == "__main__":
    main()

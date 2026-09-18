from __future__ import annotations

import argparse
import struct
import time
from pathlib import Path

HEADER_SIZE = 256
FRAME_SIZE = 8
GAME_ID = b"GALE01"


def controller(buttons: int = 0, stick=(128, 128), cstick=(128, 128), triggers=(0, 0)) -> bytes:
    flags = buttons | (1 << 15)
    return struct.pack("<HBBBBBB", flags, triggers[0], triggers[1], stick[0], stick[1], cstick[0], cstick[1])


def build_frames() -> bytes:
    neutral = controller()
    start = controller(1 << 0)
    a = controller(1 << 1)
    return neutral * 30 + start + neutral * 30 + a + neutral * 30


def build_dtm() -> bytes:
    frames = build_frames()
    frame_count = len(frames) // FRAME_SIZE
    header = bytearray(HEADER_SIZE)
    header[0:4] = b"DTM\x1a"
    header[4:10] = GAME_ID
    header[10] = 0
    header[11] = 1
    header[12] = 0
    struct.pack_into("<Q", header, 13, frame_count)
    struct.pack_into("<Q", header, 21, frame_count)
    struct.pack_into("<Q", header, 29, 0)
    struct.pack_into("<Q", header, 37, 0)
    struct.pack_into("<I", header, 45, 0)
    author = b"MeleeMod deterministic fallback"
    header[49 : 49 + len(author)] = author
    header[137] = 1
    header[138] = 1
    struct.pack_into("<Q", header, 237, 0)
    return bytes(header) + frames


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    payload = build_dtm()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    if args.output.read_bytes()[:4] != b"DTM\x1a" or args.output.stat().st_size <= HEADER_SIZE:
        raise SystemExit("invalid DTM output")
    print(f"wrote {args.output} bytes={args.output.stat().st_size} frames={(len(payload) - HEADER_SIZE) // FRAME_SIZE} timestamp={int(time.time())}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

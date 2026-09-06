#!/usr/bin/env python3
"""Hex-dump an .emualog file.

Runs on a PC, not on the ESP32. Its purpose is comparison: dump a log this
project wrote, dump one produced by Ecumaster's own software, and look at where
they differ.

    ./emualog_hexdump.py LOG.emualog                 # plain hex dump
    ./emualog_hexdump.py LOG.emualog --frames        # decode as 5-byte frames
    ./emualog_hexdump.py LOG.emualog --summary       # channel statistics
    ./emualog_hexdump.py A.emualog --compare B.emualog

The frame layout is the EMU serial protocol's:

    byte 0  channel id
    byte 1  magic, always 0xA3
    byte 2  value, high byte
    byte 3  value, low byte
    byte 4  checksum = (b0 + b1 + b2 + b3) & 0xFF

No claim is made here about a file header. This project writes none, because
the reference implementation it learned the format from writes none. If a real
Ecumaster log turns out to start with one, this tool will show it as an
unparseable run at offset 0 - which is exactly the thing worth looking for.
"""

import argparse
import collections
import sys

MAGIC = 0xA3
FRAME = 5


def read_log(path):
    try:
        with open(path, "rb") as handle:
            return handle.read()
    except OSError as error:
        sys.exit(f"cannot read {path}: {error}")


def hex_dump(data, width, limit):
    end = len(data) if limit is None else min(len(data), limit)
    for offset in range(0, end, width):
        chunk = data[offset:offset + width]
        hexed = " ".join(f"{b:02X}" for b in chunk)
        text = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
        print(f"{offset:08X}  {hexed:<{width * 3}} |{text}|")
    if end < len(data):
        print(f"... {len(data) - end} more bytes")


def frames(data):
    """Walk the file as consecutive 5-byte frames, flagging anything that is not."""
    offset = 0
    while offset + FRAME <= len(data):
        chunk = data[offset:offset + FRAME]
        checksum = (chunk[0] + chunk[1] + chunk[2] + chunk[3]) & 0xFF
        good = chunk[1] == MAGIC and checksum == chunk[4]
        yield offset, chunk, good, checksum
        offset += FRAME
    if offset != len(data):
        yield offset, data[offset:], False, None


def frame_dump(data, limit):
    shown = 0
    for offset, chunk, good, checksum in frames(data):
        if limit is not None and shown >= limit:
            print("...")
            return
        shown += 1

        hexed = " ".join(f"{b:02X}" for b in chunk)
        if len(chunk) != FRAME:
            print(f"{offset:08X}  {hexed:<14}  TRAILING {len(chunk)} byte(s), not a whole frame")
            continue

        channel = chunk[0]
        value = (chunk[2] << 8) | chunk[3]
        if good:
            print(f"{offset:08X}  {hexed:<14}  ch {channel:3d}  raw {value:5d}  0x{value:04X}")
        else:
            why = []
            if chunk[1] != MAGIC:
                why.append(f"magic {chunk[1]:02X} != A3")
            if checksum != chunk[4]:
                why.append(f"checksum {chunk[4]:02X} != {checksum:02X}")
            print(f"{offset:08X}  {hexed:<14}  BAD: {', '.join(why)}")


def summary(data, path):
    counts = collections.Counter()
    good = bad = 0
    for _, chunk, ok, _ in frames(data):
        if len(chunk) != FRAME:
            continue
        if ok:
            good += 1
            counts[chunk[0]] += 1
        else:
            bad += 1

    print(f"file            {path}")
    print(f"size            {len(data)} bytes")
    print(f"whole frames    {len(data) // FRAME}")
    print(f"remainder       {len(data) % FRAME} byte(s)")
    print(f"valid frames    {good}")
    print(f"invalid frames  {bad}")
    print(f"distinct chans  {len(counts)}")
    if counts:
        print("\nchannel   frames   share")
        total = sum(counts.values())
        for channel, n in sorted(counts.items()):
            print(f"{channel:7d}   {n:6d}   {100.0 * n / total:5.1f}%")


def compare(a_path, a, b_path, b):
    print(f"A  {a_path}  {len(a)} bytes")
    print(f"B  {b_path}  {len(b)} bytes")

    head = min(64, len(a), len(b))
    print(f"\nfirst {head} bytes")
    print("A:", " ".join(f"{x:02X}" for x in a[:head]))
    print("B:", " ".join(f"{x:02X}" for x in b[:head]))

    limit = min(len(a), len(b))
    first = next((i for i in range(limit) if a[i] != b[i]), None)
    if first is None:
        print(f"\nidentical for the first {limit} bytes")
    else:
        print(f"\nfirst difference at offset 0x{first:X} ({first}):"
              f" A={a[first]:02X} B={b[first]:02X}")
        print(f"  frame-aligned: {'yes' if first % FRAME == 0 else 'no'}"
              f" (offset % {FRAME} = {first % FRAME})")


def main():
    parser = argparse.ArgumentParser(description="Hex-dump an .emualog file.")
    parser.add_argument("log")
    parser.add_argument("--frames", action="store_true",
                        help="decode as 5-byte EMU frames instead of a raw dump")
    parser.add_argument("--summary", action="store_true",
                        help="channel statistics instead of a dump")
    parser.add_argument("--compare", metavar="OTHER",
                        help="compare against a second log, e.g. an Ecumaster one")
    parser.add_argument("--width", type=int, default=16,
                        help="bytes per line in the raw dump (default 16; try 5 "
                             "to line the dump up with frame boundaries)")
    parser.add_argument("--limit", type=int, default=None,
                        help="stop after this many lines or frames")
    args = parser.parse_args()

    data = read_log(args.log)

    if args.compare:
        compare(args.log, data, args.compare, read_log(args.compare))
    elif args.summary:
        summary(data, args.log)
    elif args.frames:
        frame_dump(data, args.limit)
    else:
        hex_dump(data, args.width, None if args.limit is None else args.limit * args.width)


if __name__ == "__main__":
    main()

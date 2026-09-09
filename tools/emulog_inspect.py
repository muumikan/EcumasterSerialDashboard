#!/usr/bin/env python3
"""Inspect an Ecumaster .emulog file.

Runs on a PC, not on the ESP32. Two jobs: understand a log EMU Classic Client
wrote, and check one this project wrote against it.

    ./emulog_inspect.py LOG.emulog                    # structure summary
    ./emulog_inspect.py LOG.emulog --hex 3            # hex-dump three records
    ./emulog_inspect.py LOG.emulog --csv RPM,MAP,CLT  # channels as CSV
    ./emulog_inspect.py LOG.emulog --channels         # what can be asked for
    ./emulog_inspect.py A.emulog --compare B.emulog   # first differing record

The format, as verified against real Client-written logs and their CSV exports
(see docs/emu-log-format.md):

    gzip( 12-byte header + N x 256-byte records )
    header  = 80 60 40 20 | 01 00 00 00 | 80 96 98 00
    record[i] == EDL-1 frame data[i+4]

That is, a record is the 260-byte EDL-1 frame with its 4-byte marker
`32 40 50 60` stripped. The channel map is therefore not duplicated here: it
is read at runtime out of lib/EDLSerial/src/EDLSerial.cpp, so this tool and the
firmware can never disagree about what a byte means.

The gzip stream deliberately has no trailer - the Client flushes as it logs and
never finalises the file - so `gunzip` reports "unexpected end of file" and
truncates its output. Decompression here tolerates that.
"""

import argparse
import re
import sys
import zlib
from pathlib import Path

HEADER = bytes([0x80, 0x60, 0x40, 0x20, 0x01, 0, 0, 0, 0x80, 0x96, 0x98, 0x00])
HEADER_SIZE = len(HEADER)
RECORD_SIZE = 256
MARKER_SKIP = 4  # EDL frame bytes dropped before the record starts

DEFAULT_SOURCE = Path(__file__).resolve().parent.parent / "lib/EDLSerial/src/EDLSerial.cpp"


def load_channel_map(source):
    """Read `frame.X = data[i]...` out of the vendored parser."""
    try:
        text = Path(source).read_text(encoding="utf-8")
    except OSError as error:
        sys.exit(f"cannot read the channel map from {source}: {error}")

    channels = {}
    for line in text.splitlines():
        match = re.match(r"\s*frame\.(\w+)\s*=\s*(.+);\s*$", line)
        if not match:
            continue
        name, expr = match.group(1), match.group(2).strip()

        percent7 = re.match(r"^\(uint8_t\)\(data\[(\d+)\] \* 100 / 127\)$", expr)
        if percent7:
            channels[name] = (int(percent7.group(1)), "percent7", 1.0)
            continue

        divider = 1.0
        tail = re.search(r"/\s*([\d.]+)f?\s*$", expr)
        if tail:
            divider = float(tail.group(1))
            expr = expr[: tail.start()].strip()

        for pattern, kind in (
            (r"^\(int16_t\)\(data\[(\d+)\] \| \(data\[\d+\] << 8\)\)$", "s16"),
            (r"^\(?data\[(\d+)\] \| \(data\[\d+\] << 8\)\)?$", "u16"),
            (r"^\(int8_t\)data\[(\d+)\]$", "s8"),
            (r"^\(uint8_t\)data\[(\d+)\]$", "u8"),
            (r"^data\[(\d+)\]$", "u8"),
        ):
            found = re.match(pattern, expr)
            if found:
                channels[name] = (int(found.group(1)), kind, divider)
                break
    if not channels:
        sys.exit(f"no channels found in {source} - has the parser been rewritten?")
    return channels


def decompress(path):
    """Inflate a log, tolerating the missing gzip trailer."""
    try:
        raw = Path(path).read_bytes()
    except OSError as error:
        sys.exit(f"cannot read {path}: {error}")

    if raw[:2] != b"\x1f\x8b":
        return raw, False  # already decompressed, e.g. a scratch file
    stream = zlib.decompressobj(16 + zlib.MAX_WBITS)
    data = stream.decompress(raw)
    data += stream.flush()
    return data, True


def records(data):
    count = (len(data) - HEADER_SIZE) // RECORD_SIZE
    for index in range(count):
        start = HEADER_SIZE + index * RECORD_SIZE
        yield data[start : start + RECORD_SIZE]


def value(record, spec):
    offset, kind, divider = spec
    position = offset - MARKER_SKIP
    if kind == "percent7":
        return record[position] * 100 // 127
    if kind in ("u8", "s8"):
        raw = record[position]
        if kind == "s8" and raw >= 0x80:
            raw -= 0x100
    else:
        raw = record[position] | (record[position + 1] << 8)
        if kind == "s16" and raw >= 0x8000:
            raw -= 0x10000
    return raw / divider if divider != 1.0 else raw


def summarise(data, compressed, path):
    header = data[:HEADER_SIZE]
    body = len(data) - HEADER_SIZE
    count = body // RECORD_SIZE
    trailing = body % RECORD_SIZE

    print(f"{path}")
    print(f"  gzip                {'yes' if compressed else 'no (raw)'}")
    print(f"  decompressed        {len(data)} bytes")
    print(f"  header              {header.hex(' ')}")
    if header != HEADER:
        print(f"    EXPECTED          {HEADER.hex(' ')}   <-- differs")
    print(f"  records             {count} x {RECORD_SIZE} bytes")
    if trailing:
        print(f"    trailing bytes    {trailing}   <-- not a whole record")

    if not count:
        return
    stamps = [r[0] | (r[1] << 8) for r in records(data)]
    steps = [(b - a) & 0xFFFF for a, b in zip(stamps, stamps[1:])]
    gaps = sum(1 for s in steps if s != 1)
    span = (stamps[-1] - stamps[0]) & 0xFFFF
    print(f"  frameStamp          {stamps[0]} -> {stamps[-1]}")
    print(f"    steps of 1        {len(steps) - gaps}/{len(steps)}")
    if gaps:
        print(f"    discontinuities   {gaps}")
    print(f"  duration @20 Hz     {span * 0.05:.2f} s ({span * 0.05 / 60:.1f} min)")


def hex_dump(data, limit):
    for index, record in enumerate(records(data)):
        if index >= limit:
            break
        print(f"record {index}  (file offset {HEADER_SIZE + index * RECORD_SIZE})")
        for offset in range(0, RECORD_SIZE, 16):
            chunk = record[offset : offset + 16]
            text = "".join(chr(b) if 32 <= b < 127 else "." for b in chunk)
            print(f"  {offset:3d}  {chunk.hex(' '):<47}  {text}")
        print()


def to_csv(data, channels, names, out):
    unknown = [n for n in names if n not in channels]
    if unknown:
        sys.exit(f"unknown channel(s): {', '.join(unknown)}")
    out.write("TIME;" + ";".join(names) + "\n")
    first = None
    for record in records(data):
        stamp = record[0] | (record[1] << 8)
        if first is None:
            first = stamp
        seconds = ((stamp - first) & 0xFFFF) * 0.05
        cells = []
        for name in names:
            got = value(record, channels[name])
            cells.append(f"{got:.3f}" if isinstance(got, float) else str(got))
        out.write(f"{seconds:.2f};" + ";".join(cells) + "\n")


def compare(left, right):
    if left[:HEADER_SIZE] != right[:HEADER_SIZE]:
        print("headers differ:")
        print(f"  A  {left[:HEADER_SIZE].hex(' ')}")
        print(f"  B  {right[:HEADER_SIZE].hex(' ')}")
        return 1
    a = list(records(left))
    b = list(records(right))
    if len(a) != len(b):
        print(f"record count differs: A has {len(a)}, B has {len(b)}")
    for index, (one, two) in enumerate(zip(a, b)):
        if one == two:
            continue
        offsets = [i for i in range(RECORD_SIZE) if one[i] != two[i]]
        print(f"first difference in record {index}, at record offsets {offsets[:16]}")
        print(f"  (EDL frame data[] indices {[i + MARKER_SKIP for i in offsets[:16]]})")
        return 1
    print(f"identical across {min(len(a), len(b))} records")
    return 0


def main():
    parser = argparse.ArgumentParser(description="Inspect an Ecumaster .emulog file.")
    parser.add_argument("log")
    parser.add_argument("--hex", type=int, metavar="N", help="hex-dump the first N records")
    parser.add_argument("--csv", metavar="LIST", help="comma-separated channels to print as CSV")
    parser.add_argument("--channels", action="store_true", help="list decodable channels")
    parser.add_argument("--compare", metavar="OTHER", help="compare against another log")
    parser.add_argument("--source", default=DEFAULT_SOURCE, help="parser to read the channel map from")
    args = parser.parse_args()

    channels = load_channel_map(args.source)

    if args.channels:
        for name in sorted(channels, key=lambda n: channels[n][0]):
            offset, kind, divider = channels[name]
            scale = "" if divider == 1.0 else f" / {divider:g}"
            print(f"  data[{offset}]  {kind:8s}{scale:12s}  {name}")
        return 0

    data, compressed = decompress(args.log)

    if args.compare:
        other, _ = decompress(args.compare)
        return compare(data, other)
    if args.hex is not None:
        hex_dump(data, args.hex)
        return 0
    if args.csv:
        to_csv(data, channels, [n.strip() for n in args.csv.split(",")], sys.stdout)
        return 0

    summarise(data, compressed, args.log)
    return 0


if __name__ == "__main__":
    sys.exit(main())

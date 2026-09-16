#!/usr/bin/env python3
"""Check the EDL decoder against Ecumaster's own format definition.

Runs on a PC, not on the ESP32.

    ./edl_check_storage.py            # report every disagreement
    ./edl_check_storage.py --all      # list all 195 channels, agreeing or not

`docs/ecu-formats/version1_211.xml` is authoritative for how wide a channel is,
whether it is signed, and what to divide the raw value by. This compares that
against what `lib/EDLSerial/src/EDLSerial.cpp` actually does, field by field.

The check exists because doing it by eye does not work. The first pass over the
vendored library was manual, found 22 wrong fields, and still missed three:
`idleAngleCorr`, `cam1Angle` and `cam2Angle` were all read unsigned. A signed
channel read unsigned is not subtly wrong - a 3 degree retard reads as +126 -
but it only shows up when the value goes negative, which can be the one moment
you cared about it.

Exits non-zero when anything disagrees, so it can gate a build.
"""

import argparse
import re
import sys
from pathlib import Path

from emulog_inspect import DEFAULT_SOURCE, load_channel_map

DEFAULT_XML = Path(__file__).resolve().parent.parent / "docs/ecu-formats/version1_211.xml"

# What each XML `storage` means for the decoded value.
STORAGE_KIND = {
    "ubyte": "u8",
    "sbyte": "s8",
    "word": "u16",
    "sword": "s16",
}

# Channels knowingly left as upstream has them; see lib/EDLSerial/README.md.
EXEMPT = {
    "fcProbability": "upstream targets firmware 1.226; channel is zero in every log available",
}


def load_format(xml_path):
    """Read name, storage and divider for every symbol in the XML."""
    try:
        text = Path(xml_path).read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        sys.exit(f"cannot read {xml_path}: {error}")

    fields = {}
    for match in re.finditer(r"<symbol\s+name\s*=\s*\"(\w+)\"([^>]*)", text):
        name, attrs = match.group(1), match.group(2)
        storage = re.search(r"storage\s*=\s*\"(\w+)\"", attrs)
        if not storage or name in fields:
            continue  # later mentions are gauge layout, not the definition
        divider = re.search(r"divider\s*=\s*\"?\s*([\d.]+)", attrs)
        fields[name] = (storage.group(1), float(divider.group(1)) if divider else 1.0)
    if not fields:
        sys.exit(f"no symbols found in {xml_path} - has the format changed?")
    return fields


def check(fields, channels):
    problems = []
    for name, (_offset, kind, divider) in sorted(channels.items()):
        if name not in fields:
            problems.append((name, "decoded, but no symbol of that name in the XML"))
            continue
        storage, want_divider = fields[name]
        if name in EXEMPT:
            continue

        want_kind = STORAGE_KIND.get(storage)
        if want_kind is None:
            continue  # percent7 and friends: no single right answer to compare
        if kind != want_kind and not (kind == "percent7" and storage == "ubyte"):
            problems.append((name, f"XML says {storage} ({want_kind}), decoder reads {kind}"))
        elif divider != want_divider:
            problems.append((name, f"XML divides by {want_divider:g}, decoder by {divider:g}"))
    return problems


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--xml", default=DEFAULT_XML, help="format definition")
    parser.add_argument("--source", default=DEFAULT_SOURCE, help="decoder to check")
    parser.add_argument("--all", action="store_true", help="list every channel")
    args = parser.parse_args()

    fields = load_format(args.xml)
    channels = load_channel_map(args.source)

    if args.all:
        for name, (offset, kind, divider) in sorted(channels.items(), key=lambda i: i[1][0]):
            storage, want = fields.get(name, ("?", 1.0))
            print(f"  data[{offset:3}]  {name:26} {kind:8} /{divider:<6g} "
                  f"xml: {storage:6} /{want:g}")
        print()

    problems = check(fields, channels)
    if not problems:
        print(f"{len(channels)} channels, all agree with {Path(args.xml).name}")
        for name, why in EXEMPT.items():
            print(f"  (exempt) {name}: {why}")
        return 0

    for name, why in problems:
        print(f"  {name:26} {why}")
    print(f"\n{len(problems)} of {len(channels)} channels disagree")
    return 1


if __name__ == "__main__":
    sys.exit(main())

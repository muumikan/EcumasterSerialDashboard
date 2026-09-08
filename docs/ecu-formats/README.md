# ECU format files

Ecumaster ships one `versionX_YYY.xml` per EMU Classic firmware version. It
defines every logged symbol, and the subset carrying a `channel` attribute is
what the ECU transmits over the serial link.

`lib/EMUSerial-master/extras/HeaderFromXml.py` turns one of these into the two
generated tables the decoder uses, `src/format/emuFormat.h` and
`emuStruct.h`.

## What is active right now

| | |
|---|---|
| Tables built from | **1.200** (`lib/EMUSerial-master/extras/version1_200.xml`) |
| ECU firmware in the car | **1.211** |

**These do not match**, and one channel is affected — see
[../ecu-protocol.md](../ecu-protocol.md#version-mismatch-channel-33).

## Why 1.211 is kept here and not in `extras/`

The generator scans its own directory for anything matching `version*.xml` and
keeps **the last match it happens to find**. With two version files sitting
there, which one gets used depends on directory order, which is not something
to leave to chance.

So the file lives here until it is actually adopted. Adopting it means moving
it into `extras/` **and deleting `version1_200.xml`**, not adding it alongside.

## Adopting a new version

1. Put exactly one `versionX_YYY.xml` in `lib/EMUSerial-master/extras/`.
2. Run the generator from that directory:
   ```bash
   cd lib/EMUSerial-master/extras && python3 HeaderFromXml.py
   ```
   It overwrites `../src/format/emuFormat.h` and `../src/format/emuStruct.h`.
3. Build. The compiler will point at every place a renamed or removed field
   was used — `emu_serial_adapter.cpp` is the only file that touches the
   struct, by design.
4. Update the table above and the note in `ecu-protocol.md`.

**Match the file to the firmware actually flashed in the ECU.** A newer EMU
firmware needs its own XML; using 1.211's tables against a later firmware
reintroduces exactly the mismatch this file exists to remove.

## Files here

- `version1_211.xml` — Ecumaster's format file for EMU Classic firmware 1.211.
  Kept because the original lived on a removable USB volume.

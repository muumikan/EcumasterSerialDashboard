# EMU Classic serial protocol

**This project does not implement the protocol.** It uses
[GTO2013/EMUSerial](https://github.com/GTO2013/EMUSerial), vendored unmodified
in `lib/EMUSerial-master/`, as the reference decoder. This document describes
what that code does, so the behaviour is understandable without reading it —
it is not an independent specification, and nothing here should be
reconstructed from guesswork.

## Link

| | |
|---|---|
| Baud | 19200 |
| Framing | 8N1 |
| Direction | ECU → dash. The dash never transmits. |
| Levels | RS-232 at the ECU, converted by a MAX3232 |

## Frame

Five bytes, one channel value per frame:

| Byte | Meaning |
|---|---|
| 0 | Channel id |
| 1 | Magic, always `0xA3` |
| 2 | Value, high byte |
| 3 | Value, low byte |
| 4 | Checksum |

Checksum is the 8-bit sum of the first four bytes:
`channel + magic + valueH + valueL`.

## Synchronisation

There is no start delimiter. The decoder keeps a rolling five-byte window:
each incoming byte shifts the window up and lands in the last position. When
the byte in the magic position reads `0xA3` **and** the checksum matches, the
window is a valid frame. Anything else is discarded silently and the window
keeps sliding.

This means a corrupt or partial frame costs nothing but is also never
reported — see the liveness caveat below.

## Channel decoding

`lib/EMUSerial-master/src/format/emuFormat.h` carries three parallel tables of
35 entries — channel id, divider, and storage type — plus a table of pointers
into `emu_data_t`. Storage types are unsigned/signed byte and unsigned/signed
16-bit word; 16-bit values are big-endian (`valueH << 8 | valueL`).

A divider of 1 means the value is stored as an integer. Any other divider
means the raw value is divided by it and stored as a `float`. **This is where
engineering units come from** — volts, bar, lambda, degrees — which is why the
adapter above it re-scales nothing.

A channel id that is not in the table is ignored. That is the mechanism by
which a firmware/format-file mismatch degrades: unknown channels vanish rather
than corrupting neighbouring fields.

The tables in this repository are format version 1.200, generated from
`extras/version1_200.xml`. The car runs EMU Classic firmware 1.211.

## Liveness

`EMUSerial::checkEmuSerial()` returns `void` and `decodeEmuFrame()` is
private, so there is no frame counter to read. `EmuSerialAdapter` therefore
reports **bytes consumed**, and `EngineDataModel` treats byte activity as
evidence the link is alive.

The consequence is honest to state: line noise on an otherwise dead link would
read as `Online`. In practice a floating RS-232 receiver input is quiet, so
this has not been a problem — but if it ever matters, the fix is a decoder
that reports valid frames, not a change in the model.

## Fields

`emuStruct.h` defines all 35 channels. This car measures a subset; see
[../wiring/signal-list.md](../wiring/signal-list.md) for which sensors are
actually fitted and which channels are therefore meaningless.

The `cel` word is a 16-bit fault bitfield. **The bit-to-fault mapping is not
in the reference implementation**, so the diagnostics page shows the raw word
and 16 numbered bits rather than inventing names for them.

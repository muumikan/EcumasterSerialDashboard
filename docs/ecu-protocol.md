# EMU Classic serial protocol

> This describes the **classic** protocol, one channel per 5-byte frame. The
> car now runs the **EDL-1** protocol instead: 260-byte frames carrying all 195
> channels at once. Both are in the tree, chosen at build time. The EDL-1
> framing and its lack of a checksum are covered in
> [architecture.md](architecture.md) and the
> [decision log](decision-log.md); its channels are listed in
> [edl-channels.md](edl-channels.md).

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

## Version mismatch: channel 33

Those two versions disagree about exactly one channel, and the dashboard is on
the wrong side of it.

1.200 has a defect: `pulseWidth` **and** `scondarypulseWidth` are both declared
on channel 7. The decoder scans the channel table and takes the first match, so
`scondarypulseWidth` never receives anything and sits at zero for ever.

1.211 fixes that by moving `scondarypulseWidth` to channel 33 — and drops
`afrTarget`, which held channel 33 in 1.200, from the transmitted set. It is
still in the XML, just without a `channel` attribute.

So channel 33 means two different things:

| | channel 33 |
|---|---|
| What the ECU sends (firmware 1.211) | `scondarypulseWidth`, word, ÷62, ms |
| What this firmware decodes (tables 1.200) | `afrTarget`, unsigned byte, ÷10, AFR |

`emu_data.afrTarget` therefore holds the low byte of a secondary injector pulse
width divided by ten. It is meaningless.

**Nothing displays it today**, so nothing on screen is currently wrong. The
Tune page's target comes from `lambdaTarget` on channel 32, which is identical
in both versions. `injPulseWidth2Ms` is not displayed either, and would read
zero regardless.

It is a trap rather than a live fault: the value is wrong and looks plausible,
and would start lying the moment someone puts it on a page.

Adopting 1.211 removes `afrTarget` from the struct, which makes the compiler
flag the one place it is read. AFR target is derivable anyway — lambda target
times 14.7 for petrol. See [ecu-formats/](ecu-formats/).

Nothing else differs. Channels 1-32 and 255 are identical in both versions, and
1.211 adds no new channels at all.

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

## Check-engine bits

The `cel` word is a 16-bit fault bitfield, and Ecumaster names the bits. The
mapping is not in the EMUSerial reference implementation - which is why the
diagnostics page showed bare numbers at first - but it is in Ecumaster's own
format definition, [ecu-formats/version1_211.xml](ecu-formats/version1_211.xml),
as `<paramlist name="checkEngine" bitfield="1">`:

| Bit | Flag | Bit | Flag |
|---|---|---|---|
| 0 | CLT | 6 | EGT ALARM |
| 1 | IAT | 7 | KNOCK |
| 2 | MAP | 8 | FF SENSOR |
| 3 | WBO | 9 | DBW |
| 4 | EGT1 | 10 | FPR |
| 5 | EGT2 | | |

**One inference sits in that table.** The paramlist numbers its entries from 1,
and this maps entry N to bit N-1. The file never says so outright, but it is
the only reading that works: `fuelCorrections` in the same file has sixteen
entries for a sixteen-bit word, so its last entry, value 16, has to mean bit
15. Every other bitfield list in the file fits the same way.

Confirm it on the car rather than trusting it: unplug the intake air
temperature sensor and IAT should be the flag that lights. If the flag one
place along lights instead, the table is off by one.

Bits 11 to 15 have no name in the 1.211 definition. The diagnostics page shows
the raw word alongside the flags, so a bit outside the table is still visible.

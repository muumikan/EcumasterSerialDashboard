# .emualog file format

What this project writes to the microSD card, where the format came from, and —
importantly — which parts of it are verified and which are not.

## What the firmware writes

A single unbroken sequence of **validated 5-byte EMU serial frames, byte for
byte as the ECU sent them**. Nothing else:

- no file header
- no footer
- no in-file timestamps
- no padding, no record separators
- no channel filtering — every valid frame is written, including channels this
  car has no sensor for

Frames that fail the protocol's own checks are dropped rather than written, so
resync garbage never reaches the file. Each frame is:

| Byte | Meaning |
|---|---|
| 0 | Channel id |
| 1 | Magic, always `0xA3` |
| 2 | Value, high byte |
| 3 | Value, low byte |
| 4 | Checksum, `(b0 + b1 + b2 + b3) & 0xFF` |

Files are named `/00001.emualog`, `/00002.emualog`, … — the lowest free number
at boot. A new file per power-up.

Inspect one with [`tools/emualog_hexdump.py`](../tools/emualog_hexdump.py).

## Where the format came from

From reading
[danuecumaster/ECUMaster-ESP32-Bluetooth-Dashboard-Logger](https://github.com/danuecumaster/ECUMaster-ESP32-Bluetooth-Dashboard-Logger)
(GPLv3), specifically `logEMU()`, `readFrame()` and `getNextFilename()` in
`main.ino`.

That project's README states:

> Logs are now written in **native ECUMaster HEX format**

and its `logEMU()` is, in full substance, `memcpy` of the 5-byte frame into a
buffer that is written to the card. **There is no frame construction step.** The
"format" is the wire protocol, stored verbatim.

That is worth stating plainly, because it changes what work there is to do: this
project does not need to *build* log records, only to *keep* the bytes it is
already receiving.

## Verified

- **The frame layout.** Independently implemented in this project from the
  vendored `EMUSerial` decoder, and it matches the reference byte for byte.
- **What the reference project writes.** Read directly from its source.
- **What this firmware writes.** Its own code, and `emualog_hexdump.py` will
  confirm it against any file produced.

## NOT verified — do not assume these

- **That Ecumaster's PC software opens this format at all.** This rests
  entirely on a third-party project's claim. No official Ecumaster
  specification of a log file format was found. The reference repository ships
  a `Docs/EMUSerialProtocol.pdf`, but its pages carry no extractable text —
  they appear to be scanned images — so it could not be checked.
- **That `.emualog` is Ecumaster's own extension.** It may equally be that
  project's invention. If the PC software matches on extension, a rename may be
  needed.
- **That no header is required.** The reference writes none. Whether the
  software tolerates a bare frame stream, or expects a preamble identifying the
  firmware or format version, is unknown.
- **How playback timing is derived.** There are no timestamps anywhere in the
  file. The software must infer the time axis from frame order and rate, which
  means log fidelity depends on capturing *every* frame at the rate the ECU
  sent them. This firmware does not decimate, but the assumption is untested.
- **Whether the format version matters.** This project's channel tables are
  format 1.200 against firmware 1.211; the reference ships `version2_138.xml`.
  If the software keys off a version it cannot see in the file, this is moot —
  but that is an assumption, not a finding.

## How to settle it

1. Record a log with Ecumaster's own software from the same ECU.
2. Record one with this firmware.
3. Compare:

```bash
tools/emualog_hexdump.py mine.emualog --compare ecumaster_reference.log
tools/emualog_hexdump.py ecumaster_reference.log --frames --limit 20
```

If the Ecumaster file starts with a header, the frame decode will show an
unparseable run at offset 0 — which is exactly the signature to look for. If the
first difference is not frame-aligned, the record size differs.

Until that comparison is done, treat compatibility as unconfirmed.

## Licensing

The reference project is GPLv3. **No code was copied from it.** What was taken
is the description of a file format — validated 5-byte frames concatenated
verbatim, `.emualog` extension, buffered writes — which is a fact about a data
layout rather than an expression of it.

`src/logging/emu_log.cpp` is written from that description plus the frame layout
this project already implements independently. The attribution block in
[`include/emu_log.hpp`](../include/emu_log.hpp) records the provenance.

If any actual code from that project is ever pulled in, this repository's
licensing has to be revisited before it happens.

## Implementation notes

The logger taps the byte stream through `EmuLogTap`, a pass-through `Stream`
placed between the UART and the vendored decoder. This indirection exists
because `EMUSerial` reads the port itself and never exposes the bytes it
consumed — `checkEmuSerial()` returns `void` and `decodeEmuFrame()` is private.
Tapping the stream avoids modifying the reference decoder.

`EmuLogTap::write()` is inert, so the read-only rule survives the change.

Frames are buffered 512 bytes at a time and flushed at least every 5 seconds.
**A card write can block for tens of milliseconds**, which at 19200 baud happens
roughly every 270 ms. Whether that is visible as a hitch in the LVGL UI has not
been measured on hardware; if it is, moving the writes to the second core is the
obvious fix.

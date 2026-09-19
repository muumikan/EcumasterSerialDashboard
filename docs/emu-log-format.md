# The .emulog file format

What EMU Classic Client writes, how that was established, and what this project
has to do to write a file the Client opens.

This document replaces an earlier one that described a different format
entirely. That earlier description — validated 5-byte classic serial frames
concatenated into a `.emualog` file — was learned from a third-party project and
never verified. It is wrong. Nothing below rests on it.

## The format

```
.emulog = gzip( 12-byte header + N x 256-byte records )
```

**Header**, identical in every sample seen:

| Bytes | Value | Meaning |
|---|---|---|
| 0–3 | `80 60 40 20` | Marker |
| 4–7 | `01 00 00 00` | Version, presumably. Always 1 |
| 8–11 | `80 96 98 00` | 10 000 000 as u32 LE. Purpose unknown |

**Record** — one complete sample, little-endian throughout:

```
record[i] == EDL-1 frame data[i+4]
```

A record is the 260-byte EDL-1 frame with its 4-byte marker `32 40 50 60`
stripped. 260 − 4 = 256. There is no per-record header, no separator, and no
timestamp.

That single line is the whole of it, and it is the reason this became easy: the
channel map already exists in [`lib/EDLSerial/src/EDLSerial.cpp`](../lib/EDLSerial/src/EDLSerial.cpp),
so nothing has to be decoded, assembled, scaled, or looked up to write a log.
The bytes arriving from the ECU are, minus four, the bytes the file wants.

**Compression.** The deflate stream is flushed periodically with `Z_SYNC_FLUSH`
and **never finalised**: no final block, no CRC32, no ISIZE. Both Client-written
samples end exactly on a `00 00 ff ff` flush boundary. This is not corruption,
it is how a logger writes a file it may never get to close. The practical
consequences:

- `gunzip` reports `unexpected end of file` and **silently truncates its
  output**. Use [`tools/emulog_inspect.py`](../tools/emulog_inspect.py), or
  `zlib.decompressobj(31)`, which tolerate it.
- The firmware never has to close a log cleanly. Cutting power loses at most
  the bytes since the last flush.

The gzip header the Client writes is the minimal 10 bytes, `1f 8b 08 00
00000000 00 0b` — no name, no mtime, OS 0x0b.

## The time axis

There are no timestamps in the file. `frameStamp` (`data[4:6]`, so record bytes
0–1) is the ECU's own sample counter, incrementing by 1 per record, and the
Client derives the time axis from it — 0.05 s per count in both samples, i.e.
20 Hz. Since the counter comes from the ECU inside the frame, writing frames
verbatim gets the time axis right for free.

The log's date and time come from the **filename**: `20230519_0615_08.emulog`.
That is the only thing the writer contributes that the ECU does not, and the
only reason logging needs the RTC.

This dashboard splits that name across a folder and a file - `/20260919/
1732_04.emulog` - so a season of logs is navigable on the card. Nothing in the
format sees the difference, and downloading one through the service page puts
the date back into the filename.

## How this was established

Antti supplied two Client-written logs and three CSV exports, two of them the
same log exported with different channel selections. Cross-checking the CSVs
against the byte stream pinned the structure without ambiguity:

- **Record size and alignment.** Record count matched CSV row count exactly in
  both logs — 11 421 and 20 387 rows against 11 422 and 20 388 records, the
  Client's export dropping the first record in each. `(length − 12) mod 256`
  is exactly 0 for both files.
- **The offset relation.** 17 channels — `RPM` `MAP` `TPS` `IAT` `Batt` `VE`
  `IgnAngle` `accEnrich` `wboRI` `wboVS` `wboLambda` `wboAFR` `wboIPMeas`
  `wboIPNorm` `wboHeaterDC` `fuelPressure` `ignSparkCount` `deltaFPR`
  `lambdaTarget` — matched `EDLSerial.cpp`'s field map at `data[i+4]` across all
  20 387 samples, to the CSV's 3-decimal rounding. The channels span record
  offsets 2 to 220, so this is not coincidence.
- **The stripped marker.** The byte sequence `32 40 50 60` occurs **zero** times
  in 5.2 MB of decompressed log.

**Then it was proven by writing one.** A file rebuilt from the same data by an
independent writer — our 10-byte gzip header, raw deflate, `Z_SYNC_FLUSH` every
6 records, no trailer — opens in EMU Classic Client and graphs correctly. The
format is not inferred; it has been produced and read back. Feeding
reconstructed 260-byte frames through `EmuLog`'s exact algorithm reproduces
that file byte for byte, so the firmware's output is validated ahead of the
hardware.

**The first record is an ordinary sample.** The Client's CSV export skips it,
which initially looked like a preamble. It is not: record 0 decodes to
plausible values and its `frameStamp` continues into the sequence. The exporter
simply has no previous sample to measure the first interval against. Nothing
special has to be written at the head of the file.

## What the Client requires

- **gzip is mandatory.** An uncompressed file with a correct header and correct
  records opens *without an error message* and graphs nothing. Note the failure
  mode: the Client does not report a bad file, it shows an empty graph. During
  firmware bring-up, "the Client opened it" proves nothing — the graph has to
  show data.
- A missing gzip trailer is fine; the Client's own files have none.
- A 32 KB deflate window is fine. Our test file used one and compressed better
  than the Client's own output.

## Writing one on the ESP32-S3

miniz's deflate compressor is in the chip's ROM and exported by the linker
script — `tdefl_init`, `tdefl_compress`, `tdefl_compress_buffer` — so it costs
no flash. `TDEFL_SYNC_FLUSH` is supported, and omitting `TDEFL_WRITE_ZLIB_HEADER`
produces the raw deflate that belongs under our own gzip header.
`tdefl_compressor` is 167–320 KB depending on the ROM build, which belongs in
PSRAM. At 20 Hz the input is 4.8 kB/s and the output about 1.3 kB/s — roughly
4.6 MB per hour.

This is what `src/logging/emu_log.cpp` now does. `EdlSerialAdapter` offers each
frame that passes its three framing checks to a `FrameSink` before decoding
anything from it, and `EmuLog` drops four bytes and compresses. Nothing is
decoded on the way into a log, so no decode bug can reach one.

Confirmed on the car on 10 September 2026: logs written by the dashboard open
in EMU Classic Client and read the same as ones the Client recorded itself.

## Still unknown

- **The header's third word**, 10 000 000. Constant in both samples, so it is
  copied verbatim; whether the Client reads it is untested.
- **Whether the filename pattern is required** or merely conventional.
- **Whether the record layout is stable across EMU firmware versions.** Both
  samples are one car on one firmware. The layout is the EDL-1 frame layout, so
  this is really the question of whether that frame changes between versions.

## Licensing

The earlier version of this document carried a GPLv3 attribution to
`danuecumaster/ECUMaster-ESP32-Bluetooth-Dashboard-Logger`, from which a
description of a supposed log format had been taken. That format is not this
one, none of it survives, and the attribution is withdrawn as inapplicable.
Everything here was derived from Antti's own log files and from
`docs/ecu-formats/version1_211.xml`, Ecumaster's own format definition, which
ships with their software.

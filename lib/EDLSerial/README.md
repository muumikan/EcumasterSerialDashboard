# EDLSerial

Arduino library for decoding ECUMaster EMU Serial Logger stream (EDL-1 mode).

> **This copy is modified.** Upstream is
> [ThiloZ/EDLSerial](https://github.com/ThiloZ/EDLSerial) (MIT). See
> [Local corrections](#local-corrections) at the end for what changed and why.
> Do not replace it with an upstream pull without re-applying them.

## Benefit over traditional serial protocol
- more channels available
- faster due to higher baud rate
- let you use EDL-1 and custom dash/gauges in parallel

## Features
- Parses 260-byte EDL frames
- Exposes engine data like RPM, Lambda, IAT, EGT, etc.
- Works on any Arduino-compatible board with UART

## Disclamer
- tested with EMU Classic FW version 1.226
- tested with ESP32
- not all channels are tested, please contact me if you notice something strange
- CAN channels for EGT and wheelspeed not implemented

## Hardware
- ECU Master ECUs (both Classic and Black) use RS232 (15V), so for an Arduino or ESP32 you will need an converter board to TTL (5V/3.3V)! MAX3232CPE works just fine.
 
## Usage
Default all available data streamed by the EMU is captured and parsed. For performance and memory optimisation it's strongly advised to comment or delete unused channels. Therefore you have to modify the library on your own. If you do so make shure to modify both files below the same way!

EDLTypes.h
 -> from line 7 to 201 all available channels are definded

EDLSerial.cpp
 -> from line 29 to 223 channels are parsed

```cpp
#include <EDLSerial.h>

EDLSerial edl;

void setup() {
  Serial.begin(115200);  // default serial to print to console
  Serial1.begin(115200); // RS232 from EMU
  edl.begin(Serial1);
}

void loop() {
  if (edl.update()) {                   //updates the frame, returns true if frame is valid
    Serial.println(edl.getFrame().RPM); //returns value from last captured frame
	Serial.println(edl.getFrame().MAP);
	Serial.println(edl.getFrame().TPS);
	Serial.println(edl.getFrame().wboLambda);
  }
}
```

## Local corrections

Every field `parseFrame()` decodes was cross-checked against Ecumaster's own
format definition, [`docs/ecu-formats/version1_211.xml`](../../docs/ecu-formats/version1_211.xml),
whose `storage` attribute states each channel's width and signedness. That
found 25 fields the library read wrongly. All are fixed here.

The check is now scripted rather than done by eye: the first pass was manual
and missed three signed fields, which is how `idleAngleCorr` — the one the
IDLE page needs — survived it. `tools/edl_check_storage.py` re-derives every
`storage` and `divider` from the XML and reports any field the decoder reads
with the wrong signedness or scale. Run it after touching `parseFrame()`.

**Signed values read as unsigned.** The library took the raw byte or word
without a cast, so any negative reading appeared as a large positive one — a
2° ignition retard became +127°, a −1.5 mA pump current became +510 mA.
Confirmed against a real 18-minute log: `wboIPMeas` and `wboIPNorm` were wrong
in 83 % of samples, `dwellError` in 23 %.

| XML `storage` | Fields |
|---|---|
| `sbyte` | `IgnAngle` `idleAngleCorr` `dwellError` `iatIgnTrim` `cltIgnTrim` `ignFromTable` `nitrousIgnMod` `accIgnCorr` `etcDeltaError` `alsIgnAngle` `etcFrictionCorr` `timerIgnCorr` |
| `sword` | `wboIPMeas` `wboIPNorm` `pidPTerm` `pidITerm` `pidDTerm` `cam1Angle` `cam2Angle` `cam1AngleTarget` `cam2AngleTarget` `etcError` |

`idleAngleCorr`, `cam1Angle` and `cam2Angle` were missed by the first pass and
fixed later. The cam pair is the trap the manual check fell into: the two
`…Target` fields next to them were caught, and the eye read the group as done.
`idleAngleCorr` matters most — idle ignition correction is negative whenever
the ECU pulls timing to hold the target, which at idle is most of the time, so
the channel was wrong precisely when it was worth reading.

**`percent7` read as a raw byte.** `wboHeaterDC` and `sparkCutPercent` are
7-bit percentages: the raw value spans 0…127 across 0…100 %. Reading it raw
overstates by 27 % at full scale. Now `raw * 100 / 127`. The sample log's
`wboHeaterDC` reaches exactly 127, which is what confirms the range.

**`CLT` made explicit.** It was `data[140] | (data[141] << 8)` assigned to an
`int16_t` field, so it happened to wrap to the right value — but only because
of an implementation-defined narrowing conversion. Now cast openly. Behaviour
is unchanged; the guarantee is not. This one matters: `EdlSerialAdapter`
rejects frames outside −60…250 °C, so a sub-zero coolant reading decoded as
~65500 would have stalled the whole dashboard on a Finnish winter start.

### Known, deliberately not changed

`fcProbability` (`data[246]`) is divided by 2 here, but the 1.211 XML declares
it `ubyte` with divider 1. Upstream was written against firmware 1.226, so
this may be a real version difference rather than a bug. The channel is all
zeroes in every log available, so neither reading could be confirmed. Left as
upstream has it.

### Not fixed here

Framing. The library still validates only the 4-byte marker `32 40 50 60`,
carries no checksum, and on a mismatch discards all 260 buffered bytes, which
preserves a phase error indefinitely. `EdlSerialAdapter` compensates with a
byte-wise sliding window, a next-marker-at-exactly-260-bytes check, and a
plausibility gate. See [`include/edl_serial_adapter.hpp`](../../include/edl_serial_adapter.hpp).

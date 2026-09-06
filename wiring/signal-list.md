# Signal list — ECU to dashboard

Sources:

- Car loom: `Carina_1GGTE_Ecumaster_rev16.pdf`
- ECU port: [Ecumaster BT Module manual](https://www.ecumaster.com/files/Manuals/BT_Module_Manual.pdf), document version 1.1

## The link: EMU Classic extension port → MAX3232 → ESP32-S3

The dashboard **replaces the Ecumaster BT Module** and plugs into the same
place: the EMU Classic's extension port. That port carries the ECU's serial
output and its own supply.

The port speaks **RS-232 voltage levels** — nominally ±5 to ±15 V, idling
*negative*. An ESP32-S3 GPIO is a 3.3 V input and would be destroyed by that
directly, so a MAX3232 converts between the two.

```
  EMU Classic                MAX3232                    ESP32-S3
  extension port

  serial out ───RS-232──▶  R1IN ───▶ R1OUT ──3.3 V TTL──▶  GPIO18  (UART1 RX)
                           T1OUT ◀── T1IN  ◀──────────────  GPIO17  (UART1 TX)

  3.3 V  ─────────────────▶ VCC          ← the ECU powers the level shifter
  GND    ─────────────────▶ GND  ─────────────────────────  dash ground
  +5 V   ── DO NOT CONNECT
```

### Supply: 3.3 V from the ECU, never 5 V

The MAX3232 takes its power from the extension port, not from the panel. The
BT Module manual is explicit about that port, and the warning transfers
directly to anything else plugged into it:

> The module is designed to operate at 3.3 V supply voltage only.
> Pin 5 (+5 V) must not be used.
> Applying 5 V may result in permanent damage to the device.

Two things follow, and both are easy to get wrong:

- **Leave the +5 V pin unconnected.** It is present on the port and it is not
  the supply.
- **Do not tie the ECU's 3.3 V to the panel's 3.3 V.** The MAX3232 is powered
  from one rail only — the ECU's. Bridging two independently regulated 3.3 V
  rails invites current to flow between them. **Only the grounds are common**,
  and they must be, or RS-232 signalling has no reference.

The MAX3232's TTL-side output swings to whatever VCC it is given, which is why
the supply voltage decides whether the ESP32 survives: from 3.3 V it drives a
3.3 V logic level into GPIO18, which is correct.

One consequence of powering from the ECU: with the ignition off the level
shifter is unpowered even if the dash is running on USB. The dash simply reads
`OFFLINE`, which is the honest state.

### The TX line

GPIO17 is wired through to the port and assigned in `board_config.hpp`,
because that is the configuration the link was actually verified with.

**It is never driven.** No code in this project writes to the ECU port — the
provider and the adapter are the only files that touch it, and neither calls
`write()`. The manual also describes the port as one-way:

> Data transmission: One-way (ECU → external device)

So the TX line is present but idle. If you ever want the hardware guarantee
back instead of the software one, set `board::kEcuTxPin` to `-1` and the pin
is never attached to the UART at all.

### Practical notes

- **Module silkscreens lie.** On breakout boards "RXD" sometimes labels the
  TTL side and sometimes the RS-232 side. Trust the function, not the label:
  the pin going to the ECU must be an *RS-232 input* (R1IN), and the pin going
  to GPIO18 must be a *TTL output* (R1OUT). If you get no data, this is the
  first thing to swap.
- **No inversion is needed in software.** RS-232 idles low-true; the MAX3232
  receiver hands the UART a normal idle-high TTL stream, so `SERIAL_8N1` is
  correct as-is.
- **Charge-pump capacitors.** The MAX3232 generates its ±RS-232 rails
  internally and needs four external capacitors (typically 100 nF) on C1±,
  C2±, V+ and V−. Ready-made modules already have them; a bare chip does not.
- **Enable the stream in the ECU.** The EMU Classic Client has to have the
  *ECUMASTER serial protocol* switched on, then the change made permanent
  ("Make permanent" on the toolbar). Without it the port stays quiet.

### Link parameters

| | |
|---|---|
| Baud | 19200 |
| Framing | 8N1 |
| Direction | ECU → dash. The dash never transmits. |
| Dash RX | GPIO18 (UART1-OUT connector RX) |
| Dash TX | GPIO17 — wired and assigned, never driven |
| Level shifter supply | 3.3 V from the EMU extension port |

### To fill in: extension port pin numbering

The manual states the supply rule in text but gives the pin numbering only in
a figure, which could not be read from the PDF as text. **Pin 5 is +5 V and
must not be used** — that much is written out. The pins carrying 3.3 V, ground
and the serial lines have to be read off the manual's connector drawing before
anything is soldered.

Record them here once confirmed.

## ECU sensor inputs (EMU Classic connector B)

What the car actually has, read from the schematic. This is what decides which
channels the dashboard displays.

| Pin | Signal | Sensor |
|---|---|---|
| B1 | EGT In #1 | Marked *(optio)* — **not fitted** |
| B2 | Knock Sensor In #1 | Knock sensor #1 |
| B3 | Analog In #2 | see below |
| B4 | CLT In | Coolant temperature |
| B5 | WBO Vs | Wideband lambda |
| B6 | Camsync In #2 | |
| B7 | Primary trigger In | VR crank sensor |
| B9 | EGT In #2 | Marked *(optio)* — **not fitted** |
| B10 | Knock Sensor In #2 | Knock sensor #2 |
| B11 | Analog In #3 | see below |
| B12 | TPS In | Throttle position |
| B13 | WBO Ip | Wideband lambda |
| B14 | VSS In | **Not fitted** |
| B15 | Camsync #1 | |
| B17 | ECU Ground | |
| B18 | Sensor Ground | |
| B19 | Analog In #4 | see below |
| B20 | Analog In #1 | see below |
| B21 | IAT In | Intake air temperature |
| B23 | +5 V supply | Sensor supply |
| B24 | Power Ground | |

Two three-wire pressure senders are fitted — **oil pressure, 200 psi** and
**fuel pressure, 100 psi** — each on one of the four analog inputs. Which
sender sits on which input could not be read from the schematic, because the
wire routing is drawn as plain lines with no text. It does not affect the
firmware: the EMU fills `oilPressure` and `fuelPressure` directly, provided
both channels are assigned in the EMU software. If those assignments are
missing, the values arrive only as raw volts on `analogIn1..4`.

MAP and barometric pressure come from the EMU's internal sensor; there is no
external MAP sensor in the loom.

### Channels the ECU streams but this car cannot measure

Oil temperature, fuel level, flex-fuel ethanol content and its temperature,
gear position, vehicle speed, and both EGT channels. The ECU sends them
regardless; the dashboard shows none of them.

# Signal list — ECU to dashboard

Source for the car side: `Carina_1GGTE_Ecumaster_rev16.pdf`.

## The link: EMU Classic → MAX3232 → ESP32-S3

The EMU Classic's serial port speaks **RS-232 voltage levels** — nominally
±5 to ±15 V, idling *negative*. An ESP32-S3 GPIO is a 3.3 V input and would be
destroyed by that directly. A MAX3232 converts between the two.

Because the dashboard only ever listens, just one of the MAX3232's two
receivers is used. Both of its drivers stay unconnected.

```
  EMU Classic                MAX3232                    ESP32-S3
  serial TX  ──RS-232──▶  R1IN ──▶ R1OUT  ──3.3 V TTL──▶  GPIO18   (UART1 RX)

                          VCC ◀── 3.3 V from the CrowPanel
                          GND ◀── common with ECU ground and dash ground

  (nothing)               T1IN / T1OUT unused — the dash never transmits
```

### The one detail that matters: power the MAX3232 from 3.3 V

The MAX3232 is a 3 V–5.5 V part, and its TTL-side output swings to whatever
**VCC** you give it. Powered from 5 V it will drive R1OUT to 5 V, straight
into a 3.3 V GPIO.

**Take VCC from a 3.3 V rail, not 5 V.** This is the single easiest way to
kill the board, and it is not obvious, because the chip works fine on 5 V —
it is the ESP32 that does not survive it.

### Practical notes

- **Module silkscreens lie.** On breakout boards "RXD" sometimes labels the
  TTL side and sometimes the RS-232 side. Trust the function, not the label:
  the pin going to the ECU must be an *RS-232 input* (R1IN), and the pin going
  to GPIO18 must be a *TTL output* (R1OUT). If you get no data, this is the
  first thing to swap.
- **Grounds must be common** between the ECU, the MAX3232 and the dash.
  Without a shared reference, RS-232 signalling is meaningless.
- **No inversion is needed in software.** RS-232 idles low-true; the MAX3232
  receiver hands the UART a normal idle-high TTL stream, so `SERIAL_8N1` is
  correct as-is.
- **Charge-pump capacitors.** The MAX3232 generates its ±RS-232 rails
  internally and needs four external capacitors (typically 100 nF) on C1±,
  C2±, V+ and V−. Ready-made modules already have them; a bare chip does not.
- **Do not wire the dash side of the link back to the ECU.** The firmware
  leaves UART1's TX pin unassigned on purpose (`board::kEcuTxPin = -1`), so
  even a software mistake cannot transmit — but only if the wire is absent
  too.

### Link parameters

| | |
|---|---|
| Baud | 19200 |
| Framing | 8N1 |
| Direction | ECU → dash only |
| Dash input | GPIO18 (UART1-OUT connector RX) |

### To fill in

The exact EMU Classic terminal that carries the serial output is not recorded
here yet — add it from the loom. Note also that `rev16` of the schematic shows
a **Bluetooth module** block; if that module is connected to the same serial
port, check whether the port can drive both or whether the dash replaces it.

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

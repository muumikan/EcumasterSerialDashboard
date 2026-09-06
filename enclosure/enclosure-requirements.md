# Enclosure requirements

A 3D-printed housing for the CrowPanel Advance 3.5" and the MAX3232, mounted
in the cabin of a Toyota Carina.

## What it has to do

**Survive a car.** Vibration is constant and the dashboard sees a wider
temperature range than a desk. Nothing may rely on friction alone: the panel
is retained mechanically, and the case is retained mechanically to the car.

**Be readable at a glance while driving.** The screen is the whole point of
the object. That drives three things: the bezel is as thin as printing allows
so the 480 × 320 area is not cropped, the face is angled toward the driver
rather than sitting flat, and the surface around the screen is matt black so
windscreen reflections do not wash it out. A glossy print here is a real
usability defect, not a cosmetic one.

**Keep the level shifter inside.** The MAX3232 is a small module on the ECU
side of the wiring. Housing it in the same case means one cable leaves the
enclosure toward the ECU and one USB-C cable leaves for power, instead of a
loose board taped behind the dashboard.

**Not trap heat.** The ESP32-S3 driving an LVGL UI at 40 MHz SPI is not hot,
but a sealed black box on a sunny dashboard is a different matter. Vents on
the back and bottom, none on top where they would collect dust and drink.

**Come apart.** Firmware is flashed over USB-C, so that port stays reachable
without opening anything. Everything else — reseating the ECU cable, swapping
the MAX3232 — should need one screwdriver and no cutting.

## Fixed constraints

| | |
|---|---|
| Display active area | 480 × 320 px, 3.5" diagonal |
| Panel orientation | Landscape |
| Touch | Capacitive — the bezel must not overhang the glass edge |
| Ports used | USB-C (power and flashing), UART1-OUT connector (ECU) |
| Ports unused | UART0-IN, SD card, speaker, microphone |

The capacitive touch point matters: a GT911 digitiser registers touches near
the glass edge, and a bezel lip that sits over the active area both hides
pixels and creates dead zones. The opening is sized to the glass, not to the
picture.

## Deliberately out of scope

Waterproofing, an internal battery, and a mount design. The mount is
car-specific and belongs in [mounting-notes.md](mounting-notes.md).

## Print settings that matter

- **Matt black filament** for the front. PETG or ASA rather than PLA: a PLA
  case will sag on a sunny dashboard, and this is not a hypothetical.
- **Screw bosses need walls**, not infill. Four perimeters minimum around
  every boss and around the screen opening.
- **Print the front face down** so the visible surface is the smooth build
  plate side and the bezel opening needs no support.

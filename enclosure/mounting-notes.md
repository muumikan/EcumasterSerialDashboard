# Mounting notes

## Where the dimensions come from

Elecrow publishes the panel's mechanical model. It is the authoritative source
— this repository does not restate the numbers, because a transcribed
dimension that is wrong costs a print.

In the
[Elecrow-RD demo repository](https://github.com/Elecrow-RD/CrowPanel-Advance-3.5-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-480x320):

| Path | What it is |
|---|---|
| `3D file/advance-hmi3_5-20260126.stp` | Panel assembly, STEP, ~6 MB |
| `3D file/00-35-_asm.stp` | Full assembly, STEP, ~39 MB |
| `Eagle_SCH&PCB/` | Board files — mounting hole positions |

Open the STEP in any CAD tool (FreeCAD is free and reads it) and measure:

1. Overall outline of the panel assembly, X and Y
2. Depth from the glass face to the tallest component on the back
3. Mounting hole diameter, and their positions from one corner
4. Glass active-area rectangle relative to that same corner
5. USB-C and UART connector positions along their edges

Those five measurements are exactly the parameters at the top of
[`case.scad`](case.scad). Fill them in and the model is complete.

> **The values currently in `case.scad` are placeholders.** They are marked as
> such in the file. Do not print until they have been replaced with measured
> numbers — the geometry is right, the dimensions are not.

The STEP file was inspected while writing this, but its point cloud is in
per-part local coordinates without the assembly transforms applied, so a
bounding box taken from it is meaningless. There is no shortcut around opening
it in CAD or measuring the physical panel with calipers.

## Mounting in the car

Decide the location before finalising the case, because it sets the exit angle
of both cables.

Points worth thinking through:

- **Sight line.** The screen should fall inside the driver's normal scan
  without obscuring the road or the original instruments.
- **Airbag paths and trim.** Nothing rigid in a deployment path.
- **Cable strain relief.** Both cables need a fixed anchor within ~100 mm of
  the case so vibration works against the anchor, not the connector.
- **Serviceability.** The USB-C port has to stay reachable for flashing with
  the case mounted.
- **Removal.** A dashboard-mounted screen is worth stealing. A mount that
  detaches without tools is a feature.

## Record here once decided

- Mount location and fixing method
- Cable routing from the case to the EMU extension port
- Where the MAX3232 sits inside the case, and how it is retained

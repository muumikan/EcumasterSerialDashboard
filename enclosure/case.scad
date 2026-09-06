// Enclosure for the ECU dashboard
// CrowPanel Advance 3.5" HMI + MAX3232, cabin-mounted in a Toyota Carina.
//
// Two printed parts, screwed together:
//   front  - bezel, screen opening, panel standoffs
//   back   - shell, vents, MAX3232 bay, cable exits
//
// Render one part at a time with the `part` variable below, or "both" for a
// preview of the assembly.
//
// ---------------------------------------------------------------------------
//  READ THIS BEFORE PRINTING
//
//  Every value in the MEASURE block is a PLACEHOLDER. The geometry in this
//  file is correct; the numbers are not. Elecrow does not publish a dimension
//  table, and the STEP model's coordinates cannot be reduced to a bounding box
//  without a CAD kernel.
//
//  Take the five measurements listed in mounting-notes.md from
//  3D file/advance-hmi3_5-20260126.stp, or with calipers from the panel
//  itself, and replace them here. Nothing else needs to change.
// ---------------------------------------------------------------------------

part = "both";   // "front" | "back" | "both"

$fn = 48;

// ===========================================================================
//  MEASURE  -  placeholders, replace with real numbers
// ===========================================================================

// Panel assembly outline, looking at the glass.
panel_w      = 100.0;   // PLACEHOLDER  width
panel_h      =  70.0;   // PLACEHOLDER  height
panel_d      =  14.0;   // PLACEHOLDER  glass face to tallest rear component

// Glass active area, measured from the panel's bottom-left corner.
glass_x      =  13.0;   // PLACEHOLDER
glass_y      =  10.0;   // PLACEHOLDER
glass_w      =  74.0;   // PLACEHOLDER  480 px at 3.5" diagonal is ~73 mm
glass_h      =  49.0;   // PLACEHOLDER  320 px                     ~49 mm

// Panel mounting holes: four, inset from each corner.
hole_dia     =   2.6;   // PLACEHOLDER  M2.5 clearance
hole_inset_x =   3.5;   // PLACEHOLDER
hole_inset_y =   3.5;   // PLACEHOLDER

// Connector cut-outs. `pos` is measured along that edge from the
// bottom-left corner; `z` is height above the rear face of the panel.
usb_pos      =  50.0;   // PLACEHOLDER  USB-C, bottom edge
usb_w        =  12.0;   // PLACEHOLDER
usb_h        =   8.0;   // PLACEHOLDER

uart_pos     =  20.0;   // PLACEHOLDER  UART1-OUT connector, right edge
uart_w       =  14.0;   // PLACEHOLDER
uart_h       =   9.0;   // PLACEHOLDER

// MAX3232 breakout, housed inside the back shell.
max_w        =  22.0;   // PLACEHOLDER
max_h        =  16.0;   // PLACEHOLDER
max_d        =   8.0;   // PLACEHOLDER

// ===========================================================================
//  Design parameters  -  safe to tune
// ===========================================================================

wall         =   2.4;   // 3 perimeters at 0.4 mm plus a margin
bezel        =   2.0;   // how far the front face overlaps the glass edge
                        // keep small: a capacitive digitiser reads to the edge
fit          =   0.35;  // clearance around the panel, per side
standoff_h   =   1.6;   // lifts the panel off the front face
corner_r     =   3.0;
screw_dia    =   3.2;   // M3 clearance in the front
insert_dia   =   4.2;   // heat-set insert in the back
tilt         =  12;     // degrees the face leans back toward the driver

vent_w       =   2.0;
vent_gap     =   3.5;

// ---------------------------------------------------------------------------

cav_w = panel_w + 2 * fit;
cav_h = panel_h + 2 * fit;
out_w = cav_w + 2 * wall;
out_h = cav_h + 2 * wall;
front_d = standoff_h + wall;
back_d  = panel_d + max_d + 2 * wall;

module rrect(w, h, d, r) {
    hull()
        for (x = [r, w - r], y = [r, h - r])
            translate([x, y, 0]) cylinder(r = r, h = d);
}

// The four panel screw positions, in panel coordinates.
module at_holes() {
    for (x = [hole_inset_x, panel_w - hole_inset_x],
         y = [hole_inset_y, panel_h - hole_inset_y])
        translate([wall + fit + x, wall + fit + y, 0]) children();
}

// Case screws sit outside the panel, in the corners of the shell.
module at_screws() {
    for (x = [wall / 2 + 1.5, out_w - wall / 2 - 1.5],
         y = [wall / 2 + 1.5, out_h - wall / 2 - 1.5])
        translate([x, y, 0]) children();
}

// ===========================================================================
//  Front
// ===========================================================================

module front() {
    difference() {
        union() {
            rrect(out_w, out_h, front_d, corner_r);
            // standoffs the panel rests on
            at_holes()
                cylinder(d = hole_dia + 3.2, h = front_d + standoff_h);
        }

        // screen opening, inset by the bezel overlap on every side
        translate([wall + fit + glass_x + bezel,
                   wall + fit + glass_y + bezel,
                   -1])
            cube([glass_w - 2 * bezel, glass_h - 2 * bezel, front_d + 4]);

        // panel screws pass through
        at_holes() translate([0, 0, -1])
            cylinder(d = hole_dia, h = front_d + standoff_h + 4);

        // case screws, countersunk from the face
        at_screws() {
            translate([0, 0, -1]) cylinder(d = screw_dia, h = front_d + 4);
            translate([0, 0, -0.01]) cylinder(d1 = screw_dia + 3, d2 = screw_dia, h = 1.8);
        }
    }
}

// ===========================================================================
//  Back
// ===========================================================================

module back() {
    difference() {
        rrect(out_w, out_h, back_d, corner_r);

        // hollow
        translate([wall, wall, -1])
            cube([cav_w, cav_h, back_d - wall + 1]);

        // USB-C, bottom edge - stays reachable for flashing
        translate([wall + fit + usb_pos - usb_w / 2, -1, wall])
            cube([usb_w, wall + 2, usb_h]);

        // ECU cable, right edge
        translate([out_w - wall - 1, wall + fit + uart_pos - uart_w / 2, wall])
            cube([wall + 2, uart_w, uart_h]);

        // vents: back face only, never the top edge
        for (i = [0 : floor((cav_h - 20) / vent_gap)])
            translate([out_w * 0.25, wall + 10 + i * vent_gap, back_d - wall - 1])
                cube([out_w * 0.5, vent_w, wall + 2]);

        // heat-set inserts for the case screws
        at_screws() translate([0, 0, back_d - wall - 6])
            cylinder(d = insert_dia, h = 8);
    }

    // MAX3232 bay: a shelf with two retaining walls, behind the panel
    translate([wall + 3, wall + 3, back_d - wall - max_d]) {
        difference() {
            cube([max_w + 2 * 1.6, max_h + 2 * 1.6, 1.6]);
            translate([1.6, 1.6, -1]) cube([max_w, max_h, 4]);
        }
        cube([1.6, max_h + 2 * 1.6, max_d]);
        translate([max_w + 1.6, 0, 0]) cube([1.6, max_h + 2 * 1.6, max_d]);
    }
}

// ===========================================================================
//  Render
// ===========================================================================

if (part == "front") front();
else if (part == "back") back();
else {
    // preview: front laid flat, back beside it, tilt shown on the front only
    rotate([tilt, 0, 0]) front();
    translate([out_w + 15, 0, 0]) back();
}

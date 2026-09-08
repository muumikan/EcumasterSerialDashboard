#pragma once

#include <lvgl.h>

// The dashboard's fixed palette. The screen is read in a dark cockpit, so
// there is one theme only - these values are the design, not a default.
namespace theme {

// Panels sit darker than the first pass and the dividing lines a little
// brighter. On the car the mid-grey cells washed out the numerals; the reading
// now comes from the digits against near-black, and the tiles are separated by
// their edges rather than by their fill.
inline lv_color_t bg()        { return lv_color_hex(0x07090A); }
inline lv_color_t statusBg()  { return lv_color_hex(0x0B0E0F); }
inline lv_color_t panel()     { return lv_color_hex(0x0D1113); }
inline lv_color_t line()      { return lv_color_hex(0x2C363A); }
inline lv_color_t track()     { return lv_color_hex(0x12171A); }
inline lv_color_t text()      { return lv_color_hex(0xEDF1F2); }
inline lv_color_t dim()       { return lv_color_hex(0x75858B); }
inline lv_color_t dotOff()    { return lv_color_hex(0x303A3E); }

inline lv_color_t good()      { return lv_color_hex(0x57C08A); }
inline lv_color_t warn()      { return lv_color_hex(0xE8A33D); }
inline lv_color_t crit()      { return lv_color_hex(0xE2504A); }
inline lv_color_t cyan()      { return lv_color_hex(0x58C7D6); }

// Tile backgrounds when a value goes out of range.
inline lv_color_t warnBg()    { return lv_color_hex(0x271E0D); }
inline lv_color_t warnEdge()  { return lv_color_hex(0x4A3A17); }
inline lv_color_t warnLabel() { return lv_color_hex(0xC0913F); }
inline lv_color_t critBg()    { return lv_color_hex(0x3E100D); }
inline lv_color_t critEdge()  { return lv_color_hex(0x7A2320); }
inline lv_color_t critLabel() { return lv_color_hex(0xE9A9A5); }
inline lv_color_t critValue() { return lv_color_hex(0xFFECEA); }

// Screen geometry, fixed by the panel.
constexpr lv_coord_t kShiftHeight = 6;
constexpr lv_coord_t kStatusHeight = 28;
constexpr lv_coord_t kChromeHeight = kShiftHeight + kStatusHeight;  // 34
constexpr lv_coord_t kPageWidth = 480;
constexpr lv_coord_t kPageHeight = 286;

}  // namespace theme

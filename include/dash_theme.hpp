#pragma once

#include <lvgl.h>

// The dashboard's fixed palette. The screen is read in a dark cockpit, so
// there is one theme only - these values are the design, not a default.
namespace theme {

inline lv_color_t bg()        { return lv_color_hex(0x0A0C0D); }
inline lv_color_t statusBg()  { return lv_color_hex(0x101415); }
inline lv_color_t panel()     { return lv_color_hex(0x14181A); }
inline lv_color_t line()      { return lv_color_hex(0x262F32); }
inline lv_color_t track()     { return lv_color_hex(0x171D20); }
inline lv_color_t text()      { return lv_color_hex(0xEDF1F2); }
inline lv_color_t dim()       { return lv_color_hex(0x75858B); }
inline lv_color_t dotOff()    { return lv_color_hex(0x303A3E); }

inline lv_color_t good()      { return lv_color_hex(0x57C08A); }
inline lv_color_t warn()      { return lv_color_hex(0xE8A33D); }
inline lv_color_t crit()      { return lv_color_hex(0xE2504A); }
inline lv_color_t cyan()      { return lv_color_hex(0x58C7D6); }

// Tile backgrounds when a value goes out of range.
inline lv_color_t warnBg()    { return lv_color_hex(0x2E2411); }
inline lv_color_t warnEdge()  { return lv_color_hex(0x4A3A17); }
inline lv_color_t warnLabel() { return lv_color_hex(0xC0913F); }
inline lv_color_t critBg()    { return lv_color_hex(0x4A1512); }
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

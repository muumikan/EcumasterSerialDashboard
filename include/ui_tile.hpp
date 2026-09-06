#pragma once

#include <lvgl.h>

#include "alarm_engine.hpp"

namespace ecu {

// One measurement cell: caption, value, unit.
//
// An out-of-range value lights its own cell rather than taking over the
// screen, so the driver keeps seeing everything else.
class Tile {
public:
    void create(lv_obj_t* parent,
                lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                const char* caption, const char* unit,
                const lv_font_t* valueFont);

    void setInt(int value);
    void setFloat(float value, int decimals);
    void setText(const char* text);
    void setSeverity(AlarmSeverity severity);

    lv_obj_t* root() const { return root_; }
    lv_obj_t* value() const { return value_; }

private:
    lv_obj_t* root_ = nullptr;
    lv_obj_t* caption_ = nullptr;
    lv_obj_t* value_ = nullptr;
    lv_obj_t* unit_ = nullptr;
    AlarmSeverity severity_ = AlarmSeverity::None;
};

// Shared helpers for building the flat, borderless containers the dash is
// made of.
lv_obj_t* makePanel(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);
lv_obj_t* makeCaption(lv_obj_t* parent, const char* text);

}  // namespace ecu

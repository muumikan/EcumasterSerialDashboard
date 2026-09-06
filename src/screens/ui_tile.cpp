#include "ui_tile.hpp"

#include <stdio.h>

#include "dash_theme.hpp"

namespace ecu {

lv_obj_t* makePanel(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

lv_obj_t* makeCaption(lv_obj_t* parent, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, theme::dim(), 0);
    return label;
}

void Tile::create(lv_obj_t* parent,
                  lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                  const char* caption, const char* unit,
                  const lv_font_t* valueFont) {
    root_ = lv_obj_create(parent);
    lv_obj_set_pos(root_, x, y);
    lv_obj_set_size(root_, w, h);
    lv_obj_set_style_radius(root_, 0, 0);
    lv_obj_set_style_pad_all(root_, 8, 0);
    lv_obj_set_style_bg_color(root_, theme::panel(), 0);
    lv_obj_set_style_bg_opa(root_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(root_, theme::line(), 0);
    lv_obj_set_style_border_width(root_, 1, 0);
    lv_obj_set_style_border_side(root_, LV_BORDER_SIDE_TOP | LV_BORDER_SIDE_LEFT, 0);
    lv_obj_clear_flag(root_, LV_OBJ_FLAG_SCROLLABLE);

    caption_ = makeCaption(root_, caption);
    lv_obj_align(caption_, LV_ALIGN_TOP_LEFT, 0, 0);

    unit_ = nullptr;
    if (unit != nullptr && unit[0] != '\0') {
        unit_ = makeCaption(root_, unit);
        lv_obj_align(unit_, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    }

    value_ = lv_label_create(root_);
    lv_label_set_text(value_, "--");
    lv_obj_set_style_text_font(value_, valueFont, 0);
    lv_obj_set_style_text_color(value_, theme::text(), 0);
    lv_obj_align(value_, LV_ALIGN_BOTTOM_LEFT, 0, 0);
}

void Tile::setInt(int value) {
    char text[16];
    snprintf(text, sizeof(text), "%d", value);
    lv_label_set_text(value_, text);
}

void Tile::setFloat(float value, int decimals) {
    char text[16];
    snprintf(text, sizeof(text), "%.*f", decimals, value);
    lv_label_set_text(value_, text);
}

void Tile::setText(const char* text) {
    lv_label_set_text(value_, text);
}

void Tile::setSeverity(AlarmSeverity severity) {
    if (severity == severity_) {
        return;  // restyling every frame would thrash LVGL's invalidation
    }
    severity_ = severity;

    lv_color_t bg = theme::panel();
    lv_color_t edge = theme::line();
    lv_color_t label = theme::dim();
    lv_color_t value = theme::text();

    if (severity == AlarmSeverity::Warning) {
        bg = theme::warnBg();
        edge = theme::warnEdge();
        label = theme::warnLabel();
        value = theme::warn();
    } else if (severity == AlarmSeverity::Critical) {
        bg = theme::critBg();
        edge = theme::critEdge();
        label = theme::critLabel();
        value = theme::critValue();
    }

    lv_obj_set_style_bg_color(root_, bg, 0);
    lv_obj_set_style_border_color(root_, edge, 0);
    lv_obj_set_style_text_color(caption_, label, 0);
    lv_obj_set_style_text_color(value_, value, 0);
    if (unit_ != nullptr) {
        lv_obj_set_style_text_color(unit_, label, 0);
    }
}

}  // namespace ecu

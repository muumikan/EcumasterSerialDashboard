#include "main_screen.hpp"

#include <stdio.h>

namespace ecu {
namespace {

constexpr lv_coord_t kTopBarHeight = 34;
constexpr lv_coord_t kRpmBlockHeight = 96;

lv_color_t linkColor(LinkState state) {
    switch (state) {
        case LinkState::Online:  return lv_palette_main(LV_PALETTE_GREEN);
        case LinkState::Stale:   return lv_palette_main(LV_PALETTE_AMBER);
        case LinkState::Offline: return lv_palette_main(LV_PALETTE_RED);
    }
    return lv_palette_main(LV_PALETTE_GREY);
}

// A flat, borderless container - the dashboard is built entirely from these.
lv_obj_t* makePanel(lv_obj_t* parent, lv_coord_t w, lv_coord_t h) {
    lv_obj_t* panel = lv_obj_create(parent);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 0, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

// One measurement: small caption on top, value underneath.
lv_obj_t* makeTile(lv_obj_t* parent, const char* caption, const char* unit) {
    lv_obj_t* tile = makePanel(parent, 116, 90);
    lv_obj_set_style_pad_all(tile, 6, 0);
    lv_obj_set_style_bg_color(tile, lv_color_hex(0x1b1b1b), 0);
    lv_obj_set_style_radius(tile, 6, 0);

    lv_obj_t* caption_label = lv_label_create(tile);
    lv_label_set_text(caption_label, caption);
    lv_obj_set_style_text_color(caption_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(caption_label, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t* unit_label = lv_label_create(tile);
    lv_label_set_text(unit_label, unit);
    lv_obj_set_style_text_color(unit_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(unit_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    lv_obj_t* value_label = lv_label_create(tile);
    lv_label_set_text(value_label, "--");
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_28, 0);
    lv_obj_align(value_label, LV_ALIGN_BOTTOM_LEFT, 0, 0);

    return value_label;
}

void setInt(lv_obj_t* label, int value) {
    char text[16];
    snprintf(text, sizeof(text), "%d", value);
    lv_label_set_text(label, text);
}

void setFloat(lv_obj_t* label, float value, int decimals) {
    char text[16];
    snprintf(text, sizeof(text), "%.*f", decimals, value);
    lv_label_set_text(label, text);
}

}  // namespace

void MainScreen::create() {
    lv_obj_t* screen = lv_scr_act();
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x0c0c0c), 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    // --- top bar ---------------------------------------------------------
    lv_obj_t* topBar = makePanel(screen, LV_PCT(100), kTopBarHeight);
    lv_obj_set_style_bg_color(topBar, lv_color_hex(0x161616), 0);
    lv_obj_set_style_pad_hor(topBar, 10, 0);
    lv_obj_align(topBar, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t* title = lv_label_create(topBar);
    lv_label_set_text(title, "EMU CLASSIC");
    lv_obj_set_style_text_color(title, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 0, 0);

    linkLabel_ = lv_label_create(topBar);
    lv_label_set_text(linkLabel_, "OFFLINE");
    lv_obj_align(linkLabel_, LV_ALIGN_RIGHT_MID, 0, 0);

    // --- rpm -------------------------------------------------------------
    lv_obj_t* rpmBlock = makePanel(screen, LV_PCT(100), kRpmBlockHeight);
    lv_obj_set_style_bg_opa(rpmBlock, LV_OPA_TRANSP, 0);
    lv_obj_align(rpmBlock, LV_ALIGN_TOP_MID, 0, kTopBarHeight);

    rpmLabel_ = lv_label_create(rpmBlock);
    lv_label_set_text(rpmLabel_, "----");
    lv_obj_set_style_text_font(rpmLabel_, &lv_font_montserrat_48, 0);
    lv_obj_align(rpmLabel_, LV_ALIGN_CENTER, 0, -6);

    lv_obj_t* rpmUnit = lv_label_create(rpmBlock);
    lv_label_set_text(rpmUnit, "RPM");
    lv_obj_set_style_text_color(rpmUnit, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(rpmUnit, LV_ALIGN_BOTTOM_MID, 0, 0);

    // --- measurement grid ------------------------------------------------
    lv_obj_t* grid = makePanel(screen, LV_PCT(100), 190);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_align(grid, LV_ALIGN_TOP_MID, 0, kTopBarHeight + kRpmBlockHeight);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(grid, 5, 0);
    lv_obj_set_style_pad_column(grid, 5, 0);

    tileValues_[kMap] = makeTile(grid, "MAP", "kPa");
    tileValues_[kTps] = makeTile(grid, "TPS", "%");
    tileValues_[kClt] = makeTile(grid, "CLT", "C");
    tileValues_[kIat] = makeTile(grid, "IAT", "C");
    tileValues_[kBattery] = makeTile(grid, "BATT", "V");
    tileValues_[kLambda] = makeTile(grid, "LAMBDA", "");
    tileValues_[kOilPressure] = makeTile(grid, "OIL P", "bar");
    tileValues_[kOilTemp] = makeTile(grid, "OIL T", "C");
}

void MainScreen::update(const EngineDataModel& model, uint32_t nowMs) {
    const LinkState state = model.linkState(nowMs);

    if (state != lastState_) {
        lastState_ = state;
        lv_label_set_text(linkLabel_, toString(state));
        lv_obj_set_style_text_color(linkLabel_, linkColor(state), 0);
    }

    if (model.revision() == lastRevision_) {
        return;
    }
    lastRevision_ = model.revision();

    const EngineSnapshot& s = model.snapshot();

    setInt(rpmLabel_, s.rpm);
    setInt(tileValues_[kMap], s.mapKpa);
    setInt(tileValues_[kTps], s.tpsPct);
    setInt(tileValues_[kClt], s.cltC);
    setInt(tileValues_[kIat], s.iatC);
    setFloat(tileValues_[kBattery], s.batteryV, 1);
    setFloat(tileValues_[kLambda], s.wboLambda, 2);
    setFloat(tileValues_[kOilPressure], s.oilPressureBar, 1);
    setInt(tileValues_[kOilTemp], s.oilTempC);
}

}  // namespace ecu

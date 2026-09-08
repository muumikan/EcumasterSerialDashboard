#include "main_screen.hpp"

#include <stdio.h>

#include "dash_theme.hpp"

namespace ecu {
namespace {

constexpr lv_coord_t kHeroHeight = 156;
constexpr lv_coord_t kRpmBoxWidth = 290;
constexpr lv_coord_t kMapBoxWidth = 190;
constexpr lv_coord_t kTileWidth = 120;
constexpr lv_coord_t kTileHeight = 130;

// Boost bar scale: -1.0 bar (full vacuum) to +1.2 bar, so atmospheric sits
// just under half way across.
constexpr lv_coord_t kBarWidth = 162;
constexpr lv_coord_t kBarHeight = 10;
constexpr float kBarMin = -1.0f;
constexpr float kBarMax = 1.2f;

lv_coord_t barX(float bar) {
    if (bar < kBarMin) bar = kBarMin;
    if (bar > kBarMax) bar = kBarMax;
    return static_cast<lv_coord_t>(((bar - kBarMin) / (kBarMax - kBarMin)) * kBarWidth);
}

}  // namespace

void MainScreen::create(lv_obj_t* parent) {
    root_ = makePanel(parent, 0, 0, theme::kPageWidth, theme::kPageHeight);

    // ---- RPM ------------------------------------------------------------
    lv_obj_t* rpmBox = makePanel(root_, 0, 0, kRpmBoxWidth, kHeroHeight);
    lv_obj_set_style_pad_all(rpmBox, 12, 0);

    lv_obj_t* rpmCaption = makeCaption(rpmBox, "RPM");
    lv_obj_align(rpmCaption, LV_ALIGN_TOP_LEFT, 0, 0);

    rpmValue_ = lv_label_create(rpmBox);
    lv_label_set_text(rpmValue_, "0");
    lv_obj_set_style_text_font(rpmValue_, &lv_font_montserrat_48, 0);
    lv_obj_align(rpmValue_, LV_ALIGN_LEFT_MID, 0, 4);

    lv_obj_t* rpmTrack = makePanel(rpmBox, 0, kHeroHeight - 12 - 12 - 7, kRpmBoxWidth - 24, 7);
    lv_obj_set_style_bg_color(rpmTrack, theme::track(), 0);
    lv_obj_set_style_bg_opa(rpmTrack, LV_OPA_COVER, 0);

    rpmBar_ = makePanel(rpmTrack, 0, 0, 0, 7);
    lv_obj_set_style_bg_color(rpmBar_, theme::cyan(), 0);
    lv_obj_set_style_bg_opa(rpmBar_, LV_OPA_COVER, 0);

    // ---- boost ----------------------------------------------------------
    lv_obj_t* mapBox = makePanel(root_, kRpmBoxWidth, 0, kMapBoxWidth, kHeroHeight);
    lv_obj_set_style_pad_all(mapBox, 12, 0);
    lv_obj_set_style_border_color(mapBox, theme::line(), 0);
    lv_obj_set_style_border_width(mapBox, 1, 0);
    lv_obj_set_style_border_side(mapBox, LV_BORDER_SIDE_LEFT, 0);

    lv_obj_t* boostCaption = makeCaption(mapBox, "BOOST");
    lv_obj_align(boostCaption, LV_ALIGN_TOP_LEFT, 0, 0);

    boostValue_ = lv_label_create(mapBox);
    lv_label_set_text(boostValue_, "+0.00");
    lv_obj_set_style_text_font(boostValue_, &lv_font_montserrat_28, 0);
    lv_obj_align(boostValue_, LV_ALIGN_TOP_LEFT, 0, 22);

    mapLabel_ = makeCaption(mapBox, "bar  MAP 0 kPa");
    lv_obj_align(mapLabel_, LV_ALIGN_TOP_LEFT, 0, 58);

    lv_obj_t* boostTrack = makePanel(mapBox, 0, 82, kBarWidth, kBarHeight);
    lv_obj_set_style_bg_color(boostTrack, theme::track(), 0);
    lv_obj_set_style_bg_opa(boostTrack, LV_OPA_COVER, 0);

    boostFill_ = makePanel(boostTrack, barX(0.0f), 0, 0, kBarHeight);
    lv_obj_set_style_bg_color(boostFill_, theme::cyan(), 0);
    lv_obj_set_style_bg_opa(boostFill_, LV_OPA_COVER, 0);

    lv_obj_t* zeroMark = makePanel(boostTrack, barX(0.0f), 0, 1, kBarHeight);
    lv_obj_set_style_bg_color(zeroMark, theme::dim(), 0);
    lv_obj_set_style_bg_opa(zeroMark, LV_OPA_COVER, 0);

    boostPeakMark_ = makePanel(boostTrack, barX(0.0f), 0, 2, kBarHeight);
    lv_obj_set_style_bg_color(boostPeakMark_, theme::warn(), 0);
    lv_obj_set_style_bg_opa(boostPeakMark_, LV_OPA_COVER, 0);

    boostPeakLabel_ = makeCaption(mapBox, "Peak --");
    lv_obj_align(boostPeakLabel_, LV_ALIGN_TOP_LEFT, 0, 102);

    // ---- tiles ----------------------------------------------------------
    const lv_coord_t y = kHeroHeight;
    clt_.create(root_, 0, y, kTileWidth, kTileHeight, "CLT", "C", &lv_font_montserrat_28);
    oil_.create(root_, kTileWidth, y, kTileWidth, kTileHeight, "OIL P", "bar", &lv_font_montserrat_28);
    lambda_.create(root_, kTileWidth * 2, y, kTileWidth, kTileHeight, "LAMBDA", "", &lv_font_montserrat_28);
    battery_.create(root_, kTileWidth * 3, y, kTileWidth, kTileHeight, "BATT", "V", &lv_font_montserrat_28);
}

void MainScreen::update(const EngineDataModel& model,
                        const AlarmEngine& alarms,
                        const RunPeaks& peaks,
                        uint32_t nowMs) {
    (void)peaks;
    (void)nowMs;
    const EngineSnapshot& s = model.snapshot();

    char text[24];

    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(s.rpm));
    lv_label_set_text(rpmValue_, text);

    lv_coord_t rpmWidth = static_cast<lv_coord_t>((s.rpm / 7500.0f) * (kRpmBoxWidth - 24));
    if (rpmWidth < 0) rpmWidth = 0;
    if (rpmWidth > kRpmBoxWidth - 24) rpmWidth = kRpmBoxWidth - 24;
    lv_obj_set_width(rpmBar_, rpmWidth);

    const float boost = (static_cast<float>(s.mapKpa) - 100.0f) / 100.0f;
    snprintf(text, sizeof(text), "%+.2f", boost);
    lv_label_set_text(boostValue_, text);

    snprintf(text, sizeof(text), "bar  MAP %u kPa", static_cast<unsigned>(s.mapKpa));
    lv_label_set_text(mapLabel_, text);

    const lv_coord_t zero = barX(0.0f);
    const lv_coord_t now = barX(boost);
    if (now >= zero) {
        lv_obj_set_pos(boostFill_, zero, 0);
        lv_obj_set_width(boostFill_, now - zero);
    } else {
        lv_obj_set_pos(boostFill_, now, 0);
        lv_obj_set_width(boostFill_, zero - now);
    }
    lv_obj_set_style_bg_color(boostFill_, boost > 1.05f ? theme::warn() : theme::cyan(), 0);

    if (alarms.engineRunning() && boost > boostPeak_) {
        boostPeak_ = boost;
        lv_obj_set_pos(boostPeakMark_, barX(boostPeak_), 0);
        snprintf(text, sizeof(text), "Peak %+.2f bar", boostPeak_);
        lv_label_set_text(boostPeakLabel_, text);
    }

    clt_.setInt(s.cltC);
    oil_.setFloat(s.oilPressureBar, 1);
    lambda_.setFloat(s.wboLambda, 2);
    battery_.setFloat(s.batteryV, 1);

    clt_.setSeverity(alarms.severity(AlarmId::Coolant));
    oil_.setSeverity(alarms.severity(AlarmId::OilPressure));
    lambda_.setSeverity(alarms.severity(AlarmId::Lean));
    // Under- and over-voltage are separate rules; the cell shows the worse.
    const AlarmSeverity low = alarms.severity(AlarmId::BatteryLow);
    const AlarmSeverity high = alarms.severity(AlarmId::BatteryHigh);
    battery_.setSeverity(low > high ? low : high);
}

}  // namespace ecu

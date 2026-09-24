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
    lv_obj_set_style_text_color(rpmCaption, theme::caption(), 0);
    lv_obj_align(rpmCaption, LV_ALIGN_TOP_LEFT, 0, 0);

    rpmValue_ = lv_label_create(rpmBox);
    setLabelText(rpmValue_, "0");
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
    lv_obj_set_style_text_color(boostCaption, theme::caption(), 0);
    lv_obj_align(boostCaption, LV_ALIGN_TOP_LEFT, 0, 0);

    boostValue_ = lv_label_create(mapBox);
    setLabelText(boostValue_, "+0.00");
    lv_obj_set_style_text_font(boostValue_, &lv_font_montserrat_36, 0);
    lv_obj_align(boostValue_, LV_ALIGN_TOP_LEFT, 0, 20);

    mapLabel_ = makeCaption(mapBox, "bar  MAP 0 kPa");
    lv_obj_align(mapLabel_, LV_ALIGN_TOP_LEFT, 0, 64);

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
    clt_.create(root_, 0, y, kTileWidth, kTileHeight, "CLT", "C", &lv_font_montserrat_36);
    oil_.create(root_, kTileWidth, y, kTileWidth, kTileHeight, "OIL P", "bar", &lv_font_montserrat_36);
    lambda_.create(root_, kTileWidth * 2, y, kTileWidth, kTileHeight, "LAMBDA", "", &lv_font_montserrat_36);
    battery_.create(root_, kTileWidth * 3, y, kTileWidth, kTileHeight, "BATT", "V", &lv_font_montserrat_36);
}

void MainScreen::update(const EngineDataModel& model,
                        const AlarmEngine& alarms,
                        const RunPeaks& peaks,
                        uint32_t nowMs) {
    (void)nowMs;
    const EngineSnapshot& s = model.snapshot();

    char text[24];

    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(s.rpm));
    setLabelText(rpmValue_, text);

    // Full scale where the shift lights are all lit, so the bar and the strip
    // above it agree about how close the engine is to the limit.
    lv_coord_t rpmWidth =
        static_cast<lv_coord_t>((s.rpm / static_cast<float>(rpmFullScale_)) * (kRpmBoxWidth - 24));
    if (rpmWidth < 0) rpmWidth = 0;
    if (rpmWidth > kRpmBoxWidth - 24) rpmWidth = kRpmBoxWidth - 24;
    lv_obj_set_width(rpmBar_, rpmWidth);

    const float boost = boostBar(s);
    snprintf(text, sizeof(text), "%+.2f", boost);
    setLabelText(boostValue_, text);

    snprintf(text, sizeof(text), "bar  MAP %u kPa", static_cast<unsigned>(s.mapKpa));
    setLabelText(mapLabel_, text);

    const lv_coord_t zero = barX(0.0f);
    const lv_coord_t now = barX(boost);
    if (now >= zero) {
        lv_obj_set_pos(boostFill_, zero, 0);
        lv_obj_set_width(boostFill_, now - zero);
    } else {
        lv_obj_set_pos(boostFill_, now, 0);
        lv_obj_set_width(boostFill_, zero - now);
    }
    // Coloured by the overboost alarm, so the bar, the number and the status
    // bar all change at the limit set on the setup page - not at a number of
    // this page's own.
    const AlarmSeverity over = alarms.severity(AlarmId::Overboost);
    lv_color_t fill = theme::cyan();
    lv_color_t reading = theme::text();
    if (over == AlarmSeverity::Warning) {
        fill = reading = theme::warn();
    } else if (over == AlarmSeverity::Critical) {
        fill = reading = theme::crit();
    }
    lv_obj_set_style_bg_color(boostFill_, fill, 0);
    lv_obj_set_style_text_color(boostValue_, reading, 0);

    // From the run peaks, which see every frame, rather than from what this
    // page happened to be painted with: a peak between two repaints is still
    // the peak.
    const float peak = peaks.seeded ? boostBar(peaks.mapKpa, peaks.baroKpa) : boostPeak_;
    if (peak > boostPeak_) {
        boostPeak_ = peak;
        lv_obj_set_pos(boostPeakMark_, barX(boostPeak_), 0);
        snprintf(text, sizeof(text), "Peak %+.2f bar", boostPeak_);
        setLabelText(boostPeakLabel_, text);
    }

    clt_.setInt(s.cltC);
    oil_.setFloat(s.oilPressureBar, 1);
    showMixture(lambda_, s, mixtureAfr_);
    battery_.setFloat(s.batteryV, 1);

    clt_.setSeverity(alarms.severity(AlarmId::Coolant));
    oil_.setSeverity(alarms.severity(AlarmId::OilPressure));
    lambda_.setSeverity(alarms.severity(AlarmId::Lean));
    // Under- and over-voltage are separate rules; the cell shows the worse.
    const AlarmSeverity low = alarms.severity(AlarmId::BatteryLow);
    const AlarmSeverity high = alarms.severity(AlarmId::BatteryHigh);
    battery_.setSeverity(low > high ? low : high);

    // Last, so a failed sensor overrides whatever its reading would have said.
    if (celSet(s.celFlags, kCelBitClt)) clt_.showFault();
    if (celSet(s.celFlags, kCelBitWbo)) lambda_.showFault();
    if (celSet(s.celFlags, kCelBitMap)) {
        setLabelText(boostValue_, "ERR");
        lv_obj_set_style_text_color(boostValue_, theme::crit(), 0);
    }
}

bool MainScreen::showNoData() {
    setLabelText(rpmValue_, "--");
    lv_obj_set_width(rpmBar_, 0);
    setLabelText(boostValue_, "--");
    lv_obj_set_style_text_color(boostValue_, theme::text(), 0);
    setLabelText(mapLabel_, "bar  MAP -- kPa");
    lv_obj_set_width(boostFill_, 0);
    // The peak mark and its label stay: they are this run's record, not a
    // live reading, and they are still true.

    clt_.clear();
    oil_.clear();
    lambda_.clear();
    battery_.clear();
    return true;
}

void MainScreen::applySettings(const DashSettings& settings) {
    mixtureAfr_ = settings.mixtureAfr;
    lambda_.setCaption(mixtureName(mixtureAfr_));
    rpmFullScale_ = settings.shiftAllRpm >= 1000 ? settings.shiftAllRpm : 7500;
}

}  // namespace ecu

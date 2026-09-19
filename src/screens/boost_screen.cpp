#include "boost_screen.hpp"

#include <stdio.h>

#include "dash_theme.hpp"

namespace ecu {
namespace {

constexpr lv_coord_t kCol = 160;
constexpr lv_coord_t kRow = 95;

// Absolute kPa, with the target as a mark on the same scale - no deviation
// arithmetic. 220 kPa is about 1.2 bar of gauge boost at sea level, which
// covers this car and keeps atmospheric pressure just under halfway, so off
// boost is a glanceable position rather than a number to read.
constexpr lv_coord_t kBarWidth = 288;
constexpr lv_coord_t kBarHeight = 7;
constexpr float kBarFullScaleKpa = 220.0f;

lv_coord_t barX(float kpa) {
    float fraction = kpa / kBarFullScaleKpa;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    return static_cast<lv_coord_t>(fraction * kBarWidth);
}

}  // namespace

void BoostScreen::create(lv_obj_t* parent) {
    root_ = makePanel(parent, 0, 0, theme::kPageWidth, theme::kPageHeight);

    // ---- manifold pressure against target, two columns wide -------------
    map_.create(root_, 0, 0, kCol * 2, kRow, "MAP / BOOST TGT", "kPa", &lv_font_montserrat_36);
    lv_obj_align(map_.value(), LV_ALIGN_TOP_LEFT, 0, 20);

    targetLabel_ = makeCaption(map_.root(), "tgt --");
    // Clear of the reading beside it at its widest: "7500" is 90 px in
    // Montserrat 36, and 96 left six of them.
    lv_obj_align(targetLabel_, LV_ALIGN_TOP_LEFT, 104, 30);

    lv_obj_t* track = makePanel(map_.root(), 0, kRow - 8 - 8 - kBarHeight, kBarWidth, kBarHeight);
    lv_obj_set_style_bg_color(track, theme::track(), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);

    mapFill_ = makePanel(track, 0, 0, 0, kBarHeight);
    lv_obj_set_style_bg_color(mapFill_, theme::good(), 0);
    lv_obj_set_style_bg_opa(mapFill_, LV_OPA_COVER, 0);

    // After the fill, so it survives the overlap that matters most.
    targetMark_ = makePanel(track, 0, 0, 2, kBarHeight);
    lv_obj_set_style_bg_color(targetMark_, theme::cyan(), 0);
    lv_obj_set_style_bg_opa(targetMark_, LV_OPA_COVER, 0);
    lv_obj_add_flag(targetMark_, LV_OBJ_FLAG_HIDDEN);

    tableSet_.create(root_, kCol * 2, 0, kCol, kRow, "BOOST SET", "", &lv_font_montserrat_36);

    duty_.create(root_, 0, kRow, kCol, kRow, "BOOST DC", "%", &lv_font_montserrat_36);
    pidCorrection_.create(root_, kCol, kRow, kCol, kRow, "BOOST PID", "%", &lv_font_montserrat_36);
    errorCorrection_.create(root_, kCol * 2, kRow, kCol, kRow, "ERR COR", "%", &lv_font_montserrat_36);

    rpm_.create(root_, 0, kRow * 2, kCol, kRow, "RPM", "rpm", &lv_font_montserrat_36);
    tps_.create(root_, kCol, kRow * 2, kCol, kRow, "TPS", "%", &lv_font_montserrat_36);
    afr_.create(root_, kCol * 2, kRow * 2, kCol, kRow, "AFR", "", &lv_font_montserrat_36);
}

void BoostScreen::update(const EngineDataModel& model,
                         const AlarmEngine& alarms,
                         const RunPeaks& peaks,
                         uint32_t nowMs) {
    (void)alarms;
    (void)peaks;
    (void)nowMs;
    const EngineSnapshot& s = model.snapshot();

    char text[24];

    map_.setInt(s.mapKpa);
    rpm_.setInt(s.rpm);
    tps_.setInt(s.tpsPct);
    afr_.setFloat(s.wboAfr, 1);

    lv_obj_set_width(mapFill_, barX(static_cast<float>(s.mapKpa)));

    if (!s.controlChannels) {
        lv_label_set_text(targetLabel_, "tgt n/a");
        lv_obj_add_flag(targetMark_, LV_OBJ_FLAG_HIDDEN);
        tableSet_.setText("n/a");
        duty_.setText("--");
        pidCorrection_.setText("--");
        errorCorrection_.setText("--");
        return;
    }

    // Zero means boost control is not set up, not a target of zero.
    if (s.boostTargetKpa > 0) {
        snprintf(text, sizeof(text), "tgt %u", s.boostTargetKpa);
        lv_obj_set_pos(targetMark_, barX(static_cast<float>(s.boostTargetKpa)), 0);
        lv_obj_clear_flag(targetMark_, LV_OBJ_FLAG_HIDDEN);
    } else {
        snprintf(text, sizeof(text), "tgt --");
        lv_obj_add_flag(targetMark_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(targetLabel_, text);

    // Raw, the way the ECU numbers its sets. Renumbering it from one here
    // would only disagree with the Client on the laptop next to the car.
    tableSet_.setInt(s.boostTableSet);

    duty_.setInt(s.boostDutyPct);

    snprintf(text, sizeof(text), "%+d", s.boostPidCorrPct);
    pidCorrection_.setText(text);

    snprintf(text, sizeof(text), "%+d", s.boostDcErrCorPct);
    errorCorrection_.setText(text);
}

}  // namespace ecu

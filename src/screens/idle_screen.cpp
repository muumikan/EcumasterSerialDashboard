#include "idle_screen.hpp"

#include <stdio.h>

#include "dash_theme.hpp"

namespace ecu {
namespace {

constexpr lv_coord_t kCol = 160;
constexpr lv_coord_t kRow = 95;

// The rpm bar reads absolutely, 0 to full scale, with the target as a mark on
// it. 2000 rpm covers every idle this ECU will ask for - a cold start target
// is around 1500 - and leaves the needle in the lower half where the eye can
// still separate 850 from 900. Driving pegs it, which is the honest thing for
// a bar whose scale stops at 2000.
constexpr lv_coord_t kBarWidth = 288;
constexpr lv_coord_t kBarHeight = 7;
constexpr float kBarFullScaleRpm = 2000.0f;

lv_coord_t barX(float rpm) {
    float fraction = rpm / kBarFullScaleRpm;
    if (fraction < 0.0f) fraction = 0.0f;
    if (fraction > 1.0f) fraction = 1.0f;
    return static_cast<lv_coord_t>(fraction * kBarWidth);
}

}  // namespace

void IdleScreen::create(lv_obj_t* parent) {
    root_ = makePanel(parent, 0, 0, theme::kPageWidth, theme::kPageHeight);

    // ---- rpm against target, two columns wide ---------------------------
    rpm_.create(root_, 0, 0, kCol * 2, kRow, "RPM / IDLE TGT", "", &lv_font_montserrat_28);
    lv_obj_align(rpm_.value(), LV_ALIGN_TOP_LEFT, 0, 20);

    targetLabel_ = makeCaption(rpm_.root(), "tgt --");
    lv_obj_align(targetLabel_, LV_ALIGN_TOP_LEFT, 96, 30);

    lv_obj_t* track = makePanel(rpm_.root(), 0, kRow - 8 - 8 - kBarHeight, kBarWidth, kBarHeight);
    lv_obj_set_style_bg_color(track, theme::track(), 0);
    lv_obj_set_style_bg_opa(track, LV_OPA_COVER, 0);

    rpmFill_ = makePanel(track, 0, 0, 0, kBarHeight);
    lv_obj_set_style_bg_color(rpmFill_, theme::good(), 0);
    lv_obj_set_style_bg_opa(rpmFill_, LV_OPA_COVER, 0);

    // Drawn after the fill so it stays visible where the two overlap, which is
    // exactly when the loop is holding target and you most want to see both.
    targetMark_ = makePanel(track, 0, 0, 2, kBarHeight);
    lv_obj_set_style_bg_color(targetMark_, theme::cyan(), 0);
    lv_obj_set_style_bg_opa(targetMark_, LV_OPA_COVER, 0);
    lv_obj_add_flag(targetMark_, LV_OBJ_FLAG_HIDDEN);

    control_.create(root_, kCol * 2, 0, kCol, kRow, "IDLE CTL", "", &lv_font_montserrat_20);

    duty_.create(root_, 0, kRow, kCol, kRow, "IDLE DC", "%", &lv_font_montserrat_28);
    pidCorrection_.create(root_, kCol, kRow, kCol, kRow, "IDLE PID", "%", &lv_font_montserrat_28);
    afr_.create(root_, kCol * 2, kRow, kCol, kRow, "AFR", "", &lv_font_montserrat_28);

    ignition_.create(root_, 0, kRow * 2, kCol, kRow, "IGN", "BTDC", &lv_font_montserrat_28);
    angleCorrection_.create(root_, kCol, kRow * 2, kCol, kRow, "IDLE IGN", "deg", &lv_font_montserrat_28);
    map_.create(root_, kCol * 2, kRow * 2, kCol, kRow, "MAP", "kPa", &lv_font_montserrat_28);
}

void IdleScreen::update(const EngineDataModel& model,
                        const AlarmEngine& alarms,
                        const RunPeaks& peaks,
                        uint32_t nowMs) {
    (void)alarms;
    (void)peaks;
    (void)nowMs;
    const EngineSnapshot& s = model.snapshot();

    char text[24];

    // rpm and mixture come from every protocol; the loop's own state does not.
    rpm_.setInt(s.rpm);
    afr_.setFloat(s.wboAfr, 1);
    ignition_.setFloat(s.ignAngleDeg, 1);
    map_.setInt(s.mapKpa);

    lv_obj_set_width(rpmFill_, barX(static_cast<float>(s.rpm)));

    if (!s.controlChannels) {
        // Classic protocol: none of these channels exist. Showing the zeros
        // would read as a closed valve and a dead PID, which is a lie a
        // dash should not tell.
        lv_label_set_text(targetLabel_, "tgt n/a");
        lv_obj_add_flag(targetMark_, LV_OBJ_FLAG_HIDDEN);
        control_.setText("n/a");
        duty_.setText("--");
        pidCorrection_.setText("--");
        angleCorrection_.setText("--");
        return;
    }

    // Zero is what the ECU sends when idle control is not set up at all, so
    // there is no target to mark and nothing to measure against.
    if (s.idleTargetRpm > 0) {
        snprintf(text, sizeof(text), "tgt %u", s.idleTargetRpm);
        lv_obj_set_pos(targetMark_, barX(static_cast<float>(s.idleTargetRpm)), 0);
        lv_obj_clear_flag(targetMark_, LV_OBJ_FLAG_HIDDEN);
    } else {
        snprintf(text, sizeof(text), "tgt --");
        lv_obj_add_flag(targetMark_, LV_OBJ_FLAG_HIDDEN);
    }
    lv_label_set_text(targetLabel_, text);

    control_.setText(s.idleControlActive ? "CLOSED" : "OPEN");

    duty_.setInt(s.idleDutyPct);

    // Signed, and the sign is the whole point: it says which way the loop is
    // pushing the valve right now.
    snprintf(text, sizeof(text), "%+d", s.idlePidCorrPct);
    pidCorrection_.setText(text);

    snprintf(text, sizeof(text), "%+.1f", s.idleAngleCorrDeg);
    angleCorrection_.setText(text);
}

}  // namespace ecu

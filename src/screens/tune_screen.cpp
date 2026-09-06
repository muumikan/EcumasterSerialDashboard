#include "tune_screen.hpp"

#include <stdio.h>

#include "dash_theme.hpp"

namespace ecu {
namespace {

constexpr lv_coord_t kCol = 160;
constexpr lv_coord_t kRow = 95;

// Deviation bar: lambda minus target, +/- 0.12 across the full width.
constexpr lv_coord_t kDevWidth = 288;
constexpr lv_coord_t kDevHeight = 7;
constexpr float kDevSpan = 0.12f;

}  // namespace

void TuneScreen::create(lv_obj_t* parent) {
    root_ = makePanel(parent, 0, 0, theme::kPageWidth, theme::kPageHeight);

    // ---- lambda vs target, two columns wide -----------------------------
    lambda_.create(root_, 0, 0, kCol * 2, kRow, "LAMBDA / TARGET", "", &lv_font_montserrat_28);
    lv_obj_align(lambda_.value(), LV_ALIGN_TOP_LEFT, 0, 20);

    targetLabel_ = makeCaption(lambda_.root(), "tgt --");
    lv_obj_align(targetLabel_, LV_ALIGN_TOP_LEFT, 96, 30);

    lv_obj_t* devTrack = makePanel(lambda_.root(), 0, kRow - 8 - 8 - kDevHeight, kDevWidth, kDevHeight);
    lv_obj_set_style_bg_color(devTrack, theme::track(), 0);
    lv_obj_set_style_bg_opa(devTrack, LV_OPA_COVER, 0);

    deviationFill_ = makePanel(devTrack, kDevWidth / 2, 0, 0, kDevHeight);
    lv_obj_set_style_bg_color(deviationFill_, theme::good(), 0);
    lv_obj_set_style_bg_opa(deviationFill_, LV_OPA_COVER, 0);

    lv_obj_t* mid = makePanel(devTrack, kDevWidth / 2, 0, 1, kDevHeight);
    lv_obj_set_style_bg_color(mid, theme::dim(), 0);
    lv_obj_set_style_bg_opa(mid, LV_OPA_COVER, 0);

    knock_.create(root_, kCol * 2, 0, kCol, kRow, "KNOCK", "V", &lv_font_montserrat_28);

    ignition_.create(root_, 0, kRow, kCol, kRow, "IGN", "BTDC", &lv_font_montserrat_28);
    pulseWidth_.create(root_, kCol, kRow, kCol, kRow, "INJ PW", "ms", &lv_font_montserrat_28);
    dutyCycle_.create(root_, kCol * 2, kRow, kCol, kRow, "INJ DC", "%", &lv_font_montserrat_28);

    map_.create(root_, 0, kRow * 2, kCol, kRow, "MAP", "kPa", &lv_font_montserrat_28);
    tps_.create(root_, kCol, kRow * 2, kCol, kRow, "TPS", "%", &lv_font_montserrat_28);
    rpm_.create(root_, kCol * 2, kRow * 2, kCol, kRow, "RPM", "rpm", &lv_font_montserrat_28);
}

void TuneScreen::update(const EngineDataModel& model,
                        const AlarmEngine& alarms,
                        const RunPeaks& peaks,
                        uint32_t nowMs) {
    (void)peaks;
    (void)nowMs;
    const EngineSnapshot& s = model.snapshot();

    char text[24];

    lambda_.setFloat(s.wboLambda, 2);
    snprintf(text, sizeof(text), "tgt %.2f", s.lambdaTarget);
    lv_label_set_text(targetLabel_, text);

    float deviation = (s.wboLambda - s.lambdaTarget) / kDevSpan;
    if (deviation < -1.0f) deviation = -1.0f;
    if (deviation > 1.0f) deviation = 1.0f;

    const lv_coord_t half = kDevWidth / 2;
    const lv_coord_t offset = static_cast<lv_coord_t>(deviation * half);
    if (offset >= 0) {
        lv_obj_set_pos(deviationFill_, half, 0);
        lv_obj_set_width(deviationFill_, offset);
    } else {
        lv_obj_set_pos(deviationFill_, half + offset, 0);
        lv_obj_set_width(deviationFill_, -offset);
    }
    const bool wide = deviation > 0.6f || deviation < -0.6f;
    lv_obj_set_style_bg_color(deviationFill_, wide ? theme::warn() : theme::good(), 0);

    knock_.setFloat(s.knockLevelV, 1);
    ignition_.setFloat(s.ignAngleDeg, 1);
    pulseWidth_.setFloat(s.injPulseWidthMs, 1);
    dutyCycle_.setInt(static_cast<int>(s.injDutyPct));
    map_.setInt(s.mapKpa);
    tps_.setInt(s.tpsPct);
    rpm_.setInt(s.rpm);

    lambda_.setSeverity(alarms.severity(AlarmId::Lean));
    knock_.setSeverity(alarms.severity(AlarmId::Knock));
    dutyCycle_.setSeverity(alarms.severity(AlarmId::InjectorDuty));
}

}  // namespace ecu

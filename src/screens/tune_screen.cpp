#include "tune_screen.hpp"

#include <stdio.h>

#include "dash_theme.hpp"

namespace ecu {
namespace {

constexpr lv_coord_t kCol = 160;
constexpr lv_coord_t kRow = 95;

// Deviation bar: AFR minus target, +/- 1.8 across the full width. That is the
// same mixture error the bar showed when this tile read lambda - 0.12 lambda
// is about 1.8 AFR on petrol - so the needle means what it always did.
constexpr lv_coord_t kDevWidth = 288;
constexpr lv_coord_t kDevHeight = 7;
constexpr float kDevSpan = 1.8f;

// Below this the ECU is not sending a target at all, and a deviation from zero
// would peg the bar for the whole drive. Seen on this car: the 1.211 stream
// carried afrTarget as 0.0 for an entire log.
constexpr float kTargetPresent = 5.0f;

}  // namespace

void TuneScreen::create(lv_obj_t* parent) {
    root_ = makePanel(parent, 0, 0, theme::kPageWidth, theme::kPageHeight);

    // ---- AFR vs target, two columns wide --------------------------------
    afr_.create(root_, 0, 0, kCol * 2, kRow, "AFR / TARGET", "", &lv_font_montserrat_36);
    lv_obj_align(afr_.value(), LV_ALIGN_TOP_LEFT, 0, 20);

    targetLabel_ = makeCaption(afr_.root(), "tgt --");
    // Clear of the reading beside it at its widest: "7500" is 90 px in
    // Montserrat 36, and 96 left six of them.
    lv_obj_align(targetLabel_, LV_ALIGN_TOP_LEFT, 104, 30);

    lv_obj_t* devTrack = makePanel(afr_.root(), 0, kRow - 8 - 8 - kDevHeight, kDevWidth, kDevHeight);
    lv_obj_set_style_bg_color(devTrack, theme::track(), 0);
    lv_obj_set_style_bg_opa(devTrack, LV_OPA_COVER, 0);

    deviationFill_ = makePanel(devTrack, kDevWidth / 2, 0, 0, kDevHeight);
    lv_obj_set_style_bg_color(deviationFill_, theme::good(), 0);
    lv_obj_set_style_bg_opa(deviationFill_, LV_OPA_COVER, 0);

    lv_obj_t* mid = makePanel(devTrack, kDevWidth / 2, 0, 1, kDevHeight);
    lv_obj_set_style_bg_color(mid, theme::dim(), 0);
    lv_obj_set_style_bg_opa(mid, LV_OPA_COVER, 0);

    // Knock over the ECU's noise threshold; 1.00 is where the ECU counts it.
    knock_.create(root_, kCol * 2, 0, kCol, kRow, "KNOCK", "x noise", &lv_font_montserrat_36);

    ignition_.create(root_, 0, kRow, kCol, kRow, "IGN", "BTDC", &lv_font_montserrat_36);
    // The closed-loop fuel trim, in place of injector pulse width: the duty
    // cell beside it already says how hard the injectors are working, while
    // this is the one number that says whether the fuel map is right.
    egoCorrection_.create(root_, kCol, kRow, kCol, kRow, "EGO CORR", "%", &lv_font_montserrat_36);
    dutyCycle_.create(root_, kCol * 2, kRow, kCol, kRow, "INJ DC", "%", &lv_font_montserrat_36);

    map_.create(root_, 0, kRow * 2, kCol, kRow, "MAP", "kPa", &lv_font_montserrat_36);
    tps_.create(root_, kCol, kRow * 2, kCol, kRow, "TPS", "%", &lv_font_montserrat_36);
    rpm_.create(root_, kCol * 2, kRow * 2, kCol, kRow, "RPM", "rpm", &lv_font_montserrat_36);
}

void TuneScreen::update(const EngineDataModel& model,
                        const AlarmEngine& alarms,
                        const RunPeaks& peaks,
                        uint32_t nowMs) {
    (void)peaks;
    (void)nowMs;
    const EngineSnapshot& s = model.snapshot();

    char text[24];

    showMixture(afr_, s, mixtureAfr_);

    // The target comes from afrTarget in either unit. The stream's own
    // lambdaTarget reads zero in every log from this car; afrTarget does not.
    const bool haveTarget = s.afrTarget >= kTargetPresent;
    if (haveTarget && mixtureAfr_) {
        snprintf(text, sizeof(text), "tgt %.1f", s.afrTarget);
    } else if (haveTarget) {
        snprintf(text, sizeof(text), "tgt %.2f", s.afrTarget / kStoichAfr);
    } else {
        snprintf(text, sizeof(text), "tgt --");
    }
    setLabelText(targetLabel_, text);

    // No target, no deviation. Measuring against zero would hold the bar hard
    // over for the whole drive, which reads as a fault that is not there.
    float deviation = haveTarget ? (s.wboAfr - s.afrTarget) / kDevSpan : 0.0f;
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

    if (s.engineNoiseV >= kMinEngineNoiseV) {
        knock_.setFloat(knockRatio(s), 2);
    } else {
        knock_.setText("--");   // no threshold sent, nothing to measure against
    }
    ignition_.setFloat(s.ignAngleDeg, 1);
    // Shown as the trim itself rather than the ECU's 100-based figure: -6
    // reads as "taking fuel out" without doing the subtraction. The logs from
    // this car run 90 to 106.5 on the ECU's scale.
    // Zero on the ECU's scale is not a trim anyone runs; it means the field
    // is not being filled, which "-100" would misreport.
    if (s.egoCorrectionPct >= 50.0f) {
        snprintf(text, sizeof(text), "%+.1f", s.egoCorrectionPct - 100.0f);
        egoCorrection_.setText(text);
    } else {
        egoCorrection_.setText("--");
    }
    dutyCycle_.setInt(static_cast<int>(s.injDutyPct));
    map_.setInt(s.mapKpa);
    tps_.setInt(s.tpsPct);
    rpm_.setInt(s.rpm);

    // The Lean alarm watches lambda whichever unit this tile is showing; it is
    // the same mixture either way.
    afr_.setSeverity(alarms.severity(AlarmId::Lean));
    knock_.setSeverity(alarms.severity(AlarmId::Knock));
    dutyCycle_.setSeverity(alarms.severity(AlarmId::InjectorDuty));

    if (celSet(s.celFlags, kCelBitWbo)) afr_.showFault();
    if (celSet(s.celFlags, kCelBitMap)) map_.showFault();
}

bool TuneScreen::showNoData() {
    afr_.clear();
    setLabelText(targetLabel_, "tgt --");
    lv_obj_set_width(deviationFill_, 0);

    knock_.clear();
    ignition_.clear();
    egoCorrection_.clear();
    dutyCycle_.clear();
    map_.clear();
    tps_.clear();
    rpm_.clear();
    return true;
}

void TuneScreen::applySettings(const DashSettings& settings) {
    mixtureAfr_ = settings.mixtureAfr;
    afr_.setCaption(mixtureAfr_ ? "AFR / TARGET" : "LAMBDA / TARGET");
}

}  // namespace ecu

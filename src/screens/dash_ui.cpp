#include "dash_ui.hpp"

#include <Arduino.h>
#include <stdio.h>
#include <string.h>

#include "dash_theme.hpp"
#include "display.hpp"
#include "ui_tile.hpp"

namespace ecu {
namespace {

void gestureCb(lv_event_t* event) {
    DashUi* ui = static_cast<DashUi*>(lv_event_get_user_data(event));
    const lv_dir_t direction = lv_indev_get_gesture_dir(lv_indev_get_act());

    if (direction == LV_DIR_LEFT) {
        ui->nextPage();
    } else if (direction == LV_DIR_RIGHT) {
        ui->previousPage();
    } else {
        return;
    }
    lv_indev_wait_release(lv_indev_get_act());
}

void summaryTapCb(lv_event_t* event) {
    static_cast<DashUi*>(lv_event_get_user_data(event))->dismissSummary();
}

void settingsChangedCb(void* context) {
    static_cast<DashUi*>(context)->settingsChanged();
}

}  // namespace

void DashUi::begin(lv_obj_t* screen) {
    if (!store_.load(settings_)) {
        Serial.println(F("settings: no stored record, using defaults"));
    }

    lv_obj_set_style_bg_color(screen, theme::bg(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_text_color(screen, theme::text(), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    buildChrome(screen);

    setup_.bind(&settings_, settingsChangedCb, this);

    pages_[0] = &drive_;
    pages_[1] = &tune_;
    pages_[2] = &temps_;
    pages_[3] = &diagnostics_;
    pages_[4] = &setup_;

    rtc_.begin();

    for (uint8_t i = 0; i < kPageCount; ++i) {
        pages_[i]->create(pageArea_);
        lv_obj_add_flag(pages_[i]->root(), LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_add_event_cb(screen, gestureCb, LV_EVENT_GESTURE, this);

    applySettings();

    // The driving page is what the car powers up into.
    showPage(0);
    if (settings_.bootSweep) {
        bootSweepEndMs_ = lv_tick_get() + kBootSweepMs;
    }
}

void DashUi::applySettings() {
    alarms_.settings() = settings_.alarms;
    display::setBrightness(settings_.nightMode ? settings_.nightBrightnessPct
                                               : settings_.brightnessPct);
    litSegments_ = 0xFF;  // shift points may have moved; force a repaint
}

void DashUi::settingsChanged() {
    applySettings();
    settingsDirty_ = true;
    saveDueMs_ = lv_tick_get() + kSettingsSaveDelayMs;
}

void DashUi::buildChrome(lv_obj_t* screen) {
    // ---- shift lights ---------------------------------------------------
    for (uint8_t i = 0; i < kShiftSegments; ++i) {
        const lv_coord_t x0 = static_cast<lv_coord_t>((i * theme::kPageWidth) / kShiftSegments);
        const lv_coord_t x1 = static_cast<lv_coord_t>(((i + 1) * theme::kPageWidth) / kShiftSegments);

        lv_obj_t* segment = makePanel(screen, x0, 0, x1 - x0 - 2, theme::kShiftHeight);
        lv_obj_set_style_bg_color(segment, theme::track(), 0);
        lv_obj_set_style_bg_opa(segment, LV_OPA_COVER, 0);
        shift_[i] = segment;
    }

    // ---- status bar -----------------------------------------------------
    lv_obj_t* status = makePanel(screen, 0, theme::kShiftHeight, theme::kPageWidth, theme::kStatusHeight);
    lv_obj_set_style_bg_color(status, theme::statusBg(), 0);
    lv_obj_set_style_bg_opa(status, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(status, theme::line(), 0);
    lv_obj_set_style_border_width(status, 1, 0);
    lv_obj_set_style_border_side(status, LV_BORDER_SIDE_BOTTOM, 0);

    for (uint8_t i = 0; i < kPageCount; ++i) {
        lv_obj_t* dot = makePanel(status, 10 + i * 10, (theme::kStatusHeight - 5) / 2, 5, 5);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(dot, theme::dotOff(), 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        dots_[i] = dot;
    }

    pageName_ = lv_label_create(status);
    lv_label_set_text(pageName_, "DRIVE");
    lv_obj_align(pageName_, LV_ALIGN_LEFT_MID, 58, 0);

    alarmText_ = lv_label_create(status);
    lv_label_set_text(alarmText_, "");
    lv_obj_set_style_text_color(alarmText_, theme::dim(), 0);
    lv_obj_align(alarmText_, LV_ALIGN_CENTER, 0, 0);

    latchBadge_ = lv_label_create(status);
    lv_label_set_text(latchBadge_, "");
    lv_obj_set_style_text_color(latchBadge_, theme::crit(), 0);
    lv_obj_align(latchBadge_, LV_ALIGN_RIGHT_MID, -132, 0);

    linkText_ = lv_label_create(status);
    lv_label_set_text(linkText_, "OFFLINE");
    lv_obj_set_style_text_color(linkText_, theme::crit(), 0);
    lv_obj_align(linkText_, LV_ALIGN_RIGHT_MID, -66, 0);

    // The car's own instrument cluster has no clock, so this is the only one
    // the driver gets. Minutes only: seconds would force a repaint every
    // second for something nobody reads to that precision.
    clockText_ = lv_label_create(status);
    lv_label_set_text(clockText_, "--:--");
    lv_obj_set_style_text_color(clockText_, theme::dim(), 0);
    lv_obj_align(clockText_, LV_ALIGN_RIGHT_MID, -10, 0);

    // ---- page area ------------------------------------------------------
    pageArea_ = makePanel(screen, 0, theme::kChromeHeight, theme::kPageWidth, theme::kPageHeight);

    buildSummary(screen);
}

// Shown once when the engine stops. The peaks were always there, on a page the
// driver had to remember to swipe to; this is the moment they are wanted.
void DashUi::buildSummary(lv_obj_t* screen) {
    summary_ = lv_obj_create(screen);
    lv_obj_set_pos(summary_, 0, theme::kChromeHeight);
    lv_obj_set_size(summary_, theme::kPageWidth, theme::kPageHeight);
    lv_obj_set_style_radius(summary_, 0, 0);
    lv_obj_set_style_border_width(summary_, 0, 0);
    lv_obj_set_style_pad_all(summary_, 16, 0);
    lv_obj_set_style_bg_color(summary_, theme::bg(), 0);
    lv_obj_set_style_bg_opa(summary_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(summary_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(summary_, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t* title = makeCaption(summary_, "RUN COMPLETE");
    lv_obj_set_style_text_color(title, theme::cyan(), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 34);

    summaryHead_ = lv_label_create(summary_);
    lv_label_set_text(summaryHead_, "");
    lv_obj_set_style_text_font(summaryHead_, &lv_font_montserrat_28, 0);
    lv_obj_align(summaryHead_, LV_ALIGN_TOP_MID, 0, 58);

    summaryBody_ = lv_label_create(summary_);
    lv_label_set_text(summaryBody_, "");
    lv_obj_set_style_text_color(summaryBody_, theme::dim(), 0);
    lv_obj_align(summaryBody_, LV_ALIGN_TOP_MID, 0, 104);

    lv_obj_t* note = makeCaption(summary_, "tap to dismiss");
    lv_obj_set_style_text_color(note, theme::dotOff(), 0);
    lv_obj_align(note, LV_ALIGN_BOTTOM_MID, 0, -20);

    lv_obj_add_event_cb(summary_, summaryTapCb, LV_EVENT_CLICKED, this);
}

void DashUi::dismissSummary() {
    lv_obj_add_flag(summary_, LV_OBJ_FLAG_HIDDEN);
}

void DashUi::updateSummary(LinkState link) {
    // Waiting for RPM to fall waits for ever. Killing the ignition cuts the
    // ECU's power too, so the last frame ever sent freezes at whatever the
    // engine was doing - often several hundred rpm - and the model keeps
    // reporting it. A dead link ends the run just as surely as an idle that
    // comes to a stop, so either one closes it out.
    const bool running = alarms_.engineRunning() && link != LinkState::Offline;

    if (running) {
        engineWasRunning_ = true;
        lv_obj_add_flag(summary_, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (!engineWasRunning_ || !peaks_.seeded) {
        return;  // never ran, or nothing recorded
    }
    engineWasRunning_ = false;

    char text[48];
    const float boost = (static_cast<float>(peaks_.mapKpa) - 100.0f) / 100.0f;
    snprintf(text, sizeof(text), "%u rpm   %+.2f bar",
             static_cast<unsigned>(peaks_.rpm), boost);
    lv_label_set_text(summaryHead_, text);

    const uint8_t latched = alarms_.latchedCount();
    if (latched == 0) {
        snprintf(text, sizeof(text), "Oil P min %.1f bar   CLT max %d C   no alarms",
                 peaks_.oilPressureMinBar, static_cast<int>(peaks_.cltC));
    } else {
        snprintf(text, sizeof(text), "Oil P min %.1f bar   CLT max %d C   %u latched",
                 peaks_.oilPressureMinBar, static_cast<int>(peaks_.cltC),
                 static_cast<unsigned>(latched));
    }
    lv_label_set_text(summaryBody_, text);
    lv_obj_set_style_text_color(summaryBody_, latched ? theme::warn() : theme::dim(), 0);

    lv_obj_clear_flag(summary_, LV_OBJ_FLAG_HIDDEN);
}

void DashUi::showPage(uint8_t index) {
    if (index >= kPageCount) {
        return;
    }

    lv_obj_add_flag(pages_[page_]->root(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(dots_[page_], theme::dotOff(), 0);

    page_ = index;

    lv_obj_clear_flag(pages_[page_]->root(), LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(dots_[page_], theme::cyan(), 0);
    lv_label_set_text(pageName_, pages_[page_]->name());

    lastRevision_ = UINT32_MAX;  // force the new page to fill in immediately
}

void DashUi::setShiftSegments(uint8_t lit) {
    // Where the strip turns red follows the configured red zone rather than a
    // fixed number of segments, so moving the red line moves the colours too.
    const uint16_t span =
        settings_.shiftAllRpm > settings_.shiftFirstRpm
            ? static_cast<uint16_t>(settings_.shiftAllRpm - settings_.shiftFirstRpm)
            : 1;
    int redFrom = ((settings_.shiftRedRpm - settings_.shiftFirstRpm) * kShiftSegments) / span;
    if (redFrom < 1) redFrom = 1;
    if (redFrom > kShiftSegments) redFrom = kShiftSegments;

    for (uint8_t i = 0; i < kShiftSegments; ++i) {
        lv_color_t color = theme::track();
        if (i < lit) {
            if (i >= redFrom) {
                color = theme::crit();
            } else if (i >= redFrom - 4) {
                color = theme::warn();
            } else {
                color = theme::good();
            }
        }
        lv_obj_set_style_bg_color(shift_[i], color, 0);
    }
}

// Power-up sweep: fill left to right, hold everything red, then clear. It is
// the one moment the driver can confirm every segment works, and it costs
// nothing but the time the panel spends waiting for the first ECU frame.
bool DashUi::runBootSweep(uint32_t nowMs) {
    if (bootSweepEndMs_ == 0) {
        return false;
    }

    const int32_t remaining = static_cast<int32_t>(bootSweepEndMs_ - nowMs);
    if (remaining <= 0) {
        bootSweepEndMs_ = 0;
        litSegments_ = 0xFF;  // force the next real update to repaint
        setShiftSegments(0);
        return false;
    }

    const uint32_t elapsed = kBootSweepMs - static_cast<uint32_t>(remaining);

    if (elapsed < kBootSweepMs / 2) {
        // fill
        const uint8_t lit = static_cast<uint8_t>((elapsed * kShiftSegments) / (kBootSweepMs / 2)) + 1;
        setShiftSegments(lit > kShiftSegments ? kShiftSegments : lit);
    } else if (elapsed < (kBootSweepMs * 3) / 4) {
        // hold every segment red
        for (uint8_t i = 0; i < kShiftSegments; ++i) {
            lv_obj_set_style_bg_color(shift_[i], theme::crit(), 0);
        }
    } else {
        setShiftSegments(0);
    }
    return true;
}

void DashUi::updateShiftLights(uint16_t rpm) {
    int lit = 0;
    if (rpm > settings_.shiftFirstRpm && settings_.shiftAllRpm > settings_.shiftFirstRpm) {
        lit = ((rpm - settings_.shiftFirstRpm) * kShiftSegments) /
              (settings_.shiftAllRpm - settings_.shiftFirstRpm);
        if (lit > kShiftSegments) lit = kShiftSegments;
    }
    if (static_cast<uint8_t>(lit) == litSegments_) {
        return;
    }
    litSegments_ = static_cast<uint8_t>(lit);
    setShiftSegments(static_cast<uint8_t>(lit));
}

void DashUi::updateStatusBar(const EngineDataModel& model, uint32_t nowMs) {
    const LinkState link = model.linkState(nowMs);
    if (link != lastLink_) {
        lastLink_ = link;
        lv_label_set_text(linkText_, toString(link));

        lv_color_t color = theme::good();
        if (link == LinkState::Stale) color = theme::warn();
        if (link == LinkState::Offline) color = theme::crit();
        lv_obj_set_style_text_color(linkText_, color, 0);
    }

    const AlarmSeverity worst = alarms_.worst();
    lv_color_t alarmColor = theme::dim();
    if (worst == AlarmSeverity::Warning) alarmColor = theme::warn();
    if (worst == AlarmSeverity::Critical) alarmColor = theme::crit();

    if (worst != lastWorst_) {
        lastWorst_ = worst;
        lv_obj_set_style_text_color(alarmText_, alarmColor, 0);
    }

    // Arming is shown, not hidden: the driver should be able to see that the
    // dash is deliberately quiet for a few seconds rather than asleep.
    char text[24];
    const char* line = "ENGINE OFF";
    if (alarms_.engineRunning()) {
        if (alarms_.armed()) {
            line = alarms_.worstText();
        } else {
            snprintf(text, sizeof(text), "ARMING %us",
                     static_cast<unsigned>(alarms_.armingRemainingS()));
            line = text;
        }
    }
    lv_label_set_text(alarmText_, line);
    lv_obj_align(alarmText_, LV_ALIGN_CENTER, 0, 0);

    const uint8_t latched = alarms_.latchedCount();
    if (latched != lastLatched_) {
        lastLatched_ = latched;
        if (latched == 0) {
            lv_label_set_text(latchBadge_, "");
        } else {
            char badge[8];
            snprintf(badge, sizeof(badge), "! %u", static_cast<unsigned>(latched));
            lv_label_set_text(latchBadge_, badge);
        }
    }

    char clock[sizeof(clockShown_)];
    formatHm(rtc_.now(), clock, sizeof(clock));
    if (strcmp(clock, clockShown_) != 0) {
        memcpy(clockShown_, clock, sizeof(clockShown_));
        lv_label_set_text(clockText_, clockShown_);
        lv_obj_set_style_text_color(
            clockText_, rtc_.now().valid ? theme::text() : theme::dim(), 0);
    }
}

void DashUi::update(const EngineDataModel& model, uint32_t nowMs) {
    const bool sweeping = runBootSweep(nowMs);
    const bool changed = model.revision() != lastRevision_;
    const bool clockRead = rtc_.loop(nowMs);

    if (changed) {
        lastRevision_ = model.revision();
        alarms_.evaluate(model.snapshot(), nowMs);
        peaks_.record(model.snapshot());
        if (!sweeping) {
            updateShiftLights(model.snapshot().rpm);
        }
    }

    // Edits are written once the driver stops adjusting, not per button press.
    if (settingsDirty_ && static_cast<int32_t>(nowMs - saveDueMs_) >= 0) {
        settingsDirty_ = false;
        store_.save(settings_);
        Serial.println(F("settings: saved"));
    }

    // Checked every pass, not only when data arrives: the case this exists for
    // is the one where data has stopped arriving.
    const LinkState link = model.linkState(nowMs);
    updateSummary(link);

    // The clock read is in the condition so the status bar keeps ticking even
    // when the ECU has gone quiet.
    if (changed || lastLink_ != link || clockRead) {
        updateStatusBar(model, nowMs);
        pages_[page_]->update(model, alarms_, peaks_, nowMs);
    }
}

}  // namespace ecu

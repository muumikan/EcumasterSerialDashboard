#include "dash_ui.hpp"

#include <stdio.h>

#include "dash_theme.hpp"
#include "ui_tile.hpp"

namespace ecu {
namespace {

// Shift lights: first segment at this rpm, all lit (and the last three red)
// by the time the engine is at the limiter.
constexpr uint16_t kShiftFirstRpm = 3500;
constexpr uint16_t kShiftLastRpm = 7200;

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

void pressedCb(lv_event_t* event) {
    static_cast<DashUi*>(lv_event_get_user_data(event))->noteInteraction(lv_tick_get());
}

}  // namespace

void DashUi::begin(lv_obj_t* screen) {
    lv_obj_set_style_bg_color(screen, theme::bg(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    lv_obj_set_style_text_color(screen, theme::text(), 0);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);

    buildChrome(screen);

    pages_[0] = &drive_;
    pages_[1] = &tune_;
    pages_[2] = &temps_;
    pages_[3] = &diagnostics_;

    for (uint8_t i = 0; i < kPageCount; ++i) {
        pages_[i]->create(pageArea_);
        lv_obj_add_flag(pages_[i]->root(), LV_OBJ_FLAG_HIDDEN);
    }

    lv_obj_add_event_cb(screen, gestureCb, LV_EVENT_GESTURE, this);
    lv_obj_add_event_cb(screen, pressedCb, LV_EVENT_PRESSED, this);

    // The driving page is what the car powers up into.
    showPage(0);
    bootSweepEndMs_ = lv_tick_get() + kBootSweepMs;
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
    lv_obj_align(latchBadge_, LV_ALIGN_RIGHT_MID, -74, 0);

    linkText_ = lv_label_create(status);
    lv_label_set_text(linkText_, "OFFLINE");
    lv_obj_set_style_text_color(linkText_, theme::crit(), 0);
    lv_obj_align(linkText_, LV_ALIGN_RIGHT_MID, -10, 0);

    // ---- page area ------------------------------------------------------
    pageArea_ = makePanel(screen, 0, theme::kChromeHeight, theme::kPageWidth, theme::kPageHeight);
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
    lastInteractionMs_ = lv_tick_get();
}

void DashUi::setShiftSegments(uint8_t lit) {
    for (uint8_t i = 0; i < kShiftSegments; ++i) {
        lv_color_t color = theme::track();
        if (i < lit) {
            if (i >= kShiftSegments - 3) {
                color = theme::crit();
            } else if (i >= kShiftSegments - 7) {
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
    if (rpm > kShiftFirstRpm) {
        lit = ((rpm - kShiftFirstRpm) * kShiftSegments) / (kShiftLastRpm - kShiftFirstRpm);
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
}

void DashUi::update(const EngineDataModel& model, uint32_t nowMs) {
    const bool sweeping = runBootSweep(nowMs);
    const bool changed = model.revision() != lastRevision_;

    if (changed) {
        lastRevision_ = model.revision();
        alarms_.evaluate(model.snapshot(), nowMs);
        peaks_.record(model.snapshot());
        if (!sweeping) {
            updateShiftLights(model.snapshot().rpm);
        }
    }

    // Idle timeout: never leave a non-driving page up on the move.
    if (page_ != 0 && (nowMs - lastInteractionMs_) > kIdleReturnMs) {
        showPage(0);
    }

    if (changed || lastLink_ != model.linkState(nowMs)) {
        updateStatusBar(model, nowMs);
        pages_[page_]->update(model, alarms_, peaks_, nowMs);
    }
}

}  // namespace ecu

#include "diagnostic_screen.hpp"

#include <stdio.h>

#include "dash_theme.hpp"
#include "service_status.hpp"
#include "ui_tile.hpp"

namespace ecu {
namespace {

constexpr lv_coord_t kColWidth = 240;
constexpr lv_coord_t kRowHeight = 17;

// Names are yellow and values are white, the same way round as every
// measurement cell. This page is nothing but names and values, so it is the
// page where getting that wrong cost the most.
lv_obj_t* makeHeading(lv_obj_t* parent, lv_coord_t y, const char* text) {
    lv_obj_t* label = makeCaption(parent, text);
    lv_obj_set_style_text_color(label, theme::caption(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, y);
    return label;
}

// One "label ......... value" line. Returns the value label so it can be
// updated later.
lv_obj_t* makeRow(lv_obj_t* parent, lv_coord_t y, const char* caption) {
    lv_obj_t* left = makeCaption(parent, caption);
    lv_obj_set_style_text_color(left, theme::caption(), 0);
    lv_obj_align(left, LV_ALIGN_TOP_LEFT, 0, y);

    lv_obj_t* right = lv_label_create(parent);
    setLabelText(right, "--");
    lv_obj_set_style_text_color(right, theme::text(), 0);
    lv_obj_align(right, LV_ALIGN_TOP_RIGHT, 0, y);
    return right;
}

const char* kPeakCaptions[11] = {
    "RPM max", "MAP max", "CLT max", "IAT max",
    "Oil P max", "Oil P min",
    "Fuel P max", "Fuel P min",
    "Lambda min", "Knock max", "Inj DC max",
};

}  // namespace

void DiagnosticScreen::create(lv_obj_t* parent) {
    root_ = makePanel(parent, 0, 0, theme::kPageWidth, theme::kPageHeight);

    // ---- serial link ----------------------------------------------------
    lv_obj_t* left = makePanel(root_, 0, 0, kColWidth, theme::kPageHeight);
    lv_obj_set_style_pad_all(left, 10, 0);

    makeHeading(left, 0, "SERIAL LINK");
    state_ = makeRow(left, 18, "State");
    age_ = makeRow(left, 18 + kRowHeight, "Age");
    updates_ = makeRow(left, 18 + kRowHeight * 2, "Updates");
    badFrames_ = makeRow(left, 18 + kRowHeight * 3, "Bad frames");

    // Where the latched alarms were: the Alarms page keeps those, with times,
    // and does it properly. A trigger that loses sync is the other fault
    // nothing else on the dash can show, and one this car has logged - an
    // unexpected missing tooth on 10 September.
    makeHeading(left, 94, "TRIGGER");
    sync_ = makeRow(left, 112, "Sync");
    // The count and the last error share a row: a third line ran into the
    // CEL heading below.
    triggerErrors_ = makeRow(left, 112 + kRowHeight, "Errors");

    makeHeading(left, 162, "CEL FLAGS");
    cel_ = makeRow(left, 180, "Raw");

    for (int i = 0; i < 11; ++i) {
        lv_obj_t* bit = lv_label_create(left);
        setLabelText(bit, celBitName(static_cast<uint8_t>(i)));
        lv_obj_set_style_text_color(bit, theme::dotOff(), 0);
        lv_obj_align(bit, LV_ALIGN_TOP_LEFT, (i % 3) * 72, 200 + (i / 3) * 16);
        celBits_[i] = bit;
    }

    // ---- peaks ----------------------------------------------------------
    lv_obj_t* right = makePanel(root_, kColWidth, 0, kColWidth, theme::kPageHeight);
    lv_obj_set_style_pad_all(right, 10, 0);
    lv_obj_set_style_border_color(right, theme::line(), 0);
    lv_obj_set_style_border_width(right, 1, 0);
    lv_obj_set_style_border_side(right, LV_BORDER_SIDE_LEFT, 0);

    makeHeading(right, 0, "PEAKS THIS RUN");
    for (int i = 0; i < 11; ++i) {
        peakValues_[i] = makeRow(right, 18 + kRowHeight * i, kPeakCaptions[i]);
    }

    // ---- service access point -------------------------------------------
    // Under the peaks, in the space they leave: the last peak row ends around
    // y=204 and the padded column ends at 266, which is a heading and two
    // rows. The address is the row that lost the argument for the third - it
    // is always 192.168.4.1, while the network and the key are the two things
    // that have to be read off the screen while standing beside the car.
    makeHeading(right, 208, "SERVICE AP");
    apNetwork_ = makeRow(right, 226, "Network");
    apKey_ = makeRow(right, 243, "Key");
}

void DiagnosticScreen::setServiceState(const ServiceApStatus& status) {
    if (apNetwork_ == nullptr) {
        return;
    }
    if (status.serving == apServing_ && status.clients == apClients_) {
        return;
    }
    apServing_ = status.serving;
    apClients_ = status.clients;

    if (!status.serving) {
        setLabelText(apNetwork_, "off");
        lv_obj_set_style_text_color(apNetwork_, theme::dim(), 0);
        setLabelText(apKey_, status.password);
        lv_obj_set_style_text_color(apKey_, theme::dim(), 0);
        return;
    }

    // The client count rides along with the network name rather than taking a
    // row of its own - there is no third row here. Green once a laptop has
    // actually joined, which is the question being asked of this page.
    char text[28];
    if (status.clients > 0) {
        snprintf(text, sizeof(text), "%s (%u)", status.ssid, status.clients);
    } else {
        snprintf(text, sizeof(text), "%s", status.ssid);
    }
    setLabelText(apNetwork_, text);
    lv_obj_set_style_text_color(apNetwork_, theme::good(), 0);

    setLabelText(apKey_, status.password);
    lv_obj_set_style_text_color(apKey_, theme::text(), 0);
}

void DiagnosticScreen::update(const EngineDataModel& model,
                              const AlarmEngine& alarms,
                              const RunPeaks& peaks,
                              uint32_t nowMs) {
    (void)alarms;   // the Alarms page is where alarms are read now
    const EngineSnapshot& s = model.snapshot();
    char text[40];

    const LinkState link = model.linkState(nowMs);
    setLabelText(state_, toString(link));
    lv_color_t linkColor = theme::good();
    if (link == LinkState::Stale) linkColor = theme::warn();
    if (link == LinkState::Offline) linkColor = theme::crit();
    lv_obj_set_style_text_color(state_, linkColor, 0);

    const uint32_t age = model.ageMs(nowMs);
    if (age == UINT32_MAX) {
        setLabelText(age_, "never");
    } else {
        snprintf(text, sizeof(text), "%lu ms", static_cast<unsigned long>(age));
        setLabelText(age_, text);
    }

    snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(model.updateCount()));
    setLabelText(updates_, text);

    const uint32_t bad = model.badFrames();
    snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(bad));
    setLabelText(badFrames_, text);
    lv_obj_set_style_text_color(badFrames_, bad > 0 ? theme::warn() : theme::text(), 0);

    snprintf(text, sizeof(text), "0x%04X", static_cast<unsigned>(s.celFlags));
    setLabelText(cel_, text);
    lv_obj_set_style_text_color(cel_, s.celFlags != 0 ? theme::crit() : theme::text(), 0);

    for (int i = 0; i < 11; ++i) {
        const bool set = (s.celFlags >> i) & 0x1;
        lv_obj_set_style_text_color(celBits_[i], set ? theme::crit() : theme::dotOff(), 0);
    }

    static const char* const kSync[] = {"NO SYNC", "SYNCING", "SYNCED"};
    setLabelText(sync_, s.ignSyncStatus < 3 ? kSync[s.ignSyncStatus] : "?");
    lv_color_t syncColor = theme::good();
    if (s.ignSyncStatus != 2) {
        // No sync with the engine turning is a fault; with it stopped it is
        // simply what a stationary crank looks like.
        syncColor = s.rpm >= kEngineRunningRpm ? theme::crit() : theme::dim();
    }
    lv_obj_set_style_text_color(sync_, syncColor, 0);

    if (peaks.triggerErrors == 0) {
        setLabelText(triggerErrors_, "0");
    } else {
        uint8_t bit = 0;
        while (bit < 8 && !((peaks.lastTriggerError >> bit) & 0x1)) {
            ++bit;
        }
        snprintf(text, sizeof(text), "%u  %s", static_cast<unsigned>(peaks.triggerErrors),
                 ignErrorBitName(bit));
        setLabelText(triggerErrors_, text);
    }
    lv_obj_set_style_text_color(triggerErrors_,
                                peaks.triggerErrors > 0 ? theme::warn() : theme::text(), 0);

    if (!peaks.seeded) {
        for (int i = 0; i < 11; ++i) {
            setLabelText(peakValues_[i], "--");
        }
        return;
    }

    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(peaks.rpm));
    setLabelText(peakValues_[0], text);
    snprintf(text, sizeof(text), "%u kPa", static_cast<unsigned>(peaks.mapKpa));
    setLabelText(peakValues_[1], text);
    snprintf(text, sizeof(text), "%d C", static_cast<int>(peaks.cltC));
    setLabelText(peakValues_[2], text);
    snprintf(text, sizeof(text), "%d C", static_cast<int>(peaks.iatC));
    setLabelText(peakValues_[3], text);
    snprintf(text, sizeof(text), "%.1f bar", peaks.oilPressureMaxBar);
    setLabelText(peakValues_[4], text);
    snprintf(text, sizeof(text), "%.1f bar", peaks.oilPressureMinBar);
    setLabelText(peakValues_[5], text);
    snprintf(text, sizeof(text), "%.1f bar", peaks.fuelPressureMaxBar);
    setLabelText(peakValues_[6], text);
    snprintf(text, sizeof(text), "%.1f bar", peaks.fuelPressureMinBar);
    setLabelText(peakValues_[7], text);
    snprintf(text, sizeof(text), "%.2f", peaks.lambdaMin);
    setLabelText(peakValues_[8], text);
    snprintf(text, sizeof(text), "%.2f x", peaks.knockRatio);
    setLabelText(peakValues_[9], text);
    snprintf(text, sizeof(text), "%d %%", static_cast<int>(peaks.injDutyPct));
    setLabelText(peakValues_[10], text);
}

}  // namespace ecu

#include "diagnostic_screen.hpp"

#include <stdio.h>

#include "dash_theme.hpp"
#include "ui_tile.hpp"

namespace ecu {
namespace {

constexpr lv_coord_t kColWidth = 240;
constexpr lv_coord_t kRowHeight = 17;

lv_obj_t* makeHeading(lv_obj_t* parent, lv_coord_t y, const char* text) {
    lv_obj_t* label = makeCaption(parent, text);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 0, y);
    return label;
}

// One "label ......... value" line. Returns the value label so it can be
// updated later.
lv_obj_t* makeRow(lv_obj_t* parent, lv_coord_t y, const char* caption) {
    lv_obj_t* left = makeCaption(parent, caption);
    lv_obj_align(left, LV_ALIGN_TOP_LEFT, 0, y);

    lv_obj_t* right = lv_label_create(parent);
    lv_label_set_text(right, "--");
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
    revision_ = makeRow(left, 18 + kRowHeight * 3, "Revision");

    makeHeading(left, 18 + kRowHeight * 4 + 8, "CEL FLAGS");
    cel_ = makeRow(left, 18 + kRowHeight * 5 + 8, "Raw");

    // Bit positions only - the EMU firmware's bit-to-fault mapping is not in
    // the reference implementation, so nothing here claims to name them.
    for (int i = 0; i < 16; ++i) {
        lv_obj_t* bit = lv_label_create(left);
        char text[4];
        snprintf(text, sizeof(text), "%d", i);
        lv_label_set_text(bit, text);
        lv_obj_set_style_text_color(bit, theme::dotOff(), 0);
        lv_obj_align(bit, LV_ALIGN_TOP_LEFT, (i % 8) * 27, 18 + kRowHeight * 6 + 12 + (i / 8) * 18);
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
}

void DiagnosticScreen::update(const EngineDataModel& model,
                              const AlarmEngine& alarms,
                              const RunPeaks& peaks,
                              uint32_t nowMs) {
    (void)alarms;
    const EngineSnapshot& s = model.snapshot();
    char text[24];

    const LinkState link = model.linkState(nowMs);
    lv_label_set_text(state_, toString(link));
    lv_color_t linkColor = theme::good();
    if (link == LinkState::Stale) linkColor = theme::warn();
    if (link == LinkState::Offline) linkColor = theme::crit();
    lv_obj_set_style_text_color(state_, linkColor, 0);

    const uint32_t age = model.ageMs(nowMs);
    if (age == UINT32_MAX) {
        lv_label_set_text(age_, "never");
    } else {
        snprintf(text, sizeof(text), "%lu ms", static_cast<unsigned long>(age));
        lv_label_set_text(age_, text);
    }

    snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(model.updateCount()));
    lv_label_set_text(updates_, text);

    snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(model.revision()));
    lv_label_set_text(revision_, text);

    snprintf(text, sizeof(text), "0x%04X", static_cast<unsigned>(s.celFlags));
    lv_label_set_text(cel_, text);
    lv_obj_set_style_text_color(cel_, s.celFlags != 0 ? theme::crit() : theme::text(), 0);

    for (int i = 0; i < 16; ++i) {
        const bool set = (s.celFlags >> i) & 0x1;
        lv_obj_set_style_text_color(celBits_[i], set ? theme::crit() : theme::dotOff(), 0);
    }

    if (!peaks.seeded) {
        for (int i = 0; i < 11; ++i) {
            lv_label_set_text(peakValues_[i], "--");
        }
        return;
    }

    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(peaks.rpm));
    lv_label_set_text(peakValues_[0], text);
    snprintf(text, sizeof(text), "%u kPa", static_cast<unsigned>(peaks.mapKpa));
    lv_label_set_text(peakValues_[1], text);
    snprintf(text, sizeof(text), "%d C", static_cast<int>(peaks.cltC));
    lv_label_set_text(peakValues_[2], text);
    snprintf(text, sizeof(text), "%d C", static_cast<int>(peaks.iatC));
    lv_label_set_text(peakValues_[3], text);
    snprintf(text, sizeof(text), "%.1f bar", peaks.oilPressureMaxBar);
    lv_label_set_text(peakValues_[4], text);
    snprintf(text, sizeof(text), "%.1f bar", peaks.oilPressureMinBar);
    lv_label_set_text(peakValues_[5], text);
    snprintf(text, sizeof(text), "%.1f bar", peaks.fuelPressureMaxBar);
    lv_label_set_text(peakValues_[6], text);
    snprintf(text, sizeof(text), "%.1f bar", peaks.fuelPressureMinBar);
    lv_label_set_text(peakValues_[7], text);
    snprintf(text, sizeof(text), "%.2f", peaks.lambdaMin);
    lv_label_set_text(peakValues_[8], text);
    snprintf(text, sizeof(text), "%.1f V", peaks.knockLevelV);
    lv_label_set_text(peakValues_[9], text);
    snprintf(text, sizeof(text), "%d %%", static_cast<int>(peaks.injDutyPct));
    lv_label_set_text(peakValues_[10], text);
}

}  // namespace ecu

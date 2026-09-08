#include "alarms_screen.hpp"

#include <stdio.h>

#include "alarm_engine.hpp"
#include "dash_theme.hpp"
#include "ui_tile.hpp"

namespace ecu {
namespace {

constexpr lv_coord_t kHeaderHeight = 22;
constexpr lv_coord_t kRowHeight = 22;
constexpr lv_coord_t kFooterHeight = 22;
constexpr lv_coord_t kTextTop = 4;

// Column geometry. Measured against Montserrat 14, which is the only size the
// list uses: the point of this page is fitting eleven events on a 480 px
// screen, not making any one of them large.
constexpr lv_coord_t kTimeX = 12;
constexpr lv_coord_t kLabelX = 90;
constexpr lv_coord_t kValueX = 172;
constexpr lv_coord_t kRpmX = 240;
constexpr lv_coord_t kDurationX = 316;
constexpr lv_coord_t kStateX = 408;
constexpr lv_coord_t kNumberWidth = 60;

constexpr lv_coord_t kStripeWidth = 4;

// An active event's duration is still running, so the list has to repaint even
// when nothing new has happened. Four times a second is faster than anyone
// reads and slow enough to stay out of the way of the ECU stream.
constexpr uint32_t kTickMs = 250;

lv_obj_t* makeCell(lv_obj_t* parent, lv_coord_t x, const char* text) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_color(label, theme::text(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, x, kTextTop);
    return label;
}

lv_obj_t* makeNumberCell(lv_obj_t* parent, lv_coord_t x, const char* text) {
    lv_obj_t* label = makeCell(parent, x, text);
    lv_obj_set_width(label, kNumberWidth);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_RIGHT, 0);
    return label;
}

lv_color_t severityColor(AlarmSeverity severity) {
    return severity == AlarmSeverity::Critical ? theme::crit() : theme::warn();
}

// "0.6 s" up close, "44 s" for most of a run, minutes once seconds stop
// meaning anything.
void formatDuration(char* out, size_t size, uint32_t ms) {
    if (ms < 10000) {
        snprintf(out, size, "%.1f s", ms / 1000.0f);
    } else if (ms < 100000) {
        snprintf(out, size, "%u s", static_cast<unsigned>(ms / 1000));
    } else {
        snprintf(out, size, "%u m", static_cast<unsigned>(ms / 60000));
    }
}

}  // namespace

void AlarmsScreen::create(lv_obj_t* parent) {
    root_ = makePanel(parent, 0, 0, theme::kPageWidth, theme::kPageHeight);
    buildHeader();
    buildRows();
    buildFooter();
}

void AlarmsScreen::buildHeader() {
    lv_obj_t* header = makePanel(root_, 0, 0, theme::kPageWidth, kHeaderHeight);
    lv_obj_set_style_bg_color(header, theme::panel(), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(header, theme::line(), 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);

    struct Column {
        lv_coord_t x;
        const char* caption;
        bool number;
    };
    const Column columns[] = {
        {kTimeX, "TIME", false},
        {kLabelX, "SOURCE", false},
        {kValueX, "VALUE", false},
        {kRpmX, "RPM", true},
        {kDurationX, "FOR", true},
        {kStateX, "STATE", false},
    };

    for (const Column& column : columns) {
        lv_obj_t* caption = column.number
                                ? makeNumberCell(header, column.x, column.caption)
                                : makeCell(header, column.x, column.caption);
        lv_obj_set_style_text_color(caption, theme::dim(), 0);
    }
}

void AlarmsScreen::buildRows() {
    for (uint8_t i = 0; i < kVisibleRows; ++i) {
        Row& row = rows_[i];
        const lv_coord_t y = kHeaderHeight + i * kRowHeight;

        row.root = makePanel(root_, 0, y, theme::kPageWidth, kRowHeight);
        lv_obj_set_style_border_color(row.root, theme::track(), 0);
        lv_obj_set_style_border_width(row.root, 1, 0);
        lv_obj_set_style_border_side(row.root, LV_BORDER_SIDE_BOTTOM, 0);

        row.stripe = makePanel(row.root, 0, 0, kStripeWidth, kRowHeight);
        lv_obj_set_style_bg_opa(row.stripe, LV_OPA_COVER, 0);

        row.time = makeCell(row.root, kTimeX, "");
        row.label = makeCell(row.root, kLabelX, "");
        row.value = makeCell(row.root, kValueX, "");
        row.rpm = makeNumberCell(row.root, kRpmX, "");
        row.duration = makeNumberCell(row.root, kDurationX, "");
        row.state = makeCell(row.root, kStateX, "");

        lv_obj_add_flag(row.root, LV_OBJ_FLAG_HIDDEN);
    }

    empty_ = lv_label_create(root_);
    lv_label_set_text(empty_, "no events this run");
    lv_obj_set_style_text_color(empty_, theme::dotOff(), 0);
    lv_obj_align(empty_, LV_ALIGN_TOP_LEFT, kTimeX, kHeaderHeight + kTextTop);
}

void AlarmsScreen::buildFooter() {
    const lv_coord_t y = theme::kPageHeight - kFooterHeight;

    lv_obj_t* footer = makePanel(root_, 0, y, theme::kPageWidth, kFooterHeight);
    lv_obj_set_style_bg_color(footer, theme::panel(), 0);
    lv_obj_set_style_bg_opa(footer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(footer, theme::line(), 0);
    lv_obj_set_style_border_width(footer, 1, 0);
    lv_obj_set_style_border_side(footer, LV_BORDER_SIDE_TOP, 0);

    lv_obj_set_style_text_color(makeCell(footer, 12, "ACTIVE"), theme::dim(), 0);
    activeCount_ = makeCell(footer, 66, "0");

    lv_obj_set_style_text_color(makeCell(footer, 92, "TOTAL"), theme::dim(), 0);
    totalCount_ = makeCell(footer, 143, "0");

    lv_obj_set_style_text_color(makeCell(footer, 168, "RUN"), theme::dim(), 0);
    runTime_ = makeCell(footer, 208, "0:00");

    since_ = lv_label_create(footer);
    lv_label_set_text(since_, "");
    lv_obj_set_style_text_color(since_, theme::dotOff(), 0);
    lv_obj_align(since_, LV_ALIGN_TOP_RIGHT, -12, kTextTop);
}

void AlarmsScreen::update(const EngineDataModel& model,
                          const AlarmEngine& alarms,
                          const RunPeaks& peaks,
                          uint32_t nowMs) {
    (void)model;
    (void)peaks;

    const uint32_t revision = alarms.events().revision();
    const bool ticked = static_cast<int32_t>(nowMs - lastPaintMs_) >= static_cast<int32_t>(kTickMs);
    if (revision == lastRevision_ && !ticked) {
        return;
    }
    lastRevision_ = revision;
    lastPaintMs_ = nowMs;

    paint(alarms, nowMs);
}

void AlarmsScreen::paint(const AlarmEngine& alarms, uint32_t nowMs) {
    const EventLog& log = alarms.events();
    const uint8_t shown = log.count() < kVisibleRows ? log.count() : kVisibleRows;

    if (log.count() == 0) {
        lv_obj_clear_flag(empty_, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(empty_, LV_OBJ_FLAG_HIDDEN);
    }

    char text[16];

    for (uint8_t i = 0; i < kVisibleRows; ++i) {
        Row& row = rows_[i];
        if (i >= shown) {
            lv_obj_add_flag(row.root, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_clear_flag(row.root, LV_OBJ_FLAG_HIDDEN);

        const AlarmEvent& event = log.at(i);

        // The stripe keeps its full colour whatever the state. Brightness says
        // "is this true now", colour says "how bad was it" - two questions, so
        // two channels, and the history stays readable at a glance.
        lv_obj_set_style_bg_color(row.stripe, severityColor(event.severity), 0);

        const lv_color_t body = event.active ? theme::text() : theme::dim();
        const lv_color_t accent = event.active ? severityColor(event.severity) : theme::dim();

        formatHms(event.time, text, sizeof(text));
        lv_label_set_text(row.time, text);
        lv_obj_set_style_text_color(row.time, body, 0);

        lv_label_set_text(row.label, event.label);
        lv_obj_set_style_text_color(row.label, accent, 0);

        lv_label_set_text(row.value, event.value);
        lv_obj_set_style_text_color(row.value, body, 0);

        snprintf(text, sizeof(text), "%u", static_cast<unsigned>(event.rpm));
        lv_label_set_text(row.rpm, text);
        lv_obj_set_style_text_color(row.rpm, body, 0);

        const uint32_t held = event.active ? nowMs - event.startMs
                                           : event.endMs - event.startMs;
        formatDuration(text, sizeof(text), held);
        lv_label_set_text(row.duration, text);
        lv_obj_set_style_text_color(row.duration, theme::dim(), 0);

        lv_label_set_text(row.state, event.active ? "ACT" : "RTN");
        lv_obj_set_style_text_color(row.state, accent, 0);
    }

    const uint8_t active = log.activeCount();
    snprintf(text, sizeof(text), "%u", static_cast<unsigned>(active));
    lv_label_set_text(activeCount_, text);
    lv_obj_set_style_text_color(activeCount_, active > 0 ? theme::warn() : theme::text(), 0);

    snprintf(text, sizeof(text), "%lu", static_cast<unsigned long>(log.total()));
    lv_label_set_text(totalCount_, text);

    const uint32_t seconds = nowMs / 1000;
    snprintf(text, sizeof(text), "%u:%02u",
             static_cast<unsigned>(seconds / 60), static_cast<unsigned>(seconds % 60));
    lv_label_set_text(runTime_, text);

    if (log.runStart().valid) {
        char stamp[9];
        formatHms(log.runStart(), stamp, sizeof(stamp));
        char line[24];
        snprintf(line, sizeof(line), "SINCE %s", stamp);
        lv_label_set_text(since_, line);
    } else {
        lv_label_set_text(since_, "NEWEST FIRST");
    }
    lv_obj_align(since_, LV_ALIGN_TOP_RIGHT, -12, kTextTop);
}

}  // namespace ecu

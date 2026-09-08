#include "setup_screen.hpp"

#include <stddef.h>
#include <stdio.h>

#include "dash_theme.hpp"
#include "ui_tile.hpp"

namespace ecu {
namespace {

constexpr lv_coord_t kStripHeight = 24;
constexpr lv_coord_t kCategoryWidth = 110;
constexpr lv_coord_t kCategoryHeight = 44;
constexpr lv_coord_t kRowAreaWidth = theme::kPageWidth - kCategoryWidth;
constexpr lv_coord_t kRowHeight = 32;

enum class SetupType : uint8_t { Float, U16, U8, Bool };

// One editable value. The offset points into DashSettings, so the table stays
// declarative and nothing here can reach anything else.
struct SetupItem {
    const char* name;
    const char* unit;
    SetupType type;
    uint16_t offset;
    float step;
    float minValue;
    float maxValue;
    uint8_t decimals;
};

struct SetupCategory {
    const char* name;
    const SetupItem* items;
    uint8_t count;
};

#define LIMIT_OFFSET(alarm, field) \
    static_cast<uint16_t>(offsetof(DashSettings, alarms.limits) + \
                          static_cast<uint8_t>(AlarmId::alarm) * sizeof(AlarmLimits) + \
                          offsetof(AlarmLimits, field))

#define FIELD_OFFSET(field) static_cast<uint16_t>(offsetof(DashSettings, field))

// Behaviour of the alarm system itself, plus which alarms are live at all.
const SetupItem kAlarmItems[] = {
    { "Arm delay",  "s", SetupType::U8,   FIELD_OFFSET(alarms.armDelayS),        1, 0, 15, 0 },
    { "Hysteresis", "%", SetupType::U8,   FIELD_OFFSET(alarms.hysteresisPercent), 1, 0, 10, 0 },
    { "Oil P",      "",  SetupType::Bool, LIMIT_OFFSET(OilPressure, enabled),    0, 0, 1, 0 },
    { "Coolant",    "",  SetupType::Bool, LIMIT_OFFSET(Coolant, enabled),        0, 0, 1, 0 },
    { "Lean",       "",  SetupType::Bool, LIMIT_OFFSET(Lean, enabled),           0, 0, 1, 0 },
    { "Batt low",   "",  SetupType::Bool, LIMIT_OFFSET(BatteryLow, enabled),     0, 0, 1, 0 },
    { "Charge",     "",  SetupType::Bool, LIMIT_OFFSET(BatteryHigh, enabled),    0, 0, 1, 0 },
    { "Knock",      "",  SetupType::Bool, LIMIT_OFFSET(Knock, enabled),          0, 0, 1, 0 },
    { "Inj DC",     "",  SetupType::Bool, LIMIT_OFFSET(InjectorDuty, enabled),   0, 0, 1, 0 },
    { "Fuel P",     "",  SetupType::Bool, LIMIT_OFFSET(FuelPressure, enabled),   0, 0, 1, 0 },
    { "IAT",        "",  SetupType::Bool, LIMIT_OFFSET(IntakeAir, enabled),      0, 0, 1, 0 },
};

const SetupItem kLimitItems[] = {
    { "Oil P warn",   "bar", SetupType::Float, LIMIT_OFFSET(OilPressure, warn),  0.1f, 0.2f, 6.0f, 1 },
    { "Oil P crit",   "bar", SetupType::Float, LIMIT_OFFSET(OilPressure, crit),  0.1f, 0.2f, 6.0f, 1 },
    { "CLT warn",     "C",   SetupType::Float, LIMIT_OFFSET(Coolant, warn),      1.0f, 70,   140,  0 },
    { "CLT crit",     "C",   SetupType::Float, LIMIT_OFFSET(Coolant, crit),      1.0f, 70,   140,  0 },
    { "Lean warn",    "",    SetupType::Float, LIMIT_OFFSET(Lean, warn),         0.01f, 0.70f, 1.30f, 2 },
    { "Lean crit",    "",    SetupType::Float, LIMIT_OFFSET(Lean, crit),         0.01f, 0.70f, 1.30f, 2 },
    { "Batt warn",    "V",   SetupType::Float, LIMIT_OFFSET(BatteryLow, warn),   0.1f, 9.0f, 14.5f, 1 },
    { "Batt crit",    "V",   SetupType::Float, LIMIT_OFFSET(BatteryLow, crit),   0.1f, 9.0f, 14.5f, 1 },
    { "Charge warn",  "V",   SetupType::Float, LIMIT_OFFSET(BatteryHigh, warn),  0.1f, 13.0f, 18.0f, 1 },
    { "Charge crit",  "V",   SetupType::Float, LIMIT_OFFSET(BatteryHigh, crit),  0.1f, 13.0f, 18.0f, 1 },
    { "Knock warn",   "V",   SetupType::Float, LIMIT_OFFSET(Knock, warn),        0.1f, 0.2f, 5.0f, 1 },
    { "Knock crit",   "V",   SetupType::Float, LIMIT_OFFSET(Knock, crit),        0.1f, 0.2f, 5.0f, 1 },
    { "Inj DC warn",  "%",   SetupType::Float, LIMIT_OFFSET(InjectorDuty, warn), 1.0f, 40,   100,  0 },
    { "Inj DC crit",  "%",   SetupType::Float, LIMIT_OFFSET(InjectorDuty, crit), 1.0f, 40,   100,  0 },
    { "Fuel P warn",  "bar", SetupType::Float, LIMIT_OFFSET(FuelPressure, warn), 0.1f, 1.0f, 8.0f, 1 },
    { "Fuel P crit",  "bar", SetupType::Float, LIMIT_OFFSET(FuelPressure, crit), 0.1f, 1.0f, 8.0f, 1 },
    { "IAT warn",     "C",   SetupType::Float, LIMIT_OFFSET(IntakeAir, warn),    1.0f, 30,   130,  0 },
    { "IAT crit",     "C",   SetupType::Float, LIMIT_OFFSET(IntakeAir, crit),    1.0f, 30,   130,  0 },
};

const SetupItem kShiftItems[] = {
    { "First light", "rpm", SetupType::U16, FIELD_OFFSET(shiftFirstRpm), 100, 1000, 9000, 0 },
    { "Red zone",    "rpm", SetupType::U16, FIELD_OFFSET(shiftRedRpm),   100, 1000, 9500, 0 },
    { "All lit",     "rpm", SetupType::U16, FIELD_OFFSET(shiftAllRpm),   100, 1000, 9500, 0 },
};

const SetupItem kDisplayItems[] = {
    { "Brightness",  "%", SetupType::U8,   FIELD_OFFSET(brightnessPct),      5, 10, 100, 0 },
    { "Night mode",  "",  SetupType::Bool, FIELD_OFFSET(nightMode),          0, 0, 1, 0 },
    { "Night level", "%", SetupType::U8,   FIELD_OFFSET(nightBrightnessPct), 5, 5,  100, 0 },
    { "Boot sweep",  "",  SetupType::Bool, FIELD_OFFSET(bootSweep),          0, 0, 1, 0 },
};

const SetupItem kLogItems[] = {
    { "Logging", "", SetupType::Bool, FIELD_OFFSET(logging), 0, 0, 1, 0 },
};

#define COUNT_OF(a) static_cast<uint8_t>(sizeof(a) / sizeof((a)[0]))

const SetupCategory kCategories[] = {
    { "Alarms",  kAlarmItems,   COUNT_OF(kAlarmItems) },
    { "Limits",  kLimitItems,   COUNT_OF(kLimitItems) },
    { "Shift",   kShiftItems,   COUNT_OF(kShiftItems) },
    { "Display", kDisplayItems, COUNT_OF(kDisplayItems) },
    { "Log",     kLogItems,     COUNT_OF(kLogItems) },
};
constexpr uint8_t kCategoryCount = COUNT_OF(kCategories);

// Button user data packs the item index and the direction into one word, so no
// per-button allocation is needed.
uint16_t packAction(uint16_t item, int8_t direction) {
    return static_cast<uint16_t>((item << 1) | (direction > 0 ? 1u : 0u));
}

void stepCb(lv_event_t* event) {
    SetupScreen* screen = static_cast<SetupScreen*>(lv_event_get_user_data(event));
    lv_obj_t* target = lv_event_get_target(event);
    const uint16_t packed =
        static_cast<uint16_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target)));
    screen->adjust(static_cast<uint16_t>(packed >> 1), (packed & 1u) ? +1 : -1);
}

void categoryCb(lv_event_t* event) {
    SetupScreen* screen = static_cast<SetupScreen*>(lv_event_get_user_data(event));
    lv_obj_t* target = lv_event_get_target(event);
    screen->selectCategory(
        static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target))));
}

}  // namespace

void SetupScreen::bind(DashSettings* settings, ChangeCallback onChange, void* context) {
    settings_ = settings;
    onChange_ = onChange;
    context_ = context;
}

void SetupScreen::create(lv_obj_t* parent) {
    root_ = makePanel(parent, 0, 0, theme::kPageWidth, theme::kPageHeight);

    // ---- information strip ----------------------------------------------
    lv_obj_t* strip = makePanel(root_, 0, 0, theme::kPageWidth, kStripHeight);
    lv_obj_set_style_bg_color(strip, theme::panel(), 0);
    lv_obj_set_style_bg_opa(strip, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(strip, theme::line(), 0);
    lv_obj_set_style_border_width(strip, 1, 0);
    lv_obj_set_style_border_side(strip, LV_BORDER_SIDE_BOTTOM, 0);

    lv_obj_t* stored = makeCaption(strip, "STORED IN FLASH");
    lv_obj_align(stored, LV_ALIGN_LEFT_MID, 10, 0);

    stateLabel_ = makeCaption(strip, "");
    lv_obj_align(stateLabel_, LV_ALIGN_RIGHT_MID, -10, 0);

    // ---- categories ------------------------------------------------------
    categoryList_ = makePanel(root_, 0, kStripHeight, kCategoryWidth,
                              theme::kPageHeight - kStripHeight);
    lv_obj_set_style_border_color(categoryList_, theme::line(), 0);
    lv_obj_set_style_border_width(categoryList_, 1, 0);
    lv_obj_set_style_border_side(categoryList_, LV_BORDER_SIDE_RIGHT, 0);

    // ---- rows ------------------------------------------------------------
    rowArea_ = makePanel(root_, kCategoryWidth, kStripHeight, kRowAreaWidth,
                         theme::kPageHeight - kStripHeight);
    lv_obj_add_flag(rowArea_, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(rowArea_, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(rowArea_, LV_SCROLLBAR_MODE_AUTO);

    buildCategories();
    buildRows();
}

void SetupScreen::buildCategories() {
    for (uint8_t i = 0; i < kCategoryCount; ++i) {
        lv_obj_t* item = makePanel(categoryList_, 0, i * kCategoryHeight,
                                   kCategoryWidth, kCategoryHeight);
        lv_obj_set_style_bg_color(item, theme::panel(), 0);
        lv_obj_set_style_border_color(item, theme::line(), 0);
        lv_obj_set_style_border_width(item, 1, 0);
        lv_obj_set_style_border_side(item, LV_BORDER_SIDE_BOTTOM, 0);
        lv_obj_add_flag(item, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(item, reinterpret_cast<void*>(static_cast<uintptr_t>(i)));
        lv_obj_add_event_cb(item, categoryCb, LV_EVENT_CLICKED, this);
        categoryItems_[i] = item;

        lv_obj_t* label = lv_label_create(item);
        lv_label_set_text(label, kCategories[i].name);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 12, 0);
        categoryLabels_[i] = label;

        lv_obj_t* mark = makePanel(item, 0, 0, 3, kCategoryHeight);
        lv_obj_set_style_bg_color(mark, theme::cyan(), 0);
        lv_obj_set_style_bg_opa(mark, LV_OPA_COVER, 0);
        categoryMarks_[i] = mark;
    }
    styleCategories();
}

void SetupScreen::styleCategories() {
    for (uint8_t i = 0; i < kCategoryCount; ++i) {
        const bool selected = (i == category_);
        lv_obj_set_style_bg_opa(categoryItems_[i], selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(categoryLabels_[i],
                                    selected ? theme::text() : theme::dim(), 0);
        if (selected) {
            lv_obj_clear_flag(categoryMarks_[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(categoryMarks_[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void SetupScreen::buildRows() {
    lv_obj_clean(rowArea_);
    for (uint8_t i = 0; i < kMaxRows; ++i) {
        valueLabels_[i] = nullptr;
        pillBoxes_[i] = nullptr;
        pillLabels_[i] = nullptr;
    }
    if (settings_ == nullptr) {
        return;
    }

    const SetupCategory& group = kCategories[category_];

    for (uint8_t i = 0; i < group.count; ++i) {
        const SetupItem& item = group.items[i];

        lv_obj_t* row = makePanel(rowArea_, 0, i * kRowHeight, kRowAreaWidth, kRowHeight);
        lv_obj_set_style_border_color(row, theme::line(), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_border_side(row, LV_BORDER_SIDE_BOTTOM, 0);

        lv_obj_t* name = makeCaption(row, item.name);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 10, 0);

        const float value = readValue(i);

        if (item.type == SetupType::Bool) {
            lv_obj_t* pill = makePanel(row, kRowAreaWidth - 88, 4, 74, kRowHeight - 9);
            const bool on = value > 0.5f;
            lv_obj_set_style_border_color(pill, on ? theme::good() : theme::line(), 0);
            lv_obj_set_style_border_width(pill, 1, 0);
            lv_obj_add_flag(pill, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_user_data(pill, reinterpret_cast<void*>(
                                           static_cast<uintptr_t>(packAction(i, +1))));
            lv_obj_add_event_cb(pill, stepCb, LV_EVENT_CLICKED, this);

            lv_obj_t* text = lv_label_create(pill);
            lv_label_set_text(text, on ? "ON" : "OFF");
            lv_obj_set_style_text_color(text, on ? theme::good() : theme::dim(), 0);
            lv_obj_center(text);

            pillBoxes_[i] = pill;
            pillLabels_[i] = text;
            continue;
        }

        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%.*f", item.decimals, value);
        lv_obj_t* shown = lv_label_create(row);
        lv_label_set_text(shown, buffer);
        lv_obj_align(shown, LV_ALIGN_LEFT_MID, 150, 0);
        valueLabels_[i] = shown;

        lv_obj_t* unit = makeCaption(row, item.unit);
        lv_obj_align(unit, LV_ALIGN_LEFT_MID, 212, 0);

        // Wide, square targets: this is operated with a thumb in a moving car.
        const lv_coord_t buttonY = 4;
        const lv_coord_t buttonH = kRowHeight - 9;
        for (int8_t direction = -1; direction <= 1; direction += 2) {
            const lv_coord_t x =
                direction < 0 ? kRowAreaWidth - 90 : kRowAreaWidth - 46;
            lv_obj_t* button = makePanel(row, x, buttonY, 36, buttonH);
            lv_obj_set_style_bg_color(button, theme::panel(), 0);
            lv_obj_set_style_bg_opa(button, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(button, theme::line(), 0);
            lv_obj_set_style_border_width(button, 1, 0);
            lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_user_data(button, reinterpret_cast<void*>(
                                             static_cast<uintptr_t>(packAction(i, direction))));
            lv_obj_add_event_cb(button, stepCb, LV_EVENT_CLICKED, this);

            lv_obj_t* glyph = lv_label_create(button);
            lv_label_set_text(glyph, direction < 0 ? "-" : "+");
            lv_obj_center(glyph);
        }
    }
}

float SetupScreen::readValue(uint16_t itemIndex) const {
    const SetupItem& item = kCategories[category_].items[itemIndex];
    const uint8_t* base = reinterpret_cast<const uint8_t*>(settings_) + item.offset;

    switch (item.type) {
        case SetupType::Float: return *reinterpret_cast<const float*>(base);
        case SetupType::U16:   return static_cast<float>(*reinterpret_cast<const uint16_t*>(base));
        case SetupType::U8:    return static_cast<float>(*base);
        case SetupType::Bool:  return *reinterpret_cast<const bool*>(base) ? 1.0f : 0.0f;
    }
    return 0.0f;
}

void SetupScreen::writeValue(uint16_t itemIndex, float value) {
    const SetupItem& item = kCategories[category_].items[itemIndex];
    uint8_t* base = reinterpret_cast<uint8_t*>(settings_) + item.offset;

    switch (item.type) {
        case SetupType::Float: *reinterpret_cast<float*>(base) = value; break;
        case SetupType::U16:   *reinterpret_cast<uint16_t*>(base) = static_cast<uint16_t>(value + 0.5f); break;
        case SetupType::U8:    *base = static_cast<uint8_t>(value + 0.5f); break;
        case SetupType::Bool:  *reinterpret_cast<bool*>(base) = value > 0.5f; break;
    }
}

void SetupScreen::adjust(uint16_t itemIndex, int8_t direction) {
    if (settings_ == nullptr || itemIndex >= kCategories[category_].count) {
        return;
    }
    const SetupItem& item = kCategories[category_].items[itemIndex];

    if (item.type == SetupType::Bool) {
        writeValue(itemIndex, readValue(itemIndex) > 0.5f ? 0.0f : 1.0f);
    } else {
        float next = readValue(itemIndex) + direction * item.step;
        if (next < item.minValue) next = item.minValue;
        if (next > item.maxValue) next = item.maxValue;
        writeValue(itemIndex, next);
    }

    // In place, never a rebuild: this runs inside the button's own click
    // callback, and deleting that button here would free it mid-event.
    refreshRow(itemIndex);

    if (onChange_ != nullptr) {
        onChange_(context_);
    }
}

void SetupScreen::refreshRow(uint16_t itemIndex) {
    if (itemIndex >= kMaxRows) {
        return;
    }
    const SetupItem& item = kCategories[category_].items[itemIndex];
    const float value = readValue(itemIndex);

    if (item.type == SetupType::Bool) {
        if (pillBoxes_[itemIndex] == nullptr) {
            return;
        }
        const bool on = value > 0.5f;
        lv_label_set_text(pillLabels_[itemIndex], on ? "ON" : "OFF");
        lv_obj_set_style_text_color(pillLabels_[itemIndex],
                                    on ? theme::good() : theme::dim(), 0);
        lv_obj_set_style_border_color(pillBoxes_[itemIndex],
                                      on ? theme::good() : theme::line(), 0);
        return;
    }

    if (valueLabels_[itemIndex] == nullptr) {
        return;
    }
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.*f", item.decimals, value);
    lv_label_set_text(valueLabels_[itemIndex], buffer);
}

void SetupScreen::selectCategory(uint8_t index) {
    if (index >= kCategoryCount || index == category_) {
        return;
    }
    category_ = index;
    styleCategories();
    buildRows();   // safe: the clicked object lives in the category list
}

void SetupScreen::update(const EngineDataModel& model,
                         const AlarmEngine& alarms,
                         const RunPeaks& peaks,
                         uint32_t nowMs) {
    (void)model;
    (void)peaks;
    (void)nowMs;

    const bool running = alarms.engineRunning();
    if (running == lastRunning_) {
        return;
    }
    lastRunning_ = running;
    lv_label_set_text(stateLabel_, running ? "ENGINE RUNNING" : "ENGINE OFF");
}

}  // namespace ecu

#include "setup_screen.hpp"

#include <stddef.h>
#include <stdio.h>

#include "dash_theme.hpp"
#include "setup_items.hpp"
#include "ui_tile.hpp"

namespace ecu {
namespace {

constexpr lv_coord_t kStripHeight = 24;
constexpr lv_coord_t kCategoryWidth = 110;
constexpr lv_coord_t kCategoryHeight = 44;
constexpr lv_coord_t kRowAreaWidth = theme::kPageWidth - kCategoryWidth;
constexpr lv_coord_t kRowHeight = 32;

// Button user data packs the pool row and the direction into one word, so no
// per-button allocation is needed.
uint16_t packAction(uint8_t row, int8_t direction) {
    return static_cast<uint16_t>((row << 1) | (direction > 0 ? 1u : 0u));
}

void stepCb(lv_event_t* event) {
    SetupScreen* screen = static_cast<SetupScreen*>(lv_event_get_user_data(event));
    lv_obj_t* target = lv_event_get_target(event);
    const uint16_t packed =
        static_cast<uint16_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target)));
    screen->adjust(static_cast<uint8_t>(packed >> 1), (packed & 1u) ? +1 : -1);
}

void categoryCb(lv_event_t* event) {
    SetupScreen* screen = static_cast<SetupScreen*>(lv_event_get_user_data(event));
    lv_obj_t* target = lv_event_get_target(event);
    screen->selectCategory(
        static_cast<uint8_t>(reinterpret_cast<uintptr_t>(lv_obj_get_user_data(target))));
}

void setHidden(lv_obj_t* obj, bool hidden) {
    if (obj == nullptr) {
        return;
    }
    if (hidden) {
        lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
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

    // Says where in a long category the rows are, and that a swipe will move
    // them. Blank on the categories that fit, which is most of them.
    positionLabel_ = makeCaption(strip, "");
    lv_obj_align(positionLabel_, LV_ALIGN_CENTER, 0, 0);

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

    buildCategories();
    buildRows();
    paintRows();
}

void SetupScreen::buildCategories() {
    for (uint8_t i = 0; i < kSetupCategoryCount; ++i) {
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
        lv_label_set_text(label, kSetupCategories[i].name);
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
    for (uint8_t i = 0; i < kSetupCategoryCount; ++i) {
        const bool selected = (i == category_);
        lv_obj_set_style_bg_opa(categoryItems_[i], selected ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
        lv_obj_set_style_text_color(categoryLabels_[i],
                                    selected ? theme::text() : theme::dim(), 0);
        setHidden(categoryMarks_[i], !selected);
    }
}

// Every widget this page will ever own, created once. Nothing below this
// allocates: paintRows only moves text and colours around.
void SetupScreen::buildRows() {
    for (uint8_t i = 0; i < kVisibleRows; ++i) {
        Row& row = rows_[i];

        row.root = makePanel(rowArea_, 0, i * kRowHeight, kRowAreaWidth, kRowHeight);
        lv_obj_set_style_border_color(row.root, theme::line(), 0);
        lv_obj_set_style_border_width(row.root, 1, 0);
        lv_obj_set_style_border_side(row.root, LV_BORDER_SIDE_BOTTOM, 0);

        row.name = makeCaption(row.root, "");
        lv_obj_align(row.name, LV_ALIGN_LEFT_MID, 10, 0);

        row.value = lv_label_create(row.root);
        lv_label_set_text(row.value, "");
        lv_obj_align(row.value, LV_ALIGN_LEFT_MID, 150, 0);

        row.unit = makeCaption(row.root, "");
        lv_obj_align(row.unit, LV_ALIGN_LEFT_MID, 212, 0);

        // Wide, square targets: this is operated with a thumb in a moving car.
        const lv_coord_t buttonY = 4;
        const lv_coord_t buttonH = kRowHeight - 9;
        for (int8_t direction = -1; direction <= 1; direction += 2) {
            const lv_coord_t x =
                direction < 0 ? kRowAreaWidth - 90 : kRowAreaWidth - 46;
            lv_obj_t* button = makePanel(row.root, x, buttonY, 36, buttonH);
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

            if (direction < 0) {
                row.minus = button;
            } else {
                row.plus = button;
            }
        }

        row.pill = makePanel(row.root, kRowAreaWidth - 88, 4, 74, kRowHeight - 9);
        lv_obj_set_style_border_width(row.pill, 1, 0);
        lv_obj_add_flag(row.pill, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(row.pill, reinterpret_cast<void*>(
                                           static_cast<uintptr_t>(packAction(i, +1))));
        lv_obj_add_event_cb(row.pill, stepCb, LV_EVENT_CLICKED, this);

        row.pillText = lv_label_create(row.pill);
        lv_label_set_text(row.pillText, "");
        lv_obj_center(row.pillText);

        lv_obj_add_flag(row.root, LV_OBJ_FLAG_HIDDEN);
    }
}

uint8_t SetupScreen::itemCount() const {
    return kSetupCategories[category_].count;
}

// The item a pool row is showing, or null when the row is past the end of the
// category. Everything that reads a setting goes through this, so the scroll
// offset is applied in exactly one place.
const SetupItem* SetupScreen::itemAt(uint8_t row) const {
    const uint16_t index = static_cast<uint16_t>(scrollTop_) + row;
    if (index >= itemCount()) {
        return nullptr;
    }
    return &kSetupCategories[category_].items[index];
}

void SetupScreen::paintRow(uint8_t row) {
    if (row >= kVisibleRows || settings_ == nullptr) {
        return;
    }
    Row& widgets = rows_[row];
    const SetupItem* item = itemAt(row);

    if (item == nullptr) {
        setHidden(widgets.root, true);
        return;
    }
    setHidden(widgets.root, false);
    lv_label_set_text(widgets.name, item->name);

    const float value = readSetting(*settings_, *item);
    const bool isBool = item->type == SetupType::Bool;

    setHidden(widgets.pill, !isBool);
    setHidden(widgets.value, isBool);
    setHidden(widgets.unit, isBool);
    setHidden(widgets.minus, isBool);
    setHidden(widgets.plus, isBool);

    if (isBool) {
        const bool on = value > 0.5f;
        lv_label_set_text(widgets.pillText, on ? "ON" : "OFF");
        lv_obj_set_style_text_color(widgets.pillText, on ? theme::good() : theme::dim(), 0);
        lv_obj_set_style_border_color(widgets.pill, on ? theme::good() : theme::line(), 0);
        return;
    }

    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%.*f", item->decimals, value);
    lv_label_set_text(widgets.value, buffer);
    lv_label_set_text(widgets.unit, item->unit);
}

void SetupScreen::paintRows() {
    const uint8_t count = itemCount();
    const uint8_t maxTop =
        count > kVisibleRows ? static_cast<uint8_t>(count - kVisibleRows) : 0;
    if (scrollTop_ > maxTop) {
        scrollTop_ = maxTop;
    }

    for (uint8_t i = 0; i < kVisibleRows; ++i) {
        paintRow(i);
    }

    if (count > kVisibleRows) {
        const uint8_t last = static_cast<uint8_t>(scrollTop_ + kVisibleRows);
        char text[32];
        snprintf(text, sizeof(text), "%s %u-%u/%u %s",
                 scrollTop_ > 0 ? LV_SYMBOL_UP : " ",
                 static_cast<unsigned>(scrollTop_ + 1),
                 static_cast<unsigned>(last < count ? last : count),
                 static_cast<unsigned>(count),
                 scrollTop_ < maxTop ? LV_SYMBOL_DOWN : " ");
        lv_label_set_text(positionLabel_, text);
    } else {
        lv_label_set_text(positionLabel_, "");
    }
    lv_obj_align(positionLabel_, LV_ALIGN_CENTER, 0, 0);
}

void SetupScreen::adjust(uint8_t row, int8_t direction) {
    const SetupItem* item = itemAt(row);
    if (settings_ == nullptr || item == nullptr) {
        return;
    }

    const float current = readSetting(*settings_, *item);
    if (item->type == SetupType::Bool) {
        writeSetting(*settings_, *item, current > 0.5f ? 0.0f : 1.0f);
    } else {
        writeSetting(*settings_, *item,
                     clampSetting(*item, current + direction * item->step));
    }

    // In place, never a rebuild: this runs inside the button's own click
    // callback, and deleting that button here would free it mid-event. With a
    // pool there is nothing to rebuild anyway.
    paintRow(row);

    if (onChange_ != nullptr) {
        onChange_(context_);
    }
}

void SetupScreen::refresh() {
    if (settings_ == nullptr) {
        return;
    }
    paintRows();
}

void SetupScreen::selectCategory(uint8_t index) {
    if (index >= kSetupCategoryCount || index == category_) {
        return;
    }
    category_ = index;
    scrollTop_ = 0;
    styleCategories();
    paintRows();
}

// Up and down walk a long category; left and right are left alone so the swipe
// that turns the page still works from here.
//
// Every vertical swipe is claimed, including one that scrolls nothing, and
// that is deliberate. Claiming it makes DashUi wait for the release, which is
// what stops a drag that started on a "+" from landing as a press on it when
// the finger comes up. The row area used to be an LVGL scroller and got that
// protection for free; it is a pool now, so this has to say so. A page of
// thresholds is the last place to accept an accidental increment.
bool SetupScreen::onSwipe(lv_dir_t direction) {
    if (direction != LV_DIR_TOP && direction != LV_DIR_BOTTOM) {
        return false;
    }

    const uint8_t count = itemCount();
    if (count <= kVisibleRows) {
        return true;   // nothing to scroll, but still not a button press
    }
    const uint8_t maxTop = static_cast<uint8_t>(count - kVisibleRows);

    const uint8_t next =
        direction == LV_DIR_TOP
            ? (scrollTop_ + kScrollStep > maxTop ? maxTop
                                                 : static_cast<uint8_t>(scrollTop_ + kScrollStep))
            : (scrollTop_ < kScrollStep ? 0 : static_cast<uint8_t>(scrollTop_ - kScrollStep));

    if (next != scrollTop_) {
        scrollTop_ = next;
        paintRows();
    }
    return true;
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

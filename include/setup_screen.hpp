#pragma once

#include "dash_page.hpp"
#include "dash_settings.hpp"
#include "setup_items.hpp"

namespace ecu {

// Page 8 - SETUP. Thresholds and display, changed without a laptop.
//
// Stays editable with the engine running: setting a limit without watching the
// live value it guards is guesswork, and that is the thing this page exists to
// stop.
//
// The rows are a fixed pool, built once and re-labelled - never built per
// category. That is not an optimisation, it is the fix for a reboot. LVGL's
// pool is a fixed array, and `lv_obj_create` on a full pool returns NULL and
// is then dereferenced inside LVGL itself, so the dashboard panics and
// restarts with nothing printed. Building the eighteen-row Limits category
// meant about a hundred and fifty objects in one go, which is what tipped it
// over once the IDLE and BOOST pages had taken their share. With a pool this
// page costs the same whatever is on it, and adding settings can never cost
// more.
class SetupScreen : public DashPage {
public:
    using ChangeCallback = void (*)(void* context);

    // `settings` is edited in place; `onChange` fires after every edit so the
    // owner can apply and schedule a save.
    void bind(DashSettings* settings, ChangeCallback onChange, void* context);

    void create(lv_obj_t* parent) override;
    void update(const EngineDataModel& model,
                const AlarmEngine& alarms,
                const RunPeaks& peaks,
                uint32_t nowMs) override;
    bool onSwipe(lv_dir_t direction) override;
    const char* name() const override { return "SETUP"; }

    // Called by the button callbacks. `row` is a position in the pool, not an
    // index into the category - the two differ by the scroll position.
    void adjust(uint8_t row, int8_t direction);
    void selectCategory(uint8_t index);

    // Repaint every row from the settings, for when they were changed by
    // something other than these buttons - the service page edits the same
    // struct, and until this existed the panel went on showing the old number.
    void refresh();

private:
    // Eight 32 px rows fill the 262 px row area. A category longer than this
    // scrolls; the two that are, say so in the strip.
    static constexpr uint8_t kVisibleRows = 8;
    static constexpr uint8_t kScrollStep = 4;
    static constexpr uint8_t kMaxCategories = 6;

    // One pool row. Both shapes are built once and the one this row is not
    // showing is hidden, so a bool row and a number row cost the same and
    // neither costs an allocation to switch between.
    struct Row {
        lv_obj_t* root = nullptr;
        lv_obj_t* name = nullptr;
        lv_obj_t* value = nullptr;
        lv_obj_t* unit = nullptr;
        lv_obj_t* minus = nullptr;
        lv_obj_t* plus = nullptr;
        lv_obj_t* pill = nullptr;
        lv_obj_t* pillText = nullptr;
    };

    void buildCategories();
    void styleCategories();
    void buildRows();
    void paintRows();
    void paintRow(uint8_t row);
    uint8_t itemCount() const;
    const SetupItem* itemAt(uint8_t row) const;

    DashSettings* settings_ = nullptr;
    ChangeCallback onChange_ = nullptr;
    void* context_ = nullptr;

    lv_obj_t* stateLabel_ = nullptr;
    lv_obj_t* positionLabel_ = nullptr;
    lv_obj_t* categoryList_ = nullptr;
    lv_obj_t* rowArea_ = nullptr;

    // Categories are built once and restyled, never deleted: a click callback
    // must not free the object it was fired on. The same now goes for rows.
    lv_obj_t* categoryItems_[kMaxCategories] = {};
    lv_obj_t* categoryLabels_[kMaxCategories] = {};
    lv_obj_t* categoryMarks_[kMaxCategories] = {};

    Row rows_[kVisibleRows] = {};

    uint8_t category_ = 0;
    uint8_t scrollTop_ = 0;
    bool lastRunning_ = false;
};

}  // namespace ecu

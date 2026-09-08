#pragma once

#include "dash_page.hpp"
#include "dash_settings.hpp"

namespace ecu {

// Page 5 - SETUP. Thresholds and display, changed without a laptop.
//
// Stays editable with the engine running: setting a limit without watching the
// live value it guards is guesswork, and that is the thing this page exists to
// stop.
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
    const char* name() const override { return "SETUP"; }

    // Called by the button callbacks.
    void adjust(uint16_t itemIndex, int8_t direction);
    void selectCategory(uint8_t index);

private:
    static constexpr uint8_t kMaxRows = 20;
    static constexpr uint8_t kMaxCategories = 6;

    void buildCategories();
    void styleCategories();
    void buildRows();
    void refreshRow(uint16_t itemIndex);
    float readValue(uint16_t itemIndex) const;
    void writeValue(uint16_t itemIndex, float value);

    DashSettings* settings_ = nullptr;
    ChangeCallback onChange_ = nullptr;
    void* context_ = nullptr;

    lv_obj_t* stateLabel_ = nullptr;
    lv_obj_t* categoryList_ = nullptr;
    lv_obj_t* rowArea_ = nullptr;

    // Categories are built once and restyled, never deleted: a click callback
    // must not free the object it was fired on.
    lv_obj_t* categoryItems_[kMaxCategories] = {};
    lv_obj_t* categoryLabels_[kMaxCategories] = {};
    lv_obj_t* categoryMarks_[kMaxCategories] = {};

    // Row widgets are updated in place for the same reason, and because
    // rebuilding eighteen rows on every button press is visibly slow.
    lv_obj_t* valueLabels_[kMaxRows] = {};
    lv_obj_t* pillBoxes_[kMaxRows] = {};
    lv_obj_t* pillLabels_[kMaxRows] = {};

    uint8_t category_ = 0;
    bool lastRunning_ = false;
};

}  // namespace ecu

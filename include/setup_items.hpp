#pragma once

#include <stdint.h>

#include "dash_settings.hpp"

namespace ecu {

enum class SetupType : uint8_t { Float, U16, U8, Bool };

// One editable value. The offset points into DashSettings, so the table stays
// declarative and nothing here can reach anything else.
//
// This table is the only list of what a driver may change. It lives in its own
// header rather than inside the setup screen because there are two editors for
// the same settings - the panel and the service page - and a second list kept
// by hand would drift from this one the first time a field was added.
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

extern const SetupCategory kSetupCategories[];
extern const uint8_t kSetupCategoryCount;

// Read and write a field through its table entry. Both editors go through
// these, so the pointer arithmetic that reaches into DashSettings exists once.
float readSetting(const DashSettings& settings, const SetupItem& item);
void writeSetting(DashSettings& settings, const SetupItem& item, float value);

// Clamps to the item's own range. A value from a web form is no more trusted
// than one from a button, so both are clamped the same way.
float clampSetting(const SetupItem& item, float value);

}  // namespace ecu

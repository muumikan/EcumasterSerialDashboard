#pragma once

#include <stdint.h>

namespace ecu {

enum class AlarmSeverity : uint8_t {
    None = 0,
    Warning = 1,
    Critical = 2,
};

}  // namespace ecu

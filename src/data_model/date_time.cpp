#include "date_time.hpp"

#include <stdio.h>

namespace ecu {

void formatHm(const DateTime& t, char* out, size_t len) {
    if (!t.valid) {
        snprintf(out, len, "--:--");
        return;
    }
    snprintf(out, len, "%02u:%02u",
             static_cast<unsigned>(t.hour), static_cast<unsigned>(t.minute));
}

void formatHms(const DateTime& t, char* out, size_t len) {
    if (!t.valid) {
        snprintf(out, len, "--:--:--");
        return;
    }
    snprintf(out, len, "%02u:%02u:%02u",
             static_cast<unsigned>(t.hour), static_cast<unsigned>(t.minute),
             static_cast<unsigned>(t.second));
}

}  // namespace ecu

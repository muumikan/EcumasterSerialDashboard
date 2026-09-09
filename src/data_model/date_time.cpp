#include "date_time.hpp"

#include <stdio.h>

namespace ecu {

bool isBefore(const DateTime& a, const DateTime& b) {
    if (a.year != b.year) return a.year < b.year;
    if (a.month != b.month) return a.month < b.month;
    if (a.day != b.day) return a.day < b.day;
    if (a.hour != b.hour) return a.hour < b.hour;
    if (a.minute != b.minute) return a.minute < b.minute;
    return a.second < b.second;
}

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

#pragma once

#include <stdint.h>
#include <stddef.h>

namespace ecu {

// Wall-clock instant, as the RTC reports it.
//
// A plain value type in its own header so the alarm layer can carry timestamps
// without depending on the I2C driver that produces them.
struct DateTime {
    uint16_t year = 0;   // full year, e.g. 2026
    uint8_t month = 0;   // 1-12
    uint8_t day = 0;     // 1-31
    uint8_t hour = 0;    // 0-23
    uint8_t minute = 0;
    uint8_t second = 0;

    // False until the RTC has been read successfully. Everything that prints a
    // timestamp checks this rather than printing a plausible-looking zero.
    bool valid = false;
};

// "14:41", or "--:--" when the time is not known. Needs 6 bytes.
void formatHm(const DateTime& t, char* out, size_t len);

// "14:41:02", or "--:--:--". Needs 9 bytes.
void formatHms(const DateTime& t, char* out, size_t len);

}  // namespace ecu

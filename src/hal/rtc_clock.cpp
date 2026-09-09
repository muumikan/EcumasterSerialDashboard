#include "rtc_clock.hpp"

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <stdlib.h>
#include <string.h>

#include "board_config.hpp"

namespace ecu {
namespace {

// I2C0, the same bus and the same driver the GT911 touch panel uses.
constexpr int kPort = 0;
constexpr int kAddr = 0x51;  // fixed on the BM8563, no address pins
constexpr uint32_t kFreq = 400000;

// Register map, PCF8563 compatible.
constexpr uint8_t kRegControl1 = 0x00;
constexpr uint8_t kRegSeconds = 0x02;

constexpr uint8_t kStopBit = 0x20;         // Control_status_1 bit 5
constexpr uint8_t kVoltageLowBit = 0x80;   // VL, seconds register bit 7

// One second. The bus is shared with the touch controller, which is polled
// from LVGL's own timer, so there is no reason to be greedier than the
// smallest unit the dashboard ever displays.
constexpr uint32_t kReadIntervalMs = 1000;

uint8_t bcdToDec(uint8_t value) {
    return static_cast<uint8_t>((value >> 4) * 10 + (value & 0x0F));
}

uint8_t decToBcd(uint8_t value) {
    return static_cast<uint8_t>(((value / 10) << 4) | (value % 10));
}

// Sakamoto's method, 0 = Sunday. The RTC keeps a weekday register that has to
// be written along with the date; nothing here reads it back.
uint8_t weekdayOf(const DateTime& t) {
    static const int8_t offsets[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    int year = t.year;
    if (t.month < 3) {
        --year;
    }
    return static_cast<uint8_t>(
        (year + year / 4 - year / 100 + year / 400 + offsets[t.month - 1] + t.day) % 7);
}

bool readRegisters(uint8_t first, uint8_t* values, size_t count) {
    return lgfx::i2c::readRegister(kPort, kAddr, first, values, count, kFreq).has_value();
}

bool writeRegisters(uint8_t first, const uint8_t* values, uint8_t count) {
    uint8_t buffer[10];
    if (count + 1 > static_cast<int>(sizeof(buffer))) {
        return false;
    }
    buffer[0] = first;
    memcpy(&buffer[1], values, count);
    return lgfx::i2c::transactionWrite(kPort, kAddr, buffer,
                                       static_cast<uint8_t>(count + 1), kFreq)
        .has_value();
}

}  // namespace

DateTime buildTime() {
    // __DATE__ is "Mmm DD YYYY" with the day space padded, __TIME__ "HH:MM:SS".
    static const char kMonths[] = "JanFebMarAprMayJunJulAugSepOctNovDec";

    DateTime t;
    const char* date = __DATE__;
    const char* clock = __TIME__;

    for (uint8_t i = 0; i < 12; ++i) {
        if (strncmp(date, &kMonths[i * 3], 3) == 0) {
            t.month = static_cast<uint8_t>(i + 1);
            break;
        }
    }
    if (t.month == 0) {
        return t;  // not a date this compiler produced; leave it invalid
    }

    t.day = static_cast<uint8_t>(atoi(date + 4));
    t.year = static_cast<uint16_t>(atoi(date + 7));
    t.hour = static_cast<uint8_t>(atoi(clock));
    t.minute = static_cast<uint8_t>(atoi(clock + 3));
    t.second = static_cast<uint8_t>(atoi(clock + 6));
    t.valid = true;
    return t;
}

bool RtcClock::begin() {
    // The touch panel brings this bus up, and lgfx::i2c::init tears a working
    // bus down before rebuilding it. So try to talk to the chip first and only
    // configure the bus if that fails - which is the case where the display
    // never came up and nobody else has claimed the pins.
    uint8_t seconds = 0;
    if (!readRegisters(kRegSeconds, &seconds, 1)) {
        if (!lgfx::i2c::init(kPort, board::kTouchSdaPin, board::kTouchSclPin).has_value() ||
            !readRegisters(kRegSeconds, &seconds, 1)) {
            Serial.println(F("rtc: BM8563 did not answer at 0x51"));
            return false;
        }
    }
    present_ = true;
    readTime(now_);

    // Two triggers, because one is not enough.
    //
    // VL says the oscillator stopped. It does NOT say the time is right: a
    // part kept alive by its backup cell since the factory has a clock that
    // has been running happily and has never been set, and VL is clear the
    // whole time. That is exactly what this board turned out to have - the
    // display read 00:24 while the firmware that read it was compiled at
    // 19:44.
    //
    // So the second trigger is plausibility: a clock cannot legitimately read
    // earlier than the moment the firmware reading it was compiled. Anything
    // that does has never been set, and gets set now. Once seeded the RTC runs
    // ahead of the build time, so this does not fire again on later boots -
    // and a newer firmware flashed over it leaves a correct clock alone.
    const DateTime seed = buildTime();
    const bool stopped = (seconds & kVoltageLowBit) != 0;
    const bool implausible = now_.valid && seed.valid && isBefore(now_, seed);

    if (stopped || implausible) {
        const char* reason = stopped ? "oscillator had stopped"
                                     : "stored time predated this build";
        if (seed.valid && writeTime(seed)) {
            readTime(now_);
            Serial.print(F("rtc: seeded from the build clock ("));
            Serial.print(reason);
            Serial.println(')');
        } else {
            Serial.print(F("rtc: time is not set and could not be seeded ("));
            Serial.print(reason);
            Serial.println(')');
        }
    }
    nextReadMs_ = millis() + kReadIntervalMs;

    char shown[9];
    formatHms(now_, shown, sizeof(shown));
    Serial.print(F("rtc: BM8563 at 0x51, time "));
    Serial.println(shown);
    return true;
}

bool RtcClock::loop(uint32_t nowMs) {
    if (!present_ || static_cast<int32_t>(nowMs - nextReadMs_) < 0) {
        return false;
    }
    nextReadMs_ = nowMs + kReadIntervalMs;
    return readTime(now_);
}

bool RtcClock::readTime(DateTime& out) const {
    uint8_t buffer[7] = {0};
    if (!readRegisters(kRegSeconds, buffer, sizeof(buffer))) {
        out.valid = false;
        return false;
    }
    if (buffer[0] & kVoltageLowBit) {
        out.valid = false;  // the chip itself says the reading cannot be trusted
        return false;
    }

    out.second = bcdToDec(buffer[0] & 0x7F);
    out.minute = bcdToDec(buffer[1] & 0x7F);
    out.hour = bcdToDec(buffer[2] & 0x3F);
    out.day = bcdToDec(buffer[3] & 0x3F);
    // buffer[4] is the weekday, which nothing here needs.
    out.month = bcdToDec(buffer[5] & 0x1F);
    // The century bit (register 0x07 bit 7) is always written as zero and
    // ignored on read. This dashboard will not be running in 2100.
    out.year = static_cast<uint16_t>(2000 + bcdToDec(buffer[6]));
    out.valid = true;
    return true;
}

bool RtcClock::writeTime(const DateTime& t) const {
    // Hold the counter still while the registers are updated, so a carry
    // cannot land in the middle of the write.
    const uint8_t stop = kStopBit;
    if (!writeRegisters(kRegControl1, &stop, 1)) {
        return false;
    }

    const uint8_t values[7] = {
        decToBcd(t.second),  // bit 7 zero here is what clears the VL flag
        decToBcd(t.minute),
        decToBcd(t.hour),
        decToBcd(t.day),
        weekdayOf(t),
        decToBcd(t.month),  // century bit left at zero
        decToBcd(static_cast<uint8_t>(t.year % 100)),
    };
    const bool written = writeRegisters(kRegSeconds, values, sizeof(values));

    const uint8_t run = 0;
    return writeRegisters(kRegControl1, &run, 1) && written;
}

}  // namespace ecu

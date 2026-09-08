#pragma once

#include "date_time.hpp"

namespace ecu {

// The BM8563 real-time clock on the CrowPanel's I2C0 bus.
//
// The part is register-compatible with NXP's PCF8563. It shares SDA/SCL with
// the GT911 touch controller, so this driver deliberately uses LovyanGFX's own
// I2C implementation rather than Arduino's Wire: LovyanGFX drives that
// peripheral directly for the touch panel, and a second driver configuring the
// same registers is asking for trouble.
//
// Read-only in normal running. The one write is the seed described below.
class RtcClock {
public:
    // Probes the part and, if it has lost time, seeds it from the build clock.
    // Returns false when the chip does not answer.
    bool begin();

    // Re-reads at most once a second. Returns true when a fresh reading was
    // taken, so callers can repaint only then.
    bool loop(uint32_t nowMs);

    const DateTime& now() const { return now_; }
    bool present() const { return present_; }

private:
    bool readTime(DateTime& out) const;
    bool writeTime(const DateTime& t) const;

    DateTime now_;
    uint32_t nextReadMs_ = 0;
    bool present_ = false;
};

// The moment this firmware was compiled, from __DATE__ and __TIME__.
//
// This is the only source of time the dashboard has: there is no network and
// no GPS in the car. It is written to the RTC exactly once, when the chip
// reports that its oscillator stopped - see RtcClock::begin.
DateTime buildTime();

}  // namespace ecu

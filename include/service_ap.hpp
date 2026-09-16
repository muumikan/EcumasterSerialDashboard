#pragma once

#include <stdint.h>

#include "dash_settings.hpp"
#include "engine_data_model.hpp"

namespace ecu {

// Raises a WiFi access point while the engine is stopped, so the logs can be
// fetched with a laptop instead of the card being carried indoors.
//
// The engine-stopped condition is not a convenience. It is what keeps this
// simple: with the engine off the ECU sends nothing, so nothing is being
// written to the card while the server reads from it. No second task, no
// mutex, no shared card between two cores - the whole thing runs in the same
// cooperative loop as everything else.
//
// Conditions and timings live in service_config.hpp.
class ServiceAp {
public:
    enum class State : uint8_t {
        Off,       // disabled, or the engine is running
        Arming,    // engine has stopped; waiting out the settle delay
        Serving,   // access point up
    };

    void begin(const DashSettings& settings);

    void loop(const EngineDataModel& model, uint32_t nowMs);

    State state() const { return state_; }
    bool serving() const { return state_ == State::Serving; }

    // Stations currently associated. Drives the idle timeout and the status
    // bar, so the driver can see whether the laptop actually joined.
    uint8_t clients() const { return clients_; }

    // Seconds until the idle timeout stops the access point, or 0 when that
    // is not running.
    uint32_t idleSecondsLeft(uint32_t nowMs) const;

    // Why the access point is not up, or what it is doing. Always current.
    //
    // Five separate conditions hold this radio down and none of them is
    // visible from outside the car: without this, an access point that never
    // appears is indistinguishable from a firmware that does not have the
    // feature at all.
    const char* reason() const { return reason_; }

    // Seconds left of the settle delay while arming, otherwise 0.
    uint32_t armingSecondsLeft(uint32_t nowMs) const;

    const char* ssid() const;
    const char* ipAddress() const { return ip_; }

private:
    void start();
    void stop(const char* why);
    bool engineStopped(const EngineDataModel& model, uint32_t nowMs) const;

    const DashSettings* settings_ = nullptr;

    State state_ = State::Off;
    uint8_t clients_ = 0;
    uint32_t stoppedSinceMs_ = 0;
    uint32_t lastClientMs_ = 0;
    uint32_t lastPollMs_ = 0;

    // Set when the idle timeout took the access point down, so it does not
    // come straight back up on the next pass with the car still parked. One
    // window per drive; running the engine clears it.
    bool idleLatched_ = false;

    const char* reason_ = "not started";

    char ip_[16] = {0};
};

}  // namespace ecu

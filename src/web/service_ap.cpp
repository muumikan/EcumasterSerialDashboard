#include "service_ap.hpp"

#include <Arduino.h>
#include <WiFi.h>
#include <string.h>

#include "alarm_engine.hpp"
#include "service_config.hpp"

namespace ecu {
namespace {

// Association counts do not change quickly, and esp_wifi_ap_get_sta_list is
// not free. Once a second is plenty for a timeout measured in minutes.
constexpr uint32_t kPollIntervalMs = 1000;

}  // namespace

void ServiceAp::begin(const DashSettings& settings) {
    settings_ = &settings;

    // The radio stays off until something asks for it. This is a car: an
    // access point nobody requested is current drawn for nothing.
    WiFi.mode(WIFI_OFF);
}

const char* ServiceAp::ssid() const { return service::kSsid; }

ServiceApStatus ServiceAp::status() const {
    ServiceApStatus out;
    out.serving = state_ == State::Serving;
    out.clients = clients_;
    out.ssid = service::kSsid;
    out.password = service::kPassword;
    out.ip = ip_;
    return out;
}

bool ServiceAp::engineStopped(const EngineDataModel& model, uint32_t nowMs) const {
    const LinkState link = model.linkState(nowMs);
    if (link == LinkState::Offline) {
        // Nothing from the ECU at all. The engine cannot be running and
        // reporting nothing, so this counts as stopped.
        return true;
    }
    return model.snapshot().rpm < kEngineRunningRpm;
}

void ServiceAp::start() {
    WiFi.mode(WIFI_AP);
    if (!WiFi.softAP(service::kSsid, service::kPassword)) {
        Serial.println(F("service: could not start the access point"));
        WiFi.mode(WIFI_OFF);
        state_ = State::Off;
        reason_ = "the radio refused to start";
        return;
    }

    const IPAddress ip = WiFi.softAPIP();
    snprintf(ip_, sizeof(ip_), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);

    state_ = State::Serving;
    reason_ = "up";
    clients_ = 0;
    lastClientMs_ = millis();

    Serial.print(F("service: access point "));
    Serial.print(service::kSsid);
    Serial.print(F(" up at "));
    Serial.println(ip_);
}

void ServiceAp::stop(const char* why) {
    if (state_ == State::Serving) {
        WiFi.softAPdisconnect(true);
        Serial.print(F("service: access point down - "));
        Serial.println(why);
    }
    WiFi.mode(WIFI_OFF);
    state_ = State::Off;
    reason_ = why;
    clients_ = 0;
    ip_[0] = '\0';
}

uint32_t ServiceAp::armingSecondsLeft(uint32_t nowMs) const {
    if (state_ != State::Arming) {
        return 0;
    }
    const uint32_t elapsed = nowMs - stoppedSinceMs_;
    if (elapsed >= service::kArmDelayMs) {
        return 0;
    }
    return (service::kArmDelayMs - elapsed + 999) / 1000;
}

uint32_t ServiceAp::idleSecondsLeft(uint32_t nowMs) const {
    if (state_ != State::Serving || clients_ > 0) {
        return 0;
    }
    const uint32_t elapsed = nowMs - lastClientMs_;
    if (elapsed >= service::kIdleTimeoutMs) {
        return 0;
    }
    return (service::kIdleTimeoutMs - elapsed) / 1000;
}

void ServiceAp::loop(const EngineDataModel& model, uint32_t nowMs) {
    if (settings_ == nullptr) {
        return;
    }

    const bool stopped = engineStopped(model, nowMs);

    // Running the engine clears everything: the settle timer, and the latch
    // that keeps a timed-out access point from coming straight back up.
    if (!stopped) {
        stoppedSinceMs_ = nowMs;
        idleLatched_ = false;
        if (state_ != State::Off) {
            stop("engine running");
        }
        reason_ = "engine running";
        return;
    }

    if (!settings_->serviceAp) {
        if (state_ != State::Off) {
            stop("turned off in setup");
        }
        reason_ = "turned off in setup";
        return;
    }

    // Only meaningful while the ECU is actually reporting. With the link quiet
    // there is no reading to test - see kMinBatteryV.
    if (model.linkState(nowMs) != LinkState::Offline &&
        model.snapshot().batteryV < service::kMinBatteryV) {
        if (state_ != State::Off) {
            stop("battery too low");
        }
        reason_ = "battery below 12.0 V";
        return;
    }

    if (state_ == State::Off) {
        if (idleLatched_) {
            reason_ = "idle timeout; run the engine to get another window";
            return;
        }
        if (stoppedSinceMs_ == 0) {
            stoppedSinceMs_ = nowMs;
        }
        state_ = State::Arming;
        reason_ = "waiting out the settle delay";
    }

    if (state_ == State::Arming) {
        if (nowMs - stoppedSinceMs_ >= service::kArmDelayMs) {
            start();
        }
        return;
    }

    // ---- serving ----------------------------------------------------------
    if (nowMs - lastPollMs_ < kPollIntervalMs) {
        return;
    }
    lastPollMs_ = nowMs;

    clients_ = static_cast<uint8_t>(WiFi.softAPgetStationNum());
    if (clients_ > 0) {
        lastClientMs_ = nowMs;
        return;
    }

    if (nowMs - lastClientMs_ >= service::kIdleTimeoutMs) {
        idleLatched_ = true;
        stop("nobody connected");
    }
}

}  // namespace ecu

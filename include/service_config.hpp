#pragma once

#include <stdint.h>

namespace ecu {
namespace service {

// The access point the dashboard raises while the engine is stopped.
//
// Compile-time, not editable from the panel: the panel has a plus and a minus
// button and cannot enter text. The tuning laptop's wireless is dedicated to
// this, so the network is ours to name.
//
// WPA2, not open. The page serves the card's contents and accepts settings
// changes, and an open access point in a car park is an invitation.
constexpr char kSsid[] = "EcuDash";
constexpr char kPassword[] = "ecudash1";   // at least 8 characters, or softAP fails

// Engine speed has to stay below the running threshold continuously for this
// long before the radio comes up, so a blip on the tacho does not make the
// access point flap on and off.
constexpr uint32_t kArmDelayMs = 10000;

// With nobody connected, the access point gives up after this long. The engine
// is stopped, so the alternator is not charging and this is running off the
// battery.
constexpr uint32_t kIdleTimeoutMs = 15UL * 60UL * 1000UL;

// Below this the radio does not come up, and an access point already up goes
// down. Only applied when the ECU is actually reporting - with the link quiet
// there is no reading to test, and refusing on a value we do not have would
// mean the access point never appearing with the ignition off.
constexpr float kMinBatteryV = 12.0f;

}  // namespace service
}  // namespace ecu

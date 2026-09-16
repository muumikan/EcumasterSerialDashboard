#pragma once

#include <stdint.h>

namespace ecu {

// What the screens are allowed to know about the service access point.
//
// Its own header, with no dependencies, so the UI can show the state of the
// radio without including anything that knows what a radio is. The rule in
// architecture.md holds: the screens render what they are handed.
struct ServiceApStatus {
    bool serving = false;
    uint8_t clients = 0;

    // Valid whether or not the access point is up - the driver standing beside
    // the car with a laptop needs these before it comes up, not after.
    const char* ssid = "";
    const char* password = "";

    // Empty while the access point is down.
    const char* ip = "";
};

}  // namespace ecu

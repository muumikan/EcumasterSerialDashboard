#pragma once

#include <stdint.h>

// Board / wiring constants. Everything that depends on the physical board
// lives here so the layers above stay board agnostic.
namespace board {

// EMU Classic streams its data log at 19200 baud, 8N1.
constexpr uint32_t kEcuBaud = 19200;

// UART1 fed from the MAX3232 RS232-TTL adapter.
constexpr int8_t kEcuRxPin = 16;

// The ECU link is read-only, so no TX pin is attached to UART1 at all.
// This makes it physically impossible for the dashboard to talk back.
constexpr int8_t kEcuTxPin = -1;

// USB CDC / debug console. Matches monitor_speed in platformio.ini.
constexpr uint32_t kDebugBaud = 19200;

}  // namespace board

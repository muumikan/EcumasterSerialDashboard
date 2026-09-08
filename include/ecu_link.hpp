#pragma once

#include <stdint.h>

#include "board_config.hpp"

// Selects which ECU protocol the dashboard listens to.
//
// The two are mutually exclusive on one UART: they use different baud rates
// and different framing, and the EMU is configured for one or the other. The
// choice is therefore made at build time, by defining ECU_LINK_EDL.
//
// Everything above this header - model, alarms, screens, settings - is
// identical either way. Only the adapter and the baud rate change, which is
// the whole point of having an adapter layer.

#if defined(ECU_LINK_EDL)
#include "edl_serial_adapter.hpp"
#else
#include "emu_serial_adapter.hpp"
#endif

namespace ecu {

#if defined(ECU_LINK_EDL)
using EcuLinkAdapter = EdlSerialAdapter;
constexpr uint32_t kEcuLinkBaud = board::kEcuBaudEdl;
constexpr const char* kEcuLinkName = "EDL-1";
#else
using EcuLinkAdapter = EmuSerialAdapter;
constexpr uint32_t kEcuLinkBaud = board::kEcuBaud;
constexpr const char* kEcuLinkName = "classic";
#endif

}  // namespace ecu

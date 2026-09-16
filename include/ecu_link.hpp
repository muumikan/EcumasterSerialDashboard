#pragma once

#include <stdint.h>

#include "board_config.hpp"
#include "edl_serial_adapter.hpp"

// Names the ECU protocol the dashboard listens to.
//
// There was a build-time choice here between EDL-1 and the EMU's older
// 35-channel serial protocol. The car runs EDL-1, and the classic build could
// not log at all - the .emulog record *is* an EDL-1 frame - so it was a
// configuration that was compiled on every change and never once run. It is in
// the history if it is ever needed again.
//
// The seam stays. Everything above this header - model, alarms, screens,
// settings - knows only EcuLinkAdapter, which is what made swapping protocols
// cost nothing above this line, and would again.

namespace ecu {

using EcuLinkAdapter = EdlSerialAdapter;
constexpr uint32_t kEcuLinkBaud = board::kEcuBaudEdl;
constexpr const char* kEcuLinkName = "EDL-1";

}  // namespace ecu

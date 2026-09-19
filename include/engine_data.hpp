#pragma once

#include <stdint.h>

namespace ecu {

// Engineering-unit snapshot of the ECU state.
//
// Field order and units mirror the EMU serial format (EMUSerial v1.200 format
// file, as used by EMU Classic firmware 1.2xx). Nothing is derived or
// re-scaled here - the decoder already delivers engineering units - so the UI
// never has to know anything about the wire protocol.
struct EngineSnapshot {
    uint16_t rpm = 0;                 // rpm
    uint16_t mapKpa = 0;              // kPa
    uint8_t  tpsPct = 0;              // %
    int8_t   iatC = 0;                // degC
    int16_t  cltC = 0;                // degC
    uint8_t  baroKpa = 0;             // kPa
    float    batteryV = 0.0f;         // V

    float    ignAngleDeg = 0.0f;      // deg
    float    injPulseWidthMs = 0.0f;  // ms
    float    injPulseWidth2Ms = 0.0f; // ms (secondary injectors)
    float    injDutyPct = 0.0f;       // %
    float    dwellTimeMs = 0.0f;      // ms

    float    wboLambda = 0.0f;        // lambda
    float    wboAfr = 0.0f;           // AFR
    float    lambdaTarget = 0.0f;     // lambda
    float    afrTarget = 0.0f;        // AFR

    uint16_t egt1C = 0;               // degC
    uint16_t egt2C = 0;               // degC
    float    knockLevelV = 0.0f;      // V

    float    oilPressureBar = 0.0f;   // bar
    uint8_t  oilTempC = 0;            // degC
    float    fuelPressureBar = 0.0f;  // bar
    uint16_t deltaFprKpa = 0;         // kPa
    uint8_t  fuelLevelPct = 0;        // %

    float    ethanolPct = 0.0f;       // % (flex fuel)
    int8_t   ethanolTempC = 0;        // degC

    float    analogIn1V = 0.0f;       // V
    float    analogIn2V = 0.0f;       // V
    float    analogIn3V = 0.0f;       // V
    float    analogIn4V = 0.0f;       // V

    float    speedKph = 0.0f;         // km/h
    int8_t   gear = 0;                // gear number, 0 = neutral
    int8_t   ecuTempC = 0;            // degC (EMU internal)
    uint8_t  tablesSet = 0;           // active table set
    uint16_t celFlags = 0;            // check-engine bit field

    // Idle and boost control. Both are closed loops inside the ECU, and what
    // these fields carry is the loop's own working state: where it is aiming,
    // what it is driving the valve to, and how hard it is correcting.
    //
    // Only the EDL-1 stream carries them - the classic 35-channel protocol has
    // no idle or boost channel at all - so `controlChannels` says whether they
    // mean anything. Nothing here can be inferred from a zero: a closed idle
    // valve and an absent channel both read 0 %.
    bool     controlChannels = false;

    uint16_t idleTargetRpm = 0;       // rpm the idle loop is holding to
    uint8_t  idleDutyPct = 0;         // %, idle valve
    int8_t   idlePidCorrPct = 0;      // %, the PID's share of that duty
    float    idleAngleCorrDeg = 0.0f; // deg, timing trim the idle loop asks for
    bool     idleControlActive = false;  // closed loop, rather than open

    uint16_t boostTargetKpa = 0;      // kPa absolute, same scale as mapKpa
    uint8_t  boostDutyPct = 0;        // %, boost solenoid
    int8_t   boostPidCorrPct = 0;     // %, the PID's share of that duty
    int8_t   boostDcErrCorPct = 0;    // %, the error correction's share
    uint8_t  boostTableSet = 0;       // which boost table is live (not tablesSet)
};

// Below this the engine is not turning, so oil pressure, battery voltage and
// everything else that is only meaningful under load stays silent.
//
// Lives beside the snapshot rather than with the alarms because three
// different things ask the question - the alarms, the access point and the
// service page - and only one of them is an alarm.
constexpr uint16_t kEngineRunningRpm = 500;

// Health of the ECU -> dashboard link, derived from frame arrival timing.
enum class LinkState : uint8_t {
    Offline,  // nothing received (yet), or the link has dropped
    Stale,    // data was received, but not recently enough to trust
    Online,   // fresh data flowing
};

const char* toString(LinkState state);

// Check-engine bits carried in EngineSnapshot::celFlags.
//
// From Ecumaster's own paramlist "checkEngine" in the 1.211 format definition
// (docs/ecu-formats/version1_211.xml). That list numbers its entries from 1,
// so entry N sits at bit N-1. The file does not spell that out, but it is the
// only reading that works: fuelCorrections in the same file has sixteen
// entries for a sixteen-bit word, which needs value 16 to mean bit 15.
//
// Worth confirming on the car rather than trusting: unplug the intake air
// sensor and IAT should be the flag that lights.
constexpr uint8_t kCelBitCount = 11;

// Short name for a bit, or "?" if it is outside the documented range.
const char* celBitName(uint8_t bit);

}  // namespace ecu

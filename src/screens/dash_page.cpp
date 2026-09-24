#include "dash_page.hpp"

namespace ecu {

void RunPeaks::record(const EngineSnapshot& s) {
    if (s.rpm < kEngineRunningRpm) {
        return;  // key-on values would poison every minimum
    }

    const bool triggerError = s.ignError != 0;
    if (triggerError && !triggerErrorActive) {
        ++triggerErrors;
        lastTriggerError = s.ignError;
    }
    triggerErrorActive = triggerError;

    if (!seeded) {
        seeded = true;
        oilPressureMinBar = s.oilPressureBar;
        oilPressureMaxBar = s.oilPressureBar;
        fuelPressureMinBar = s.fuelPressureBar;
        fuelPressureMaxBar = s.fuelPressureBar;
        lambdaMin = s.wboLambda;
    }

    if (s.rpm > rpm) rpm = s.rpm;
    if (s.mapKpa > mapKpa) {
        mapKpa = s.mapKpa;
        baroKpa = s.baroKpa;
    }
    if (s.cltC > cltC) cltC = s.cltC;
    if (s.iatC > iatC) iatC = s.iatC;
    if (s.oilPressureBar < oilPressureMinBar) oilPressureMinBar = s.oilPressureBar;
    if (s.oilPressureBar > oilPressureMaxBar) oilPressureMaxBar = s.oilPressureBar;
    if (s.fuelPressureBar < fuelPressureMinBar) fuelPressureMinBar = s.fuelPressureBar;
    if (s.fuelPressureBar > fuelPressureMaxBar) fuelPressureMaxBar = s.fuelPressureBar;
    if (s.wboLambda < lambdaMin) lambdaMin = s.wboLambda;
    const float knock = ecu::knockRatio(s);
    if (knock > knockRatio) knockRatio = knock;
    if (s.injDutyPct > injDutyPct) injDutyPct = s.injDutyPct;
}

}  // namespace ecu

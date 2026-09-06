#include "dash_page.hpp"

namespace ecu {

void RunPeaks::record(const EngineSnapshot& s) {
    if (s.rpm <= kEngineRunningRpm) {
        return;  // key-on values would poison every minimum
    }

    if (!seeded) {
        seeded = true;
        oilPressureBar = s.oilPressureBar;
        fuelPressureBar = s.fuelPressureBar;
        lambdaMin = s.wboLambda;
    }

    if (s.rpm > rpm) rpm = s.rpm;
    if (s.mapKpa > mapKpa) mapKpa = s.mapKpa;
    if (s.cltC > cltC) cltC = s.cltC;
    if (s.iatC > iatC) iatC = s.iatC;
    if (s.oilPressureBar < oilPressureBar) oilPressureBar = s.oilPressureBar;
    if (s.fuelPressureBar < fuelPressureBar) fuelPressureBar = s.fuelPressureBar;
    if (s.wboLambda < lambdaMin) lambdaMin = s.wboLambda;
    if (s.knockLevelV > knockLevelV) knockLevelV = s.knockLevelV;
    if (s.injDutyPct > injDutyPct) injDutyPct = s.injDutyPct;
}

}  // namespace ecu

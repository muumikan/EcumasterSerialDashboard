#include "serial_report.hpp"

namespace ecu {

SerialReport::SerialReport(Print& out) : out_(out) {}

void SerialReport::update(const EngineDataModel& model, uint32_t nowMs, uint32_t intervalMs) {
    if (nowMs - lastPrintMs_ < intervalMs) {
        return;
    }

    const LinkState state = model.linkState(nowMs);
    const bool changed = model.revision() != lastRevision_ || state != lastState_;
    if (!changed) {
        return;
    }

    lastPrintMs_ = nowMs;
    lastRevision_ = model.revision();
    lastState_ = state;
    print(model, nowMs);
}

void SerialReport::print(const EngineDataModel& model, uint32_t nowMs) {
    const EngineSnapshot& s = model.snapshot();

    out_.println(F("---------------- EMU ----------------"));

    out_.print(F("link      : "));
    out_.print(toString(model.linkState(nowMs)));
    out_.print(F("  updates="));
    out_.print(model.updateCount());
    out_.print(F("  age="));
    out_.print(model.ageMs(nowMs));
    out_.println(F(" ms"));

    out_.print(F("RPM       : "));
    out_.println(s.rpm);

    out_.print(F("MAP       : "));
    out_.print(s.mapKpa);
    out_.println(F(" kPa"));

    out_.print(F("TPS       : "));
    out_.print(s.tpsPct);
    out_.println(F(" %"));

    out_.print(F("CLT / IAT : "));
    out_.print(s.cltC);
    out_.print(F(" / "));
    out_.print(s.iatC);
    out_.println(F(" C"));

    out_.print(F("Battery   : "));
    out_.print(s.batteryV, 2);
    out_.println(F(" V"));

    out_.print(F("Lambda    : "));
    out_.print(s.wboLambda, 2);
    out_.print(F("  target "));
    out_.println(s.lambdaTarget, 2);

    out_.print(F("Oil       : "));
    out_.print(s.oilPressureBar, 2);
    out_.print(F(" bar / "));
    out_.print(s.oilTempC);
    out_.println(F(" C"));

    out_.print(F("Ign / Inj : "));
    out_.print(s.ignAngleDeg, 1);
    out_.print(F(" deg / "));
    out_.print(s.injDutyPct, 1);
    out_.println(F(" % DC"));

    out_.print(F("CEL       : 0x"));
    out_.println(s.celFlags, HEX);
}

}  // namespace ecu

#include "emu_serial_adapter.hpp"

namespace ecu {

EmuSerialAdapter::EmuSerialAdapter(Stream& stream)
    : stream_(stream), emu_(stream) {}

uint32_t EmuSerialAdapter::poll() {
    const int pending = stream_.available();
    if (pending <= 0) {
        return 0;
    }

    // checkEmuSerial() drains everything that is available right now.
    emu_.checkEmuSerial();

    const uint32_t consumed = static_cast<uint32_t>(pending);
    bytesConsumed_ += consumed;
    return consumed;
}

EngineSnapshot EmuSerialAdapter::read() const {
    // Value-initialised so padding bytes are zeroed - the model compares
    // snapshots with memcmp to detect real changes.
    EngineSnapshot s{};
    const emu_data_t& d = emu_.emu_data;

    s.rpm = d.RPM;
    s.mapKpa = d.MAP;
    s.tpsPct = d.TPS;
    s.iatC = d.IAT;
    s.cltC = d.CLT;
    s.baroKpa = d.Baro;
    s.batteryV = d.Batt;

    s.ignAngleDeg = d.IgnAngle;
    s.injPulseWidthMs = d.pulseWidth;
    s.injPulseWidth2Ms = d.scondarypulseWidth;  // sic, spelling from the library
    s.injDutyPct = d.injDC;
    s.dwellTimeMs = d.dwellTime;

    s.wboLambda = d.wboLambda;
    s.wboAfr = d.wboAFR;
    s.lambdaTarget = d.lambdaTarget;
    s.afrTarget = d.afrTarget;

    s.egt1C = d.Egt1;
    s.egt2C = d.Egt2;
    s.knockLevelV = d.knockLevel;

    s.oilPressureBar = d.oilPressure;
    s.oilTempC = d.oilTemperature;
    s.fuelPressureBar = d.fuelPressure;
    s.deltaFprKpa = d.deltaFPR;
    s.fuelLevelPct = d.fuelLevel;

    s.ethanolPct = d.flexFuelEthanolContent;
    s.ethanolTempC = d.ffTemp;

    s.analogIn1V = d.analogIn1;
    s.analogIn2V = d.analogIn2;
    s.analogIn3V = d.analogIn3;
    s.analogIn4V = d.analogIn4;

    s.speedKph = d.vssSpeed;
    s.gear = d.gear;
    s.ecuTempC = d.emuTemp;
    s.tablesSet = d.tablesSet;
    s.celFlags = d.cel;

    return s;
}

}  // namespace ecu

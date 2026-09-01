#include <Arduino.h>
#include <EMUSerial.h>

HardwareSerial EcuSerial(1);

EMUSerial emu(EcuSerial);

void setup()
{
    Serial.begin(19200);

    EcuSerial.begin(
        19200,
        SERIAL_8N1,
        16, // RX
        17  // TX
    );

    Serial.println();
    Serial.println("EMU Serial Test");
}

void loop()
{
    emu.checkEmuSerial();

    static uint32_t lastPrint = 0;

    if (millis() - lastPrint > 1000)
    {
        Serial.println("----------------");

        Serial.print("MAP: ");
        Serial.println(emu.emu_data.MAP);

        Serial.print("TPS: ");
        Serial.println(emu.emu_data.TPS);

        Serial.print("CLT: ");
        Serial.println(emu.emu_data.CLT);

        Serial.print("Batt: ");
        Serial.println(emu.emu_data.Batt);

        lastPrint = millis();
    }
}
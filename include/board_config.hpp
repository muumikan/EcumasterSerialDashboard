#pragma once

#include <stdint.h>

// Board / wiring constants for the Elecrow CrowPanel Advance 3.5" HMI
// (ESP32-S3-WROOM-1-N16R8).
//
// Sources - do not guess these, they are taken from Elecrow's own material:
//   Wiki:  https://www.elecrow.com/wiki/CrowPanel_Advance_3.5-HMI_ESP32_AI_Display.html
//   Demos: github.com/Elecrow-RD/CrowPanel-Advance-3.5-HMI-ESP32-S3-AI-Powered-
//          IPS-Touch-Screen-480x320  (example/V1.0/Arduino/lesson-03 and -05)
//
// Everything that depends on the physical board lives here so the layers
// above stay board agnostic.
namespace board {

// ---------------------------------------------------------------- ECU link --
// EMU Classic streams its data log at 19200 baud, 8N1.
constexpr uint32_t kEcuBaud = 19200;

// UART1-OUT connector on the CrowPanel, fed from the MAX3232 RS232-TTL
// adapter. NOTE: GPIO16 (used on the previous esp32dev bring-up board) is the
// GT911 touch I2C clock here and must not be reused for the ECU.
constexpr int8_t kEcuRxPin = 18;  // UART1-OUT RX

// The ECU link is read-only, so no TX pin is attached to UART1 at all.
// The connector's TX line is GPIO17; leaving it unassigned makes it
// physically impossible for the dashboard to talk back.
constexpr int8_t kEcuTxPin = -1;

// ----------------------------------------------------------------- Console --
// USB CDC. Matches monitor_speed in platformio.ini.
constexpr uint32_t kDebugBaud = 115200;

// ----------------------------------------------------------------- Display --
// ILI9488 on SPI2_HOST. The panel itself is 320x480 portrait; offset_rotation
// 3 presents it as 480x320 landscape.
constexpr uint16_t kLcdWidth = 480;
constexpr uint16_t kLcdHeight = 320;
constexpr uint16_t kPanelWidth = 320;
constexpr uint16_t kPanelHeight = 480;
constexpr uint8_t kPanelRotation = 3;

constexpr int8_t kLcdSclkPin = 42;
constexpr int8_t kLcdMosiPin = 39;
constexpr int8_t kLcdMisoPin = -1;
constexpr int8_t kLcdDcPin = 41;
constexpr int8_t kLcdCsPin = 40;
constexpr int8_t kLcdRstPin = 2;
constexpr int8_t kLcdBacklightPin = 38;

constexpr uint32_t kLcdSpiWriteHz = 40000000;
constexpr uint32_t kLcdSpiReadHz = 16000000;

// ------------------------------------------------------------------- Touch --
// GT911 capacitive controller. Shares I2C0 with the on-board RTC.
constexpr int8_t kTouchSdaPin = 15;
constexpr int8_t kTouchSclPin = 16;
constexpr int8_t kTouchIntPin = 47;
constexpr int8_t kTouchRstPin = 48;
constexpr uint8_t kTouchI2cAddr = 0x14;  // alternative: 0x5D
constexpr uint32_t kTouchI2cHz = 400000;

}  // namespace board

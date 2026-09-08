# Pin assignment — CrowPanel Advance 3.5" HMI

Board: Elecrow CrowPanel Advance 3.5" HMI, ESP32-S3-WROOM-1-N16R8
(16 MB QIO flash, 8 MB octal PSRAM).

These numbers are transcribed from Elecrow's own material, not guessed:

- [Elecrow Wiki — CrowPanel Advance 3.5-HMI](https://www.elecrow.com/wiki/CrowPanel_Advance_3.5-HMI_ESP32_AI_Display.html)
- [Elecrow-RD demo repository](https://github.com/Elecrow-RD/CrowPanel-Advance-3.5-HMI-ESP32-S3-AI-Powered-IPS-Touch-Screen-480x320),
  `example/V1.0/Arduino/lesson-03` (`LovyanGFX_Driver.h`, `touch.h`) and
  `lesson-05` (expansion port)

The firmware's copy of this table is [`include/board_config.hpp`](../include/board_config.hpp).
Change it there; this file documents it.

## Used by this project

| GPIO | Function | Notes |
|---|---|---|
| 18 | **ECU serial RX** | UART1-OUT connector RX, fed from the MAX3232 |
| — | ECU serial TX | Deliberately unassigned. The connector's TX is GPIO17. |
| 42 | LCD SCLK | ILI9488 on SPI2_HOST, 40 MHz write |
| 39 | LCD MOSI | |
| 41 | LCD DC | |
| 40 | LCD CS | |
| 2 | LCD RST | |
| 38 | LCD backlight | LEDC PWM, channel 0, 5 kHz / 8 bit — brightness and night mode |
| 15 | Touch I2C SDA | GT911, shared with the on-board RTC |
| 16 | Touch I2C SCL | **See the conflict note below** |
| 47 | Touch INT | |
| 48 | Touch RST | |

GT911 address is `0x14`; the wiki also lists `0x5D`. If touch does not
respond, try the other one — `board::kTouchI2cAddr`.

The panel is physically 320 × 480 portrait. `offset_rotation = 3` presents it
as 480 × 320 landscape, which is what the UI is laid out for.

## Conflict note: GPIO16

**GPIO16 is the GT911 touch I2C clock on this board.** The first bring-up ran
on a generic esp32dev board with the ECU RX on GPIO16, which is fine there but
collides with touch here. When the project moved to the CrowPanel the ECU
input was re-pinned to GPIO18.

If you are following older notes or the original `main.cpp`, this is the wire
that has to move.

## Present on the board, unused by this project

| GPIO | Function |
|---|---|
| 6 / 4 / 5 | SD card MOSI / MISO / CLK |
| 11 / 13 / 12 | Speaker I2S LRCLK / BCLK / SDIN |
| 10 / 3 / 9 | Microphone SD / WS / CLK |
| 8 | Buzzer |
| 45 | Mode select: microphone vs. wireless module |
| 44 / 43 | UART0-IN connector RX / TX |
| 17 | UART1-OUT connector TX |

GPIO 33–37 are consumed by the octal PSRAM and are not available.

## Console

The debug console is USB CDC at 115200 baud over the USB-C connector
(`ARDUINO_USB_CDC_ON_BOOT=1`). This is separate from the ECU link, which runs
at 19200 on UART1.

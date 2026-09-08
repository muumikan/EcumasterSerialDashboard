#include "display.hpp"

#include <Arduino.h>
#include <LovyanGFX.hpp>
#include <driver/i2c.h>
#include <esp_heap_caps.h>
#include <lvgl.h>

#include "board_config.hpp"

namespace display {
namespace {

// Panel wiring, transcribed from Elecrow's LovyanGFX_Driver.h for this board.
// See board_config.hpp for the source links.
class CrowPanelLgfx : public lgfx::LGFX_Device {
public:
    CrowPanelLgfx() {
        {
            auto cfg = bus_.config();
            cfg.spi_host = SPI2_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = board::kLcdSpiWriteHz;
            cfg.freq_read = board::kLcdSpiReadHz;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = board::kLcdSclkPin;
            cfg.pin_mosi = board::kLcdMosiPin;
            cfg.pin_miso = board::kLcdMisoPin;
            cfg.pin_dc = board::kLcdDcPin;
            bus_.config(cfg);
            panel_.setBus(&bus_);
        }
        {
            auto cfg = panel_.config();
            cfg.pin_cs = board::kLcdCsPin;
            cfg.pin_rst = board::kLcdRstPin;
            cfg.pin_busy = -1;
            cfg.memory_width = board::kPanelWidth;
            cfg.memory_height = board::kPanelHeight;
            cfg.panel_width = board::kPanelWidth;
            cfg.panel_height = board::kPanelHeight;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = board::kPanelRotation;
            cfg.dummy_read_pixel = 8;
            cfg.dummy_read_bits = 1;
            cfg.readable = false;
            cfg.invert = true;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = true;
            panel_.config(cfg);
        }
        {
            auto cfg = touch_.config();
            cfg.x_min = 0;
            cfg.x_max = board::kPanelWidth - 1;
            cfg.y_min = 0;
            cfg.y_max = board::kPanelHeight - 1;
            cfg.pin_int = board::kTouchIntPin;
            cfg.pin_rst = board::kTouchRstPin;
            cfg.bus_shared = false;
            cfg.offset_rotation = 0;
            cfg.i2c_port = I2C_NUM_0;
            cfg.pin_sda = board::kTouchSdaPin;
            cfg.pin_scl = board::kTouchSclPin;
            cfg.freq = board::kTouchI2cHz;
            cfg.i2c_addr = board::kTouchI2cAddr;
            touch_.config(cfg);
            panel_.setTouch(&touch_);
        }
        setPanel(&panel_);
    }

private:
    lgfx::Panel_ILI9488 panel_;
    lgfx::Bus_SPI bus_;
    lgfx::Touch_GT911 touch_;
};

CrowPanelLgfx gfx;

// Two partial buffers in internal DMA-capable RAM. Full-screen buffers would
// have to live in PSRAM, which flushes noticeably slower.
// Backlight PWM. 5 kHz is well above anything the eye or a phone camera picks
// up, and 8 bits is finer than the panel's usable range anyway.
constexpr uint8_t kBacklightChannel = 0;
constexpr uint32_t kBacklightFreqHz = 5000;
constexpr uint8_t kBacklightBits = 8;

constexpr uint32_t kBufferLines = 40;
constexpr uint32_t kBufferPixels = board::kLcdWidth * kBufferLines;

lv_disp_draw_buf_t drawBuf;
lv_color_t* buf0 = nullptr;
lv_color_t* buf1 = nullptr;

void flushCb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* pixels) {
    if (gfx.getStartCount() > 0) {
        gfx.endWrite();
    }
    gfx.pushImageDMA(area->x1,
                     area->y1,
                     area->x2 - area->x1 + 1,
                     area->y2 - area->y1 + 1,
                     reinterpret_cast<lgfx::rgb565_t*>(pixels));
    lv_disp_flush_ready(drv);
}

void touchCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    uint16_t x = 0;
    uint16_t y = 0;

    if (gfx.getTouch(&x, &y)) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

}  // namespace

bool begin() {
    gfx.init();
    gfx.initDMA();
    gfx.startWrite();
    gfx.fillScreen(TFT_BLACK);

    // Backlight on only after the panel is cleared, to avoid a flash of noise.
    ledcSetup(kBacklightChannel, kBacklightFreqHz, kBacklightBits);
    ledcAttachPin(board::kLcdBacklightPin, kBacklightChannel);
    setBrightness(100);

    lv_init();

    const size_t bytes = kBufferPixels * sizeof(lv_color_t);
    buf0 = static_cast<lv_color_t*>(heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    buf1 = static_cast<lv_color_t*>(heap_caps_malloc(bytes, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL));
    if (buf0 == nullptr || buf1 == nullptr) {
        return false;
    }

    lv_disp_draw_buf_init(&drawBuf, buf0, buf1, kBufferPixels);

    static lv_disp_drv_t dispDrv;
    lv_disp_drv_init(&dispDrv);
    dispDrv.hor_res = board::kLcdWidth;
    dispDrv.ver_res = board::kLcdHeight;
    dispDrv.flush_cb = flushCb;
    dispDrv.draw_buf = &drawBuf;
    lv_disp_drv_register(&dispDrv);

    static lv_indev_drv_t indevDrv;
    lv_indev_drv_init(&indevDrv);
    indevDrv.type = LV_INDEV_TYPE_POINTER;
    indevDrv.read_cb = touchCb;
    lv_indev_drv_register(&indevDrv);

    return true;
}

void loop() {
    lv_timer_handler();
}

void setBrightness(uint8_t percent) {
    if (percent > 100) {
        percent = 100;
    }
    const uint32_t duty = (static_cast<uint32_t>(percent) * 255u) / 100u;
    ledcWrite(kBacklightChannel, duty);
}

}  // namespace display

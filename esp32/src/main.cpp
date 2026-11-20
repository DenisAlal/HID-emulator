#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>
#include <LovyanGFX.hpp>

#include "config.h"
#include "ui.h"

// ============================================================
//  Драйвер дисплея + тача (LovyanGFX под ESP32-2432S028)
// ============================================================
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_ILI9341 _panel;
    lgfx::Bus_SPI _bus;
    lgfx::Light_PWM _light;
    lgfx::Touch_XPT2046 _touch;

public:
    LGFX() {
        {
            auto cfg = _bus.config();
            cfg.spi_host = VSPI_HOST;
            cfg.spi_mode = 0;
            cfg.freq_write = 27000000;   // при помехах можно снизить до 20000000
            cfg.freq_read = 16000000;
            cfg.spi_3wire = false;
            cfg.use_lock = true;
            cfg.dma_channel = SPI_DMA_CH_AUTO;
            cfg.pin_sclk = TFT_SCLK;
            cfg.pin_mosi = TFT_MOSI;
            cfg.pin_miso = TFT_MISO;
            cfg.pin_dc = TFT_DC;
            _bus.config(cfg);
            _panel.setBus(&_bus);
        }
        {
            auto cfg = _panel.config();
            cfg.pin_cs = TFT_CS;
            cfg.pin_rst = TFT_RST;
            cfg.pin_busy = -1;
            cfg.memory_width = 240;
            cfg.memory_height = 320;
            cfg.panel_width = 240;
            cfg.panel_height = 320;
            cfg.offset_x = 0;
            cfg.offset_y = 0;
            cfg.offset_rotation = 1;   // поворот в альбомную (320x240)
            cfg.readable = true;
            cfg.invert = false;
            cfg.rgb_order = false;
            cfg.dlen_16bit = false;
            cfg.bus_shared = false;
            _panel.config(cfg);
        }
        {
            auto cfg = _light.config();
            cfg.pin_bl = TFT_BL;
            cfg.invert = false;
            cfg.freq = 12000;
            cfg.pwm_channel = 7;
            _light.config(cfg);
            _panel.setLight(&_light);
        }
        {
            auto cfg = _touch.config();
            cfg.x_min = TOUCH_X_MIN;
            cfg.x_max = TOUCH_X_MAX;
            cfg.y_min = TOUCH_Y_MIN;
            cfg.y_max = TOUCH_Y_MAX;
            cfg.pin_int = -1;   // без IRQ — опрос (надёжнее)
            cfg.bus_shared = false;
            cfg.offset_rotation = 2;   // 180° (компенсирует инверсию X_MIN/MAX)
            cfg.spi_host = HSPI_HOST;
            cfg.freq = 1000000;
            cfg.pin_sclk = TOUCH_CLK;
            cfg.pin_mosi = TOUCH_MOSI;
            cfg.pin_miso = TOUCH_MISO;
            cfg.pin_cs = TOUCH_CS;
            _touch.config(cfg);
            _panel.setTouch(&_touch);
        }
        setPanel(&_panel);
    }
};

static LGFX tft;

// Яркость подсветки (0..100). Используется из ui.cpp.
void set_backlight(uint8_t pct) {
    int v = (int)pct * 255 / 100;
    if (v < 8) v = 8;   // не гасим в ноль
    tft.setBrightness(v);
}

// Последние считанные координаты тача (для отладочного оверлея).
uint16_t g_touch_x = 0;
uint16_t g_touch_y = 0;
bool g_touch_pressed = false;

// ============================================================
//  LVGL: буфер, flush, тач
// ============================================================
#define BUF_ROWS 10
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[320 * BUF_ROWS];

static void my_disp_flush(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* color_p) {
    uint32_t w = area->x2 - area->x1 + 1;
    uint32_t h = area->y2 - area->y1 + 1;
    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushPixels((uint16_t*)color_p, w * h);
    tft.endWrite();
    lv_disp_flush_ready(drv);
}

static void my_touchpad_read(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    uint16_t x = 0, y = 0;
    bool pressed = tft.getTouch(&x, &y);
    g_touch_x = x;
    g_touch_y = y;
    g_touch_pressed = pressed;
    data->state = pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    if (pressed) {
        data->point.x = x;
        data->point.y = y;
    }
}

// ============================================================
//  Wi-Fi
// ============================================================
static bool connect_wifi() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
        delay(100);
    }
    return WiFi.status() == WL_CONNECTED;
}

// ============================================================
//  Setup / Loop
// ============================================================
void setup() {
    Serial.begin(115200);

    tft.init();
    tft.setBrightness(200);

    lv_init();
    lv_disp_draw_buf_init(&draw_buf, buf, nullptr, 320 * BUF_ROWS);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

    ui_init();

#if TOUCH_DEBUG
    ui_touch_debug_init();
#endif

    bool ok = connect_wifi();
    ui_set_wifi(ok, WiFi.RSSI());
    if (ok) {
        ui_refresh_keys();
    } else {
        ui_show_toast("No WiFi");
    }
}

void loop() {
    lv_timer_handler();
#if TOUCH_DEBUG
    ui_touch_debug_update(g_touch_x, g_touch_y, g_touch_pressed);
#endif
    delay(5);
}

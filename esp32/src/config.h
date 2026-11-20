#pragma once

// ============================================================
//  Плата ESP32-2432S028 (Cheap Yellow Display)
//  Здесь все «крутилки» под конкретную плату и сеть.
// ============================================================

// ---- Дисплей ILI9341 (SPI) ----
#define TFT_SCLK  14
#define TFT_MOSI  13
#define TFT_MISO  12
#define TFT_CS    15
#define TFT_DC    2
#define TFT_RST   -1     // без отдельного пина сброса
#define TFT_BL    21     // подсветка (ШИМ)

// ---- Тач XPT2046 (отдельный SPI) ----
#define TOUCH_CLK  25
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CS   33
#define TOUCH_IRQ  36

// ---- Wi-Fi ----
#define WIFI_SSID "SSID"
#define WIFI_PASS "PASS"

// ---- Сервер (HID Emulator на ПК) ----
#define SERVER_HOST "0.0.0.0"
#define SERVER_PORT 8080
// Должен совпадать с "token" в config.json на сервере.
#define SERVER_TOKEN "secret"

// ============================================================
//  Калибровка тача XPT2046 (12-битный АЦП, сырой диапазон 0..4095).
//  Начни с 0..4095; точные границы подберём по отладочному оверлею.
// ============================================================
#define TOUCH_X_MIN 4095
#define TOUCH_X_MAX 0
#define TOUCH_Y_MIN 0
#define TOUCH_Y_MAX 4095

// Отладочный оверлей тача (точки по углам + метка + координаты).
// Поставь 0, когда тач откалибруем.
#define TOUCH_DEBUG 1

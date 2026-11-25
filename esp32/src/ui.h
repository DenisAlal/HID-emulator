#pragma once
#include <Arduino.h>

// Инициализация UI (вызывается один раз после lv_init).
void ui_init();

// Обновляет индикатор Wi-Fi в статус-баре.
void ui_set_wifi(bool connected, int rssi);

// Тянет /keys с сервера и перерисовывает сетку кнопок.
void ui_refresh_keys();

// Индикатор последнего HTTP-запроса в верхней панели: true = зелёный, false = красный.
void ui_set_status(bool ok);

// Отладочный оверлей тача (для калибровки).
void ui_touch_debug_init();
void ui_touch_debug_update(uint16_t x, uint16_t y, bool pressed);

// Настройки.
void ui_show_settings();          // открыть экран настроек
void ui_hide_settings();          // вернуться на главный экран

// Погода: переключить страницу и обновлять её по таймеру (вызывать из loop).
void ui_show_weather(bool show);
void ui_weather_tick();

// Метрики ПК: переключить страницу и обновлять её по таймеру.
void ui_show_stats(bool show);
void ui_stats_tick();

// Яркость подсветки (0..100). Определена в main.cpp (там доступ к tft).
void set_backlight(uint8_t pct);

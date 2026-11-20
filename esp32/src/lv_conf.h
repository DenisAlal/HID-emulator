// Минимальная конфигурация LVGL 8.3.
// Остальные параметры берутся из значений по умолчанию (lv_conf_internal.h).

#ifndef LV_CONF_H
#define LV_CONF_H

// 16-битный цвет (RGB565) — под ILI9341.
#define LV_COLOR_DEPTH 16

// Тик от millis() (Arduino).
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

#define LV_USE_LOG 0

// Шрифты (14 — по умолчанию, 16/20 — для покрупнее текста).
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1

// Если цвета «перевёрнуты» (зелёный оттенок, мусорные цвета) — поменяй на 0.
#define LV_COLOR_16_SWAP 1

#endif // LV_CONF_H

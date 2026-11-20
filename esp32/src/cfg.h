#pragma once
#include <Arduino.h>

// Runtime-конфиг: defaults из config.h, поверх — значения из NVS
// (то, что пользователь вводит в настройках на устройстве).
struct AppConfig {
    String   ssid;
    String   pass;
    String   host;
    uint16_t port;
    String   token;
    bool     dark;
    uint8_t  brightness;
};

void cfg_load();      // загрузить из NVS (вызывать в setup ДО ui_init)
AppConfig& cfg();     // текущие значения
void cfg_save();      // сохранить в NVS

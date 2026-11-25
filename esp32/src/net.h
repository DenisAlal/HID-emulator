#pragma once
#include <Arduino.h>

// Одна кнопка (приходит с сервера через GET /keys).
struct KeyDef {
    String id;     // идентификатор для POST /press?id=...
    String label;  // название
    String combo;  // комбинация (справочно, сама плата её не шлёт)
};

// Загружает список кнопок с сервера в массив out.
// Возвращает true при успехе, заполняет count.
bool netFetchKeys(KeyDef* out, int maxCount, int& count);

// Отправляет нажатие кнопки по id. Возвращает true при успехе.
bool netPressKey(const String& id);

// Погода (Open-Meteo, HTTP, без ключа).
struct WeatherDay {
    String date;   // "24.11"
    int    code;   // WMO weather code
    float  tmin;
    float  tmax;
};

struct WeatherData {
    bool     valid;
    float    temp;
    float    feels;
    float    wind;
    int      humidity;
    int      code;
    int      dayCount;
    uint32_t updatedMs;
    WeatherDay days[7];
};

bool netFetchWeather(WeatherData& out);

// Метрики ПК (GET /stats с сервера).
struct PcStats {
    bool  ok;
    bool  cpuTempValid;
    bool  gpuValid;
    bool  gpuTempValid;
    float cpuLoad;
    float cpuTemp;
    bool  cpuFreqValid;
    float cpuFreqMHz;
    float gpuLoad;
    float gpuTemp;
    int   vramUsed;
    int   vramTotal;
    int   ramUsed;
    int   ramTotal;
    float ramPercent;
    bool  showCpuLoad;
    bool  showCpuTemp;
    bool  showGpuLoad;
    bool  showGpuTemp;
    bool  showVram;
    bool  showRam;
    uint32_t updatedMs;
};

bool netFetchStats(PcStats& out);

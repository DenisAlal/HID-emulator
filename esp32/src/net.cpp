#include "net.h"
#include "cfg.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

static String baseUrl() {
    return "http://" + cfg().host + ":" + String((unsigned)cfg().port);
}

bool netFetchKeys(KeyDef* out, int maxCount, int& count) {
    count = 0;

    HTTPClient http;
    http.begin(baseUrl() + "/keys");
    http.addHeader("X-Key", cfg().token);
    http.setTimeout(5000);

    int code = http.GET();
    if (code != 200) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(24576);
    DeserializationError err = deserializeJson(doc, payload);
    if (err) return false;

    JsonArray arr = doc["keys"].as<JsonArray>();
    if (arr.isNull()) return false;

    for (JsonObject o : arr) {
        if (count >= maxCount) break;
        out[count].id = o["id"].as<String>();
        out[count].label = o["label"].as<String>();
        out[count].combo = o["combo"].as<String>();
        count++;
    }
    return true;
}

bool netPressKey(const String& id) {
    HTTPClient http;
    http.begin(baseUrl() + "/press?id=" + id);
    http.addHeader("X-Key", cfg().token);
    http.setTimeout(5000);

    int code = http.POST("");
    http.end();

    return code == 200;
}

bool netFetchWeather(WeatherData& out) {
    static const char* kUrl =
        "http://api.open-meteo.com/v1/forecast"
        "?latitude=55.03&longitude=82.92"
        "&current=temperature_2m,relative_humidity_2m,apparent_temperature,weather_code,wind_speed_10m"
        "&daily=weather_code,temperature_2m_max,temperature_2m_min"
        "&timezone=auto&forecast_days=7&wind_speed_unit=ms";

    HTTPClient http;
    http.begin(kUrl);
    http.setTimeout(8000);

    int code = http.GET();
    if (code != 200) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(6144);
    if (deserializeJson(doc, payload)) return false;

    JsonObject cur = doc["current"];
    if (cur.isNull()) return false;

    out.temp     = cur["temperature_2m"] | 0.0f;
    out.feels    = cur["apparent_temperature"] | 0.0f;
    out.humidity = cur["relative_humidity_2m"] | 0;
    out.wind     = cur["wind_speed_10m"] | 0.0f;
    out.code     = cur["weather_code"] | 0;

    JsonArray times = doc["daily"]["time"];
    JsonArray codes = doc["daily"]["weather_code"];
    JsonArray tmax  = doc["daily"]["temperature_2m_max"];
    JsonArray tmin  = doc["daily"]["temperature_2m_min"];

    int n = times.size();
    if (n > 7) n = 7;
    out.dayCount = 0;
    for (int i = 0; i < n; i++) {
        String d = times[i].as<String>();
        if (d.length() >= 10) {
            out.days[i].date = d.substring(8, 10) + "." + d.substring(5, 7);
        } else {
            out.days[i].date = d;
        }
        out.days[i].code = codes[i] | 0;
        out.days[i].tmax = tmax[i] | 0.0f;
        out.days[i].tmin = tmin[i] | 0.0f;
        out.dayCount++;
    }

    out.valid = true;
    out.updatedMs = millis();
    return true;
}

bool netFetchStats(PcStats& out) {
    HTTPClient http;
    http.begin(baseUrl() + "/stats");
    http.addHeader("X-Key", cfg().token);
    http.setTimeout(2000);

    int code = http.GET();
    if (code != 200) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    DynamicJsonDocument doc(4096);
    if (deserializeJson(doc, payload)) return false;

    out.ok = doc["ok"] | false;
    out.cpuLoad = doc["cpu"]["load"] | 0.0f;

    JsonVariant ct = doc["cpu"]["temp"];
    out.cpuTempValid = !ct.isNull();
    out.cpuTemp = ct | 0.0f;

    JsonVariant cf = doc["cpu"]["freq"];
    out.cpuFreqValid = !cf.isNull();
    out.cpuFreqMHz = cf | 0.0f;

    JsonVariant g = doc["gpu"];
    out.gpuValid = !g.isNull();
    out.gpuLoad = 0.0f;
    out.gpuTemp = 0.0f;
    out.gpuTempValid = false;
    out.vramUsed = 0;
    out.vramTotal = 0;
    if (out.gpuValid) {
        out.gpuLoad = g["load"] | 0.0f;
        JsonVariant gt = g["temp"];
        out.gpuTempValid = !gt.isNull();
        out.gpuTemp = gt | 0.0f;
        out.vramUsed = g["vram_used"] | 0;
        out.vramTotal = g["vram_total"] | 0;
    }

    out.ramUsed = doc["ram"]["used"] | 0;
    out.ramTotal = doc["ram"]["total"] | 0;
    out.ramPercent = doc["ram"]["percent"] | 0.0f;

    JsonObject show = doc["show"];
    out.showCpuLoad = show["cpu_load"] | true;
    out.showCpuTemp = show["cpu_temp"] | true;
    out.showGpuLoad = show["gpu_load"] | true;
    out.showGpuTemp = show["gpu_temp"] | true;
    out.showVram    = show["vram"] | true;
    out.showRam     = show["ram"] | true;

    out.updatedMs = millis();
    return true;
}

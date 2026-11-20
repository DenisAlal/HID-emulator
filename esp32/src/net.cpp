#include "net.h"
#include "config.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>

static String baseUrl() {
    return "http://" + String(SERVER_HOST) + ":" + String(SERVER_PORT);
}

bool netFetchKeys(KeyDef* out, int maxCount, int& count) {
    count = 0;

    HTTPClient http;
    http.begin(baseUrl() + "/keys");
    http.addHeader("X-Key", SERVER_TOKEN);
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
    http.addHeader("X-Key", SERVER_TOKEN);
    http.setTimeout(5000);

    int code = http.POST("");
    http.end();

    return code == 200;
}

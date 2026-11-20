#include "wifi_mgr.h"
#include "cfg.h"
#include "ui.h"
#include <WiFi.h>

static bool     s_connecting = false;
static bool     s_reported   = false;
static uint32_t s_start      = 0;
static uint32_t s_last_rssi  = 0;

void wifi_connect() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(50);
    WiFi.begin(cfg().ssid.c_str(), cfg().pass.c_str());
    s_connecting = true;
    s_reported   = false;
    s_start      = millis();
    ui_set_wifi(false, 0);
}

bool wifi_connected() { return WiFi.status() == WL_CONNECTED; }

void wifi_tick() {
    if (WiFi.status() == WL_CONNECTED) {
        if (!s_reported) {
            s_reported   = true;
            s_connecting = false;
            ui_set_wifi(true, WiFi.RSSI());
            ui_refresh_keys();
        } else if (millis() - s_last_rssi > 5000) {
            s_last_rssi = millis();
            ui_set_wifi(true, WiFi.RSSI());
        }
        return;
    }

    if (s_reported) {
        s_reported = false;
        ui_set_wifi(false, 0);
    }
    if (s_connecting && millis() - s_start > 15000) {
        s_connecting = false;
        ui_set_status(false);
    }
}

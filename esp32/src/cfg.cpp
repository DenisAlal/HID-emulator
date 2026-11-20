#include "cfg.h"
#include "config.h"
#include <Preferences.h>

static AppConfig   g_cfg;
static Preferences g_prefs;

void cfg_load() {
    g_prefs.begin("hidemu", false);
    g_cfg.ssid  = g_prefs.getString("ssid",  WIFI_SSID);
    g_cfg.pass  = g_prefs.getString("pass",  WIFI_PASS);
    g_cfg.host  = g_prefs.getString("host",  SERVER_HOST);
    g_cfg.port  = g_prefs.getUShort("port",  SERVER_PORT);
    g_cfg.token = g_prefs.getString("token", SERVER_TOKEN);
    g_cfg.dark  = g_prefs.getBool("dark", true);
    g_cfg.brightness = g_prefs.getUChar("bright", 100);
}

AppConfig& cfg() { return g_cfg; }

void cfg_save() {
    g_prefs.putString("ssid",  g_cfg.ssid);
    g_prefs.putString("pass",  g_cfg.pass);
    g_prefs.putString("host",  g_cfg.host);
    g_prefs.putUShort("port",  g_cfg.port);
    g_prefs.putString("token", g_cfg.token);
    g_prefs.putBool("dark",    g_cfg.dark);
    g_prefs.putUChar("bright", g_cfg.brightness);
}

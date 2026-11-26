#include "ui.h"
#include "net.h"
#include "cfg.h"
#include "wifi_mgr.h"
#include <lvgl.h>

// ---- Тема ----
static uint32_t col_bg()   { return cfg().dark ? 0x151515 : 0xF2F3F5; }
static uint32_t col_bar()  { return cfg().dark ? 0x1E1E1E : 0xE2E4E8; }
static uint32_t col_btn()  { return cfg().dark ? 0x2A2A2A : 0xFFFFFF; }
static uint32_t col_text() { return cfg().dark ? 0xFFFFFF : 0x1B1B1B; }
static uint32_t col_dim()  { return cfg().dark ? 0x9E9E9E : 0x6B6B6B; }
static uint32_t col_sel()  { return cfg().dark ? 0x4F8CFF : 0x3A6FD8; }
static uint32_t col_track() { return cfg().dark ? 0x3A3A3A : 0xD5D8DD; }

#define C_GOOD 0x2ECC71
#define C_WARN 0xF1C40F
#define C_BAD  0xE74C3C
#define MAX_KEYS 48

static KeyDef g_keys[MAX_KEYS];
static int  g_count = 0;
static bool g_wifi_connected = false;
static int  g_wifi_rssi = 0;

enum View { VIEW_KEYS, VIEW_WEATHER, VIEW_STATS };

// главный экран
static lv_obj_t* g_scr = nullptr;
static lv_obj_t* g_bar = nullptr;
static lv_obj_t* g_grid = nullptr;
static lv_obj_t* g_wifi = nullptr;
static lv_obj_t* g_dot = nullptr;
static lv_obj_t* g_btn_refresh = nullptr;
static lv_obj_t* g_btn_settings = nullptr;
static lv_obj_t* g_lbl_refresh = nullptr;
static lv_obj_t* g_lbl_settings = nullptr;

// постраничный список
#define PAGE_SIZE 6
static lv_obj_t* g_slot_btn[PAGE_SIZE];
static lv_obj_t* g_slot_lbl[PAGE_SIZE];
static lv_obj_t* g_bottom = nullptr;
static lv_obj_t* g_stop_btn = nullptr;
static lv_obj_t* g_prev_btn = nullptr;
static lv_obj_t* g_next_btn = nullptr;
static lv_obj_t* g_page_lbl = nullptr;
static lv_obj_t* g_empty_lbl = nullptr;
static int g_page = 0;
static int g_pages = 1;
static int g_stop_idx = -1;
static int g_song_count = 0;
static int g_page_keys[PAGE_SIZE];

// погода
static lv_obj_t* g_btn_weather = nullptr;
static lv_obj_t* g_lbl_weather = nullptr;
static lv_obj_t* g_btn_stats = nullptr;
static lv_obj_t* g_lbl_stats = nullptr;
static View      g_view = VIEW_KEYS;
static lv_obj_t* g_wx_view = nullptr;
static lv_obj_t* g_wx_icon = nullptr;
static lv_obj_t* g_wx_temp = nullptr;
static lv_obj_t* g_wx_city = nullptr;
static lv_obj_t* g_wx_feels = nullptr;
static lv_obj_t* g_wx_hum = nullptr;
static lv_obj_t* g_wx_wind = nullptr;
static lv_obj_t* g_wx_days = nullptr;
static lv_obj_t* g_wx_msg = nullptr;
static WeatherData g_wx;
static uint32_t g_wx_last_try = 0;
static bool g_wx_busy = false;

// метрики ПК
struct Ring {
    lv_obj_t* box = nullptr;
    lv_obj_t* arc = nullptr;
    lv_obj_t* val = nullptr;
    lv_obj_t* sub = nullptr;
};

static lv_obj_t* g_stats_view = nullptr;
static lv_obj_t* g_stats_grid = nullptr;
static lv_obj_t* g_stats_msg = nullptr;
static lv_obj_t* g_stats_retry = nullptr;
static bool g_stats_offline = false;
static Ring g_ring_cpu_load;
static Ring g_ring_cpu_temp;
static Ring g_ring_ram;
static Ring g_ring_gpu_load;
static Ring g_ring_gpu_temp;
static Ring g_ring_vram;
static PcStats g_pc;
static uint32_t g_stats_last_try = 0;
static bool g_stats_busy = false;

// настройки
static lv_obj_t* g_scr_settings = nullptr;
static lv_obj_t* g_switch_theme = nullptr;
static lv_obj_t* g_slider_bright = nullptr;
static lv_obj_t* g_label_bright = nullptr;
static lv_obj_t* g_s_lbl_back = nullptr;
static lv_obj_t* g_s_btn_save = nullptr;
static lv_obj_t* g_s_btn_reload = nullptr;
static lv_obj_t* g_val_ssid = nullptr;
static lv_obj_t* g_val_pass = nullptr;
static lv_obj_t* g_val_host = nullptr;
static lv_obj_t* g_val_port = nullptr;
static lv_obj_t* g_val_token = nullptr;

// ввод текста
static lv_obj_t* g_scr_input = nullptr;
static lv_obj_t* g_ta_input = nullptr;
static lv_obj_t* g_kb_input = nullptr;
static lv_obj_t* g_i_lbl_title = nullptr;

enum Field { F_SSID, F_PASS, F_HOST, F_PORT, F_TOKEN };
static Field g_editing = F_SSID;
static String g_applied_ssid, g_applied_pass;

// общие стили (настройки/ввод)
static lv_style_t st_bg, st_bar, st_card, st_text, st_dim;

static void rebuild_pages();
static void show_page(int p);
static void build_weather_view();
static void rebuild_weather();
static void ui_weather_refresh();
static void build_stats_view();
static void ui_stats_refresh();
static void apply_stats_show();
static void apply_theme();
static void update_values();
static void open_editor(Field f, const char* title);
static void input_apply();
static void input_cancel();

// ---------------- Обработчики ----------------
static void slot_cb(lv_event_t* e) {
    int slot = (int)(intptr_t)lv_event_get_user_data(e);
    if (slot < 0 || slot >= PAGE_SIZE) return;
    int idx = g_page_keys[slot];
    if (idx < 0 || idx >= g_count) return;
    bool ok = netPressKey(g_keys[idx].id);
    ui_set_status(ok);
}

static void stop_cb(lv_event_t* e) {
    if (g_stop_idx < 0 || g_stop_idx >= g_count) return;
    bool ok = netPressKey(g_keys[g_stop_idx].id);
    ui_set_status(ok);
}

static void prev_cb(lv_event_t* e) { if (g_page > 0) show_page(g_page - 1); }
static void next_cb(lv_event_t* e) { if (g_page + 1 < g_pages) show_page(g_page + 1); }

static void refresh_cb(lv_event_t* e) {
    if (g_view == VIEW_WEATHER) ui_weather_refresh();
    else if (g_view == VIEW_STATS) { g_stats_offline = false; ui_stats_refresh(); }
    else ui_refresh_keys();
}
static void weather_cb(lv_event_t* e) { ui_show_weather(g_view != VIEW_WEATHER); }
static void stats_cb(lv_event_t* e) { ui_show_stats(g_view != VIEW_STATS); }
static void stats_retry_cb(lv_event_t* e) { g_stats_offline = false; ui_stats_refresh(); }
static void settings_cb(lv_event_t* e) { ui_show_settings(); }
static void back_cb(lv_event_t* e) { ui_hide_settings(); }

static void theme_cb(lv_event_t* e) {
    cfg().dark = lv_obj_has_state(g_switch_theme, LV_STATE_CHECKED);
    cfg_save();
    apply_theme();
}

static void bright_cb(lv_event_t* e) {
    cfg().brightness = (uint8_t)lv_slider_get_value(g_slider_bright);
    cfg_save();
    set_backlight(cfg().brightness);
    String s = String(cfg().brightness) + "%";
    lv_label_set_text(g_label_bright, s.c_str());
}

static void reload_cb(lv_event_t* e) {
    ui_refresh_keys();
    ui_hide_settings();
}

static void field_cb(lv_event_t* e) {
    Field f = (Field)(intptr_t)lv_event_get_user_data(e);
    const char* title = "Edit";
    switch (f) {
        case F_SSID:  title = "Wi-Fi SSID"; break;
        case F_PASS:  title = "Wi-Fi Password"; break;
        case F_HOST:  title = "Server Host"; break;
        case F_PORT:  title = "Server Port"; break;
        case F_TOKEN: title = "Token"; break;
    }
    open_editor(f, title);
}

static void input_ready_cb(lv_event_t* e)       { input_apply(); }
static void input_cancel_cb(lv_event_t* e)      { input_cancel(); }
static void input_save_btn_cb(lv_event_t* e)    { input_apply(); }
static void input_cancel_btn_cb(lv_event_t* e)  { input_cancel(); }

static void save_apply_cb(lv_event_t* e) {
    bool wifi_changed = (cfg().ssid != g_applied_ssid) || (cfg().pass != g_applied_pass);
    cfg_save();
    g_applied_ssid = cfg().ssid;
    g_applied_pass = cfg().pass;
    ui_hide_settings();
    if (wifi_changed) {
        wifi_connect();
    } else {
        ui_refresh_keys();
    }
}

// ---------------- Публичное ----------------
void ui_set_status(bool ok) {
    if (!g_dot) return;
    lv_obj_set_style_bg_color(g_dot, lv_color_hex(ok ? C_GOOD : C_BAD), 0);
}

void ui_set_wifi(bool connected, int rssi) {
    g_wifi_connected = connected;
    g_wifi_rssi = rssi;
    if (!g_wifi) return;
    if (!connected) {
        lv_label_set_text(g_wifi, LV_SYMBOL_WIFI " no");
        lv_obj_set_style_text_color(g_wifi, lv_color_hex(C_BAD), 0);
        return;
    }
    String t = String(LV_SYMBOL_WIFI) + " " + String(rssi) + " dBm";
    lv_label_set_text(g_wifi, t.c_str());
    lv_color_t c = lv_color_hex(rssi > -60 ? C_GOOD : (rssi > -75 ? C_WARN : C_BAD));
    lv_obj_set_style_text_color(g_wifi, c, 0);
}

void ui_refresh_keys() {
    bool ok = netFetchKeys(g_keys, MAX_KEYS, g_count);
    rebuild_pages();
    ui_set_status(ok);
}

void ui_show_settings() { if (g_scr_settings) lv_scr_load(g_scr_settings); }
void ui_hide_settings() { if (g_scr) lv_scr_load(g_scr); }

// ---------------- Хелперы ----------------
static lv_obj_t* make_row(lv_obj_t* parent, int h) {
    lv_obj_t* row = lv_btn_create(parent);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, h);
    lv_obj_add_style(row, &st_card, 0);
    lv_obj_set_style_pad_hor(row, 8, 0);
    lv_obj_set_style_pad_ver(row, 0, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    return row;
}

static lv_obj_t* make_field_row(lv_obj_t* parent, const char* name, lv_obj_t** value_out, Field f) {
    lv_obj_t* row = make_row(parent, 36);
    lv_obj_add_event_cb(row, field_cb, LV_EVENT_CLICKED, (void*)(intptr_t)f);

    lv_obj_t* l = lv_label_create(row);
    lv_label_set_text(l, name);
    lv_obj_add_style(l, &st_text, 0);

    lv_obj_t* v = lv_label_create(row);
    lv_label_set_long_mode(v, LV_LABEL_LONG_DOT);
    lv_obj_set_width(v, 160);
    lv_obj_set_style_text_align(v, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_add_style(v, &st_dim, 0);
    *value_out = v;
    return row;
}

static void update_values() {
    auto setv = [](lv_obj_t* lbl, const String& s, bool mask) {
        if (!lbl) return;
        String v;
        if (s.length() == 0) v = "(not set)";
        else if (mask) v = "********";
        else v = s;
        lv_label_set_text(lbl, v.c_str());
    };
    setv(g_val_ssid,  cfg().ssid,  false);
    setv(g_val_pass,  cfg().pass,  true);
    setv(g_val_host,  cfg().host,  false);
    setv(g_val_port,  String((unsigned)cfg().port), false);
    setv(g_val_token, cfg().token, false);
}

// ---------------- Страницы / тема ----------------
static void show_page(int p) {
    if (g_pages < 1) g_pages = 1;
    if (p < 0) p = 0;
    if (p >= g_pages) p = g_pages - 1;
    g_page = p;

    for (int s = 0; s < PAGE_SIZE; s++) {
        g_page_keys[s] = -1;
        lv_obj_add_flag(g_slot_btn[s], LV_OBJ_FLAG_HIDDEN);
    }

    int song = 0;
    int first = g_page * PAGE_SIZE;
    for (int i = 0; i < g_count; i++) {
        if (i == g_stop_idx) continue;
        if (song >= first && song < first + PAGE_SIZE) {
            int s = song - first;
            g_page_keys[s] = i;
            String t = String(song + 1) + ". " + g_keys[i].label;
            lv_label_set_text(g_slot_lbl[s], t.c_str());
            lv_obj_clear_flag(g_slot_btn[s], LV_OBJ_FLAG_HIDDEN);
        }
        song++;
    }

    if (g_page_lbl) lv_label_set_text_fmt(g_page_lbl, "%d/%d", g_page + 1, g_pages);
    if (g_prev_btn) lv_obj_set_style_opa(g_prev_btn, g_page > 0 ? LV_OPA_COVER : LV_OPA_40, 0);
    if (g_next_btn) lv_obj_set_style_opa(g_next_btn, (g_page + 1 < g_pages) ? LV_OPA_COVER : LV_OPA_40, 0);
}

static void rebuild_pages() {
    g_stop_idx = -1;
    for (int i = 0; i < g_count; i++) {
        if (g_keys[i].id.equalsIgnoreCase("stop") || g_keys[i].label.equalsIgnoreCase("stop")) {
            g_stop_idx = i;
            break;
        }
    }

    g_song_count = 0;
    for (int i = 0; i < g_count; i++) {
        if (i != g_stop_idx) g_song_count++;
    }

    g_pages = (g_song_count + PAGE_SIZE - 1) / PAGE_SIZE;
    if (g_pages < 1) g_pages = 1;
    if (g_page >= g_pages) g_page = g_pages - 1;
    if (g_page < 0) g_page = 0;

    show_page(g_page);

    if (g_stop_btn) {
        if (g_stop_idx >= 0) lv_obj_clear_flag(g_stop_btn, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(g_stop_btn, LV_OBJ_FLAG_HIDDEN);
    }

    if (g_empty_lbl) {
        if (g_view == VIEW_KEYS && g_song_count == 0) lv_obj_clear_flag(g_empty_lbl, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(g_empty_lbl, LV_OBJ_FLAG_HIDDEN);
    }
}

static void apply_theme() {
    if (!g_scr) return;

    lv_style_set_bg_color(&st_bg, lv_color_hex(col_bg()));
    lv_style_set_bg_color(&st_bar, lv_color_hex(col_bar()));
    lv_style_set_bg_color(&st_card, lv_color_hex(col_btn()));
    lv_style_set_text_color(&st_text, lv_color_hex(col_text()));
    lv_style_set_text_color(&st_dim, lv_color_hex(col_dim()));

    lv_obj_set_style_bg_color(g_scr, lv_color_hex(col_bg()), 0);
    lv_obj_set_style_bg_color(g_bar, lv_color_hex(col_bar()), 0);
    lv_obj_set_style_bg_color(g_grid, lv_color_hex(col_bg()), 0);
    lv_obj_set_style_bg_color(g_btn_refresh, lv_color_hex(col_btn()), 0);
    lv_obj_set_style_bg_color(g_btn_settings, lv_color_hex(col_btn()), 0);
    lv_obj_set_style_bg_color(g_btn_weather, lv_color_hex(col_btn()), 0);
    lv_obj_set_style_bg_color(g_btn_stats, lv_color_hex(col_btn()), 0);
    lv_obj_set_style_text_color(g_lbl_refresh, lv_color_hex(col_text()), 0);
    lv_obj_set_style_text_color(g_lbl_settings, lv_color_hex(col_text()), 0);
    lv_obj_set_style_text_color(g_lbl_weather, lv_color_hex(col_text()), 0);
    lv_obj_set_style_text_color(g_lbl_stats, lv_color_hex(col_text()), 0);
    if (g_wx_view) lv_obj_set_style_bg_color(g_wx_view, lv_color_hex(col_bg()), 0);
    Ring* rings[] = { &g_ring_cpu_load, &g_ring_cpu_temp, &g_ring_ram,
                      &g_ring_gpu_load, &g_ring_gpu_temp, &g_ring_vram };
    for (Ring* r : rings) {
        if (r->arc) lv_obj_set_style_arc_color(r->arc, lv_color_hex(col_track()), LV_PART_MAIN);
    }

    rebuild_pages();
    ui_set_wifi(g_wifi_connected, g_wifi_rssi);
}

// ---------------- Ввод текста ----------------
static void open_editor(Field f, const char* title) {
    g_editing = f;
    lv_label_set_text(g_i_lbl_title, title);

    bool pw = false, num = false;
    String v;
    switch (f) {
        case F_SSID:  v = cfg().ssid; break;
        case F_PASS:  v = cfg().pass; pw = true; break;
        case F_HOST:  v = cfg().host; break;
        case F_PORT:  v = String((unsigned)cfg().port); num = true; break;
        case F_TOKEN: v = cfg().token; break;
    }

    lv_textarea_set_password_mode(g_ta_input, pw);
    lv_textarea_set_accepted_chars(g_ta_input, num ? "0123456789" : nullptr);
    lv_keyboard_set_mode(g_kb_input, num ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_textarea_set_text(g_ta_input, v.c_str());
    lv_keyboard_set_textarea(g_kb_input, g_ta_input);
    lv_scr_load(g_scr_input);
}

static void input_apply() {
    if (!g_ta_input) return;
    const char* txt = lv_textarea_get_text(g_ta_input);
    switch (g_editing) {
        case F_SSID:  cfg().ssid = txt; break;
        case F_PASS:  cfg().pass = txt; break;
        case F_HOST:  cfg().host = txt; break;
        case F_PORT: {
            int p = atoi(txt);
            if (p > 0 && p < 65536) cfg().port = (uint16_t)p;
            break;
        }
        case F_TOKEN: cfg().token = txt; break;
    }
    cfg_save();
    update_values();
    lv_scr_load(g_scr_settings);
}

static void input_cancel() {
    lv_scr_load(g_scr_settings);
}

// ---------------- Экраны ----------------
static void build_main_screen() {
    g_scr = lv_scr_act();
    lv_obj_clear_flag(g_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_scr, lv_color_hex(col_bg()), 0);

    g_bar = lv_obj_create(g_scr);
    lv_obj_clear_flag(g_bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_bar, lv_color_hex(col_bar()), 0);
    lv_obj_set_style_radius(g_bar, 0, 0);
    lv_obj_set_style_border_width(g_bar, 0, 0);
    lv_obj_set_style_pad_all(g_bar, 4, 0);
    lv_obj_set_pos(g_bar, 0, 0);
    lv_obj_set_size(g_bar, 320, 30);
    lv_obj_set_flex_flow(g_bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_bar, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t* left = lv_obj_create(g_bar);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(left, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_set_style_pad_column(left, 6, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    g_dot = lv_obj_create(left);
    lv_obj_clear_flag(g_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(g_dot, 10, 10);
    lv_obj_set_style_radius(g_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(g_dot, 0, 0);
    lv_obj_set_style_shadow_width(g_dot, 0, 0);
    lv_obj_set_style_bg_opa(g_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(g_dot, lv_color_hex(col_dim()), 0);

    g_wifi = lv_label_create(left);
    lv_label_set_text(g_wifi, LV_SYMBOL_WIFI " ...");
    lv_obj_set_style_text_color(g_wifi, lv_color_hex(col_dim()), 0);

    lv_obj_t* right = lv_obj_create(g_bar);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(right, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lv_obj_set_style_pad_all(right, 0, 0);
    lv_obj_set_style_pad_column(right, 4, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    g_btn_weather = lv_btn_create(right);
    lv_obj_set_size(g_btn_weather, 28, 24);
    lv_obj_set_style_bg_color(g_btn_weather, lv_color_hex(col_btn()), 0);
    lv_obj_set_style_radius(g_btn_weather, 6, 0);
    lv_obj_add_event_cb(g_btn_weather, weather_cb, LV_EVENT_CLICKED, nullptr);
    g_lbl_weather = lv_label_create(g_btn_weather);
    lv_label_set_text(g_lbl_weather, "W");
    lv_obj_set_style_text_color(g_lbl_weather, lv_color_hex(col_text()), 0);
    lv_obj_center(g_lbl_weather);

    g_btn_stats = lv_btn_create(right);
    lv_obj_set_size(g_btn_stats, 28, 24);
    lv_obj_set_style_bg_color(g_btn_stats, lv_color_hex(col_btn()), 0);
    lv_obj_set_style_radius(g_btn_stats, 6, 0);
    lv_obj_add_event_cb(g_btn_stats, stats_cb, LV_EVENT_CLICKED, nullptr);
    g_lbl_stats = lv_label_create(g_btn_stats);
    lv_label_set_text(g_lbl_stats, "PC");
    lv_obj_set_style_text_color(g_lbl_stats, lv_color_hex(col_text()), 0);
    lv_obj_center(g_lbl_stats);

    g_btn_refresh = lv_btn_create(right);
    lv_obj_set_size(g_btn_refresh, 28, 24);
    lv_obj_set_style_bg_color(g_btn_refresh, lv_color_hex(col_btn()), 0);
    lv_obj_set_style_radius(g_btn_refresh, 6, 0);
    lv_obj_add_event_cb(g_btn_refresh, refresh_cb, LV_EVENT_CLICKED, nullptr);
    g_lbl_refresh = lv_label_create(g_btn_refresh);
    lv_label_set_text(g_lbl_refresh, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(g_lbl_refresh, lv_color_hex(col_text()), 0);
    lv_obj_center(g_lbl_refresh);

    g_btn_settings = lv_btn_create(right);
    lv_obj_set_size(g_btn_settings, 28, 24);
    lv_obj_set_style_bg_color(g_btn_settings, lv_color_hex(col_btn()), 0);
    lv_obj_set_style_radius(g_btn_settings, 6, 0);
    lv_obj_add_event_cb(g_btn_settings, settings_cb, LV_EVENT_CLICKED, nullptr);
    g_lbl_settings = lv_label_create(g_btn_settings);
    lv_label_set_text(g_lbl_settings, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(g_lbl_settings, lv_color_hex(col_text()), 0);
    lv_obj_center(g_lbl_settings);

    g_grid = lv_obj_create(g_scr);
    lv_obj_set_style_bg_color(g_grid, lv_color_hex(col_bg()), 0);
    lv_obj_set_style_border_width(g_grid, 0, 0);
    lv_obj_set_style_radius(g_grid, 0, 0);
    lv_obj_set_style_pad_all(g_grid, 8, 0);
    lv_obj_set_style_pad_row(g_grid, 8, 0);
    lv_obj_set_style_pad_column(g_grid, 8, 0);
    lv_obj_set_pos(g_grid, 0, 30);
    lv_obj_set_size(g_grid, 320, 170);
    lv_obj_set_flex_flow(g_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(g_grid, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g_grid, LV_OBJ_FLAG_SCROLLABLE);

    for (int s = 0; s < PAGE_SIZE; s++) {
        lv_obj_t* b = lv_btn_create(g_grid);
        lv_obj_set_size(b, 96, 58);
        lv_obj_add_style(b, &st_card, 0);
        lv_obj_set_style_radius(b, 8, 0);
        lv_obj_set_style_shadow_width(b, 0, 0);
        lv_obj_set_style_border_width(b, 0, 0);
        lv_obj_set_style_pad_all(b, 4, 0);
        lv_obj_add_event_cb(b, slot_cb, LV_EVENT_CLICKED, (void*)(intptr_t)s);
        lv_obj_add_flag(b, LV_OBJ_FLAG_HIDDEN);

        lv_obj_t* l = lv_label_create(b);
        lv_obj_add_style(l, &st_text, 0);
        lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
        lv_obj_set_width(l, lv_pct(90));
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(l);

        g_slot_btn[s] = b;
        g_slot_lbl[s] = l;
        g_page_keys[s] = -1;
    }

    g_empty_lbl = lv_label_create(g_scr);
    lv_label_set_text(g_empty_lbl, "No buttons.\nCheck server.");
    lv_obj_set_style_text_align(g_empty_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_style(g_empty_lbl, &st_dim, 0);
    lv_obj_align(g_empty_lbl, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_add_flag(g_empty_lbl, LV_OBJ_FLAG_HIDDEN);

    g_bottom = lv_obj_create(g_scr);
    lv_obj_clear_flag(g_bottom, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(g_bottom, 0, 190);
    lv_obj_set_size(g_bottom, 320, 45);
    lv_obj_set_style_bg_opa(g_bottom, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(g_bottom, 0, 0);
    lv_obj_set_style_pad_all(g_bottom, 0, 0);
    lv_obj_set_style_pad_column(g_bottom, 6, 0);
    lv_obj_set_flex_flow(g_bottom, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_bottom, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    g_page_lbl = lv_label_create(g_bottom);
    lv_label_set_text(g_page_lbl, "1/1");
    lv_obj_add_style(g_page_lbl, &st_dim, 0);
    lv_obj_set_width(g_page_lbl, 30);
    lv_obj_set_style_text_align(g_page_lbl, LV_TEXT_ALIGN_CENTER, 0);

    g_prev_btn = lv_btn_create(g_bottom);
    lv_obj_set_size(g_prev_btn, 45, 45);
    lv_obj_add_style(g_prev_btn, &st_card, 0);
    lv_obj_set_style_shadow_width(g_prev_btn, 0, 0);
    lv_obj_set_style_border_width(g_prev_btn, 0, 0);
    lv_obj_add_event_cb(g_prev_btn, prev_cb, LV_EVENT_CLICKED, nullptr);
    {
        lv_obj_t* l = lv_label_create(g_prev_btn);
        lv_label_set_text(l, LV_SYMBOL_LEFT);
        lv_obj_add_style(l, &st_text, 0);
        lv_obj_center(l);
    }

    g_stop_btn = lv_btn_create(g_bottom);
    lv_obj_set_size(g_stop_btn, 120, 45);
    lv_obj_set_style_bg_color(g_stop_btn, lv_color_hex(C_BAD), 0);
    lv_obj_set_style_bg_grad_dir(g_stop_btn, LV_GRAD_DIR_NONE, 0);
    lv_obj_set_style_radius(g_stop_btn, 8, 0);
    lv_obj_set_style_shadow_width(g_stop_btn, 0, 0);
    lv_obj_set_style_border_width(g_stop_btn, 0, 0);
    lv_obj_add_event_cb(g_stop_btn, stop_cb, LV_EVENT_CLICKED, nullptr);
    {
        lv_obj_t* l = lv_label_create(g_stop_btn);
        lv_label_set_text(l, "STOP");
        lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(l);
    }
    lv_obj_add_flag(g_stop_btn, LV_OBJ_FLAG_HIDDEN);

    g_next_btn = lv_btn_create(g_bottom);
    lv_obj_set_size(g_next_btn, 45, 45);
    lv_obj_add_style(g_next_btn, &st_card, 0);
    lv_obj_set_style_shadow_width(g_next_btn, 0, 0);
    lv_obj_set_style_border_width(g_next_btn, 0, 0);
    lv_obj_add_event_cb(g_next_btn, next_cb, LV_EVENT_CLICKED, nullptr);
    {
        lv_obj_t* l = lv_label_create(g_next_btn);
        lv_label_set_text(l, LV_SYMBOL_RIGHT);
        lv_obj_add_style(l, &st_text, 0);
        lv_obj_center(l);
    }

    lv_obj_t* g_page_ph = lv_label_create(g_bottom);
    lv_label_set_text(g_page_ph, "1/1");
    lv_obj_add_style(g_page_ph, &st_dim, 0);
    lv_obj_set_width(g_page_ph, 30);
    lv_obj_set_style_text_align(g_page_ph, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_opa(g_page_ph, LV_OPA_TRANSP, 0);
}

// ---------------- Погода ----------------
static lv_obj_t* wx_dot(lv_obj_t* parent, int x, int y, int d, uint32_t color) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, d, d);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}

static lv_obj_t* wx_bar(lv_obj_t* parent, int x, int y, int w, int h, uint32_t color) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, 1, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}

static int wx_kind(int code) {
    if (code == 0) return 0;                 // sun
    if (code <= 3) return 1;                 // partly cloudy
    if (code == 45 || code == 48) return 2;  // fog
    if (code >= 51 && code <= 57) return 3;  // drizzle
    if (code >= 61 && code <= 67) return 4;  // rain
    if (code >= 71 && code <= 77) return 5;  // snow
    if (code >= 80 && code <= 82) return 4;  // showers
    if (code >= 85 && code <= 86) return 5;  // snow showers
    if (code >= 95) return 6;                // thunder
    return 7;                                // cloud
}

static void wx_draw(lv_obj_t* parent, int code, int s) {
    const uint32_t sunC   = 0xF1C40F;
    const uint32_t cloudC = 0xB0BEC5;
    const uint32_t rainC  = 0x4F8CFF;
    const uint32_t snowC  = 0xE3F2FD;

    int k = wx_kind(code);
    float f = s / 48.0f;
    auto S = [f](int v) { return (int)(v * f); };

    bool sun = (k == 0 || k == 1);
    bool cloud = (k != 0);
    bool rain = (k == 3 || k == 4);
    bool snow = (k == 5);
    bool fog = (k == 2);
    bool thunder = (k == 6);

    if (sun) {
        int d = S(k == 0 ? 20 : 14);
        int cx = S(k == 0 ? 14 : 4);
        int cy = S(k == 0 ? 14 : 4);
        wx_dot(parent, cx, cy, d, sunC);
        if (k == 0) {
            wx_bar(parent, S(23), S(2), S(2), S(8), sunC);
            wx_bar(parent, S(23), S(38), S(2), S(8), sunC);
            wx_bar(parent, S(2), S(23), S(8), S(2), sunC);
            wx_bar(parent, S(38), S(23), S(8), S(2), sunC);
        }
    }

    if (cloud) {
        wx_dot(parent, S(9),  S(24), S(14), cloudC);
        wx_dot(parent, S(19), S(17), S(18), cloudC);
        wx_dot(parent, S(29), S(24), S(14), cloudC);
        wx_bar(parent, S(11), S(31), S(26), S(9), cloudC);
    }

    if (rain) {
        wx_bar(parent, S(16), S(42), S(2), S(6), rainC);
        wx_bar(parent, S(23), S(42), S(2), S(6), rainC);
        wx_bar(parent, S(30), S(42), S(2), S(6), rainC);
    }
    if (snow) {
        wx_dot(parent, S(16), S(42), S(3), snowC);
        wx_dot(parent, S(23), S(42), S(3), snowC);
        wx_dot(parent, S(30), S(42), S(3), snowC);
    }
    if (fog) {
        wx_bar(parent, S(12), S(42), S(24), S(2), cloudC);
        wx_bar(parent, S(16), S(46), S(20), S(2), cloudC);
    }
    if (thunder) {
        wx_bar(parent, S(24), S(40), S(3), S(8), sunC);
    }
}

static int wx_round(float v) {
    return (int)(v >= 0 ? v + 0.5f : v - 0.5f);
}

static void rebuild_weather() {
    if (!g_wx_view || !g_wx_icon || !g_wx_days) return;
    lv_obj_clean(g_wx_icon);
    lv_obj_clean(g_wx_days);

    if (!g_wx.valid) {
        if (g_wx_msg) lv_obj_clear_flag(g_wx_msg, LV_OBJ_FLAG_HIDDEN);
        if (g_wx_temp) lv_obj_add_flag(g_wx_temp, LV_OBJ_FLAG_HIDDEN);
        if (g_wx_city) lv_obj_add_flag(g_wx_city, LV_OBJ_FLAG_HIDDEN);
        if (g_wx_feels) lv_obj_add_flag(g_wx_feels, LV_OBJ_FLAG_HIDDEN);
        if (g_wx_hum) lv_obj_add_flag(g_wx_hum, LV_OBJ_FLAG_HIDDEN);
        if (g_wx_wind) lv_obj_add_flag(g_wx_wind, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (g_wx_msg) lv_obj_add_flag(g_wx_msg, LV_OBJ_FLAG_HIDDEN);
    if (g_wx_temp) lv_obj_clear_flag(g_wx_temp, LV_OBJ_FLAG_HIDDEN);
    if (g_wx_city) lv_obj_clear_flag(g_wx_city, LV_OBJ_FLAG_HIDDEN);
    if (g_wx_feels) lv_obj_clear_flag(g_wx_feels, LV_OBJ_FLAG_HIDDEN);
    if (g_wx_hum) lv_obj_clear_flag(g_wx_hum, LV_OBJ_FLAG_HIDDEN);
    if (g_wx_wind) lv_obj_clear_flag(g_wx_wind, LV_OBJ_FLAG_HIDDEN);

    wx_draw(g_wx_icon, g_wx.code, 48);

    char buf[48];
    snprintf(buf, sizeof(buf), "%+d C", wx_round(g_wx.temp));
    lv_label_set_text(g_wx_temp, buf);
    snprintf(buf, sizeof(buf), "feels %+d C", wx_round(g_wx.feels));
    lv_label_set_text(g_wx_feels, buf);
    snprintf(buf, sizeof(buf), "Humidity %d%%", g_wx.humidity);
    lv_label_set_text(g_wx_hum, buf);
    int w10 = wx_round(g_wx.wind * 10.0f);
    snprintf(buf, sizeof(buf), "Wind %d.%d m/s", w10 / 10, w10 % 10);
    lv_label_set_text(g_wx_wind, buf);

    for (int i = 0; i < g_wx.dayCount; i++) {
        lv_obj_t* col = lv_obj_create(g_wx_days);
        lv_obj_remove_style_all(col);
        lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(col, 44, 94);

        lv_obj_t* d = lv_label_create(col);
        lv_label_set_text(d, g_wx.days[i].date.c_str());
        lv_obj_add_style(d, &st_dim, 0);
        lv_obj_set_style_text_font(d, &lv_font_montserrat_12, 0);
        lv_obj_set_width(d, 44);
        lv_obj_set_style_text_align(d, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t* ic = lv_obj_create(col);
        lv_obj_remove_style_all(ic);
        lv_obj_clear_flag(ic, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(ic, 9, 16);
        lv_obj_set_size(ic, 26, 26);
        wx_draw(ic, g_wx.days[i].code, 26);

        char t[24];
        snprintf(t, sizeof(t), "%+d/%+d",
                 wx_round(g_wx.days[i].tmax), wx_round(g_wx.days[i].tmin));
        lv_obj_t* tm = lv_label_create(col);
        lv_label_set_text(tm, t);
        lv_obj_add_style(tm, &st_text, 0);
        lv_obj_set_style_text_font(tm, &lv_font_montserrat_12, 0);
        lv_obj_set_width(tm, 44);
        lv_obj_set_style_text_align(tm, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_pos(tm, 0, 50);
    }
}

static void ui_weather_refresh() {
    if (g_wx_busy) return;
    g_wx_busy = true;
    g_wx_last_try = millis();

    if (g_wx_msg && !g_wx.valid) {
        lv_label_set_text(g_wx_msg, "Loading...");
        lv_obj_clear_flag(g_wx_msg, LV_OBJ_FLAG_HIDDEN);
        lv_refr_now(NULL);
    }

    bool ok = netFetchWeather(g_wx);
    g_wx_busy = false;
    if (!ok && !g_wx.valid && g_wx_msg) {
        lv_label_set_text(g_wx_msg, "No weather data");
        lv_obj_clear_flag(g_wx_msg, LV_OBJ_FLAG_HIDDEN);
    }
    rebuild_weather();
    ui_set_status(ok);
}

void ui_show_weather(bool show) {
    if (show) {
        g_view = VIEW_WEATHER;
        if (g_grid) lv_obj_add_flag(g_grid, LV_OBJ_FLAG_HIDDEN);
        if (g_bottom) lv_obj_add_flag(g_bottom, LV_OBJ_FLAG_HIDDEN);
        if (g_empty_lbl) lv_obj_add_flag(g_empty_lbl, LV_OBJ_FLAG_HIDDEN);
        if (g_stats_view) lv_obj_add_flag(g_stats_view, LV_OBJ_FLAG_HIDDEN);
        if (g_wx_view) lv_obj_clear_flag(g_wx_view, LV_OBJ_FLAG_HIDDEN);
        if (g_lbl_weather) lv_label_set_text(g_lbl_weather, LV_SYMBOL_LIST);
        if (g_lbl_stats) lv_label_set_text(g_lbl_stats, "PC");
        if (!g_wx.valid || millis() - g_wx.updatedMs > 15UL * 60UL * 1000UL) {
            ui_weather_refresh();
        } else {
            rebuild_weather();
        }
    } else {
        g_view = VIEW_KEYS;
        if (g_wx_view) lv_obj_add_flag(g_wx_view, LV_OBJ_FLAG_HIDDEN);
        if (g_stats_view) lv_obj_add_flag(g_stats_view, LV_OBJ_FLAG_HIDDEN);
        if (g_grid) lv_obj_clear_flag(g_grid, LV_OBJ_FLAG_HIDDEN);
        if (g_bottom) lv_obj_clear_flag(g_bottom, LV_OBJ_FLAG_HIDDEN);
        if (g_lbl_weather) lv_label_set_text(g_lbl_weather, "W");
        if (g_lbl_stats) lv_label_set_text(g_lbl_stats, "PC");
        rebuild_pages();
    }
}

static void ring_show(Ring& r, bool show) {
    if (!r.box) return;
    if (show) lv_obj_clear_flag(r.box, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(r.box, LV_OBJ_FLAG_HIDDEN);
}

static void ring_set(Ring& r, int pct, const char* value, const char* sub) {
    if (!r.box) return;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    lv_arc_set_value(r.arc, pct);
    lv_label_set_text(r.val, value);

    bool hasSub = (sub && sub[0]);
    if (hasSub) {
        lv_label_set_text(r.sub, sub);
        lv_obj_clear_flag(r.sub, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align_to(r.val, r.arc, LV_ALIGN_CENTER, 0, -12);
        lv_obj_align_to(r.sub, r.arc, LV_ALIGN_CENTER, 0, 12);
    } else {
        lv_label_set_text(r.sub, "");
        lv_obj_add_flag(r.sub, LV_OBJ_FLAG_HIDDEN);
        lv_obj_align_to(r.val, r.arc, LV_ALIGN_CENTER, 0, 0);
    }
}

static void apply_stats_show() {
    if (!g_stats_view || !g_pc.ok) return;

    char v[32];
    char s[32];

    ring_show(g_ring_cpu_load, g_pc.showCpuLoad);
    if (g_pc.showCpuLoad) {
        int p = (int)(g_pc.cpuLoad + 0.5f);
        snprintf(v, sizeof(v), "%d%%", p);
        ring_set(g_ring_cpu_load, p, v, "");
    }

    ring_show(g_ring_cpu_temp, g_pc.showCpuTemp);
    if (g_pc.showCpuTemp) {
        if (g_pc.cpuTempValid) snprintf(v, sizeof(v), "%d C", (int)(g_pc.cpuTemp + 0.5f));
        else snprintf(v, sizeof(v), "--");
        if (g_pc.cpuFreqValid) {
            int f10 = (int)(g_pc.cpuFreqMHz / 100.0f + 0.5f);
            snprintf(s, sizeof(s), "%d.%d GHz", f10 / 10, f10 % 10);
        } else {
            s[0] = 0;
        }
        ring_set(g_ring_cpu_temp, g_pc.cpuTempValid ? (int)(g_pc.cpuTemp + 0.5f) : 0, v, s);
    }

    ring_show(g_ring_ram, g_pc.showRam);
    if (g_pc.showRam) {
        int p = (int)(g_pc.ramPercent + 0.5f);
        snprintf(v, sizeof(v), "%d%%", p);
        int u10 = (g_pc.ramUsed * 10 + 512) / 1024;
        int t10 = (g_pc.ramTotal * 10 + 512) / 1024;
        snprintf(s, sizeof(s), "%d.%d/%d.%dG", u10 / 10, u10 % 10, t10 / 10, t10 % 10);
        ring_set(g_ring_ram, p, v, s);
    }

    bool gpu = g_pc.gpuValid;

    ring_show(g_ring_gpu_load, gpu && g_pc.showGpuLoad);
    if (gpu && g_pc.showGpuLoad) {
        int p = (int)(g_pc.gpuLoad + 0.5f);
        snprintf(v, sizeof(v), "%d%%", p);
        ring_set(g_ring_gpu_load, p, v, "");
    }

    ring_show(g_ring_gpu_temp, gpu && g_pc.showGpuTemp);
    if (gpu && g_pc.showGpuTemp) {
        if (g_pc.gpuTempValid) snprintf(v, sizeof(v), "%d C", (int)(g_pc.gpuTemp + 0.5f));
        else snprintf(v, sizeof(v), "--");
        ring_set(g_ring_gpu_temp, g_pc.gpuTempValid ? (int)(g_pc.gpuTemp + 0.5f) : 0, v, "");
    }

    ring_show(g_ring_vram, gpu && g_pc.showVram && g_pc.vramTotal > 0);
    if (gpu && g_pc.showVram && g_pc.vramTotal > 0) {
        int p = (g_pc.vramUsed * 100 + g_pc.vramTotal / 2) / g_pc.vramTotal;
        snprintf(v, sizeof(v), "%d%%", p);
        int u10 = (g_pc.vramUsed * 10 + 512) / 1024;
        int t10 = (g_pc.vramTotal * 10 + 512) / 1024;
        snprintf(s, sizeof(s), "%d.%d/%d.%dG", u10 / 10, u10 % 10, t10 / 10, t10 % 10);
        ring_set(g_ring_vram, p, v, s);
    }
}

static void ui_stats_refresh() {
    if (g_stats_busy) return;
    g_stats_busy = true;
    g_stats_last_try = millis();

    if (g_stats_retry) lv_obj_add_flag(g_stats_retry, LV_OBJ_FLAG_HIDDEN);

    if (g_stats_msg && !g_pc.ok) {
        lv_label_set_text(g_stats_msg, "Loading...");
        lv_obj_clear_flag(g_stats_msg, LV_OBJ_FLAG_HIDDEN);
        lv_refr_now(NULL);
    }

    bool ok = netFetchStats(g_pc);
    g_stats_busy = false;

    if (ok && g_pc.ok) {
        g_stats_offline = false;
        if (g_stats_msg) lv_obj_add_flag(g_stats_msg, LV_OBJ_FLAG_HIDDEN);
        apply_stats_show();
    } else {
        g_stats_offline = true;
        if (g_stats_msg) {
            lv_label_set_text(g_stats_msg, ok ? "LHM off" : "Server offline");
            lv_obj_clear_flag(g_stats_msg, LV_OBJ_FLAG_HIDDEN);
        }
        if (g_stats_retry) lv_obj_clear_flag(g_stats_retry, LV_OBJ_FLAG_HIDDEN);
        ring_show(g_ring_cpu_load, false);
        ring_show(g_ring_cpu_temp, false);
        ring_show(g_ring_ram, false);
        ring_show(g_ring_gpu_load, false);
        ring_show(g_ring_gpu_temp, false);
        ring_show(g_ring_vram, false);
    }

    ui_set_status(ok);
}

void ui_show_stats(bool show) {
    if (show) {
        g_view = VIEW_STATS;
        if (g_grid) lv_obj_add_flag(g_grid, LV_OBJ_FLAG_HIDDEN);
        if (g_bottom) lv_obj_add_flag(g_bottom, LV_OBJ_FLAG_HIDDEN);
        if (g_empty_lbl) lv_obj_add_flag(g_empty_lbl, LV_OBJ_FLAG_HIDDEN);
        if (g_wx_view) lv_obj_add_flag(g_wx_view, LV_OBJ_FLAG_HIDDEN);
        if (g_stats_view) lv_obj_clear_flag(g_stats_view, LV_OBJ_FLAG_HIDDEN);
        if (g_lbl_stats) lv_label_set_text(g_lbl_stats, LV_SYMBOL_LIST);
        if (g_lbl_weather) lv_label_set_text(g_lbl_weather, "W");
        if (g_pc.ok) {
            if (millis() - g_pc.updatedMs > 2000) {
                g_stats_offline = false;
                ui_stats_refresh();
            } else {
                apply_stats_show();
            }
        } else if (g_stats_offline) {
            if (g_stats_msg) lv_obj_clear_flag(g_stats_msg, LV_OBJ_FLAG_HIDDEN);
            if (g_stats_retry) lv_obj_clear_flag(g_stats_retry, LV_OBJ_FLAG_HIDDEN);
        } else {
            ui_stats_refresh();
        }
    } else {
        g_view = VIEW_KEYS;
        if (g_stats_view) lv_obj_add_flag(g_stats_view, LV_OBJ_FLAG_HIDDEN);
        if (g_wx_view) lv_obj_add_flag(g_wx_view, LV_OBJ_FLAG_HIDDEN);
        if (g_grid) lv_obj_clear_flag(g_grid, LV_OBJ_FLAG_HIDDEN);
        if (g_bottom) lv_obj_clear_flag(g_bottom, LV_OBJ_FLAG_HIDDEN);
        if (g_lbl_stats) lv_label_set_text(g_lbl_stats, "PC");
        if (g_lbl_weather) lv_label_set_text(g_lbl_weather, "W");
        rebuild_pages();
    }
}

void ui_stats_tick() {
    if (g_view != VIEW_STATS || g_stats_busy || g_stats_offline) return;
    if (millis() - g_stats_last_try < 1000UL) return;
    ui_stats_refresh();
}

void ui_weather_tick() {
    if (g_view != VIEW_WEATHER || g_wx_busy) return;
    if (millis() - g_wx_last_try < 60000UL) return;
    if (g_wx.valid && millis() - g_wx.updatedMs < 15UL * 60UL * 1000UL) return;
    ui_weather_refresh();
}

static void build_weather_view() {
    g_wx_view = lv_obj_create(g_scr);
    lv_obj_clear_flag(g_wx_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(g_wx_view, 0, 30);
    lv_obj_set_size(g_wx_view, 320, 210);
    lv_obj_add_style(g_wx_view, &st_bg, 0);
    lv_obj_set_style_border_width(g_wx_view, 0, 0);
    lv_obj_set_style_pad_all(g_wx_view, 0, 0);

    g_wx_icon = lv_obj_create(g_wx_view);
    lv_obj_remove_style_all(g_wx_icon);
    lv_obj_clear_flag(g_wx_icon, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(g_wx_icon, 10, 8);
    lv_obj_set_size(g_wx_icon, 52, 52);

    g_wx_temp = lv_label_create(g_wx_view);
    lv_obj_add_style(g_wx_temp, &st_text, 0);
    lv_obj_set_style_text_font(g_wx_temp, &lv_font_montserrat_28, 0);
    lv_obj_set_pos(g_wx_temp, 72, 4);

    g_wx_city = lv_label_create(g_wx_view);
    lv_label_set_text(g_wx_city, "Novosibirsk");
    lv_obj_add_style(g_wx_city, &st_text, 0);
    lv_obj_set_style_text_font(g_wx_city, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(g_wx_city, 72, 40);

    g_wx_feels = lv_label_create(g_wx_view);
    lv_obj_add_style(g_wx_feels, &st_dim, 0);
    lv_obj_set_pos(g_wx_feels, 72, 62);

    g_wx_hum = lv_label_create(g_wx_view);
    lv_obj_add_style(g_wx_hum, &st_dim, 0);
    lv_obj_set_pos(g_wx_hum, 10, 76);

    g_wx_wind = lv_label_create(g_wx_view);
    lv_obj_add_style(g_wx_wind, &st_dim, 0);
    lv_obj_set_pos(g_wx_wind, 10, 94);

    g_wx_days = lv_obj_create(g_wx_view);
    lv_obj_remove_style_all(g_wx_days);
    lv_obj_set_pos(g_wx_days, 0, 112);
    lv_obj_set_size(g_wx_days, 320, 96);
    lv_obj_set_flex_flow(g_wx_days, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(g_wx_days, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(g_wx_days, LV_OBJ_FLAG_SCROLLABLE);

    g_wx_msg = lv_label_create(g_wx_view);
    lv_label_set_text(g_wx_msg, "Loading...");
    lv_obj_add_style(g_wx_msg, &st_dim, 0);
    lv_obj_align(g_wx_msg, LV_ALIGN_TOP_MID, 0, 110);
    lv_obj_add_flag(g_wx_msg, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(g_wx_view, LV_OBJ_FLAG_HIDDEN);
}

static void build_ring(Ring& r, lv_obj_t* parent, const char* caption, uint32_t color) {
    r.box = lv_obj_create(parent);
    lv_obj_remove_style_all(r.box);
    lv_obj_set_size(r.box, 104, 98);
    lv_obj_clear_flag(r.box, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* cap = lv_label_create(r.box);
    lv_label_set_text(cap, caption);
    lv_obj_add_style(cap, &st_dim, 0);
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_12, 0);
    lv_obj_align(cap, LV_ALIGN_BOTTOM_MID, 0, 0);

    r.arc = lv_arc_create(r.box);
    lv_obj_set_size(r.arc, 90, 90);
    lv_obj_align(r.arc, LV_ALIGN_TOP_MID, 0, 8);
    lv_arc_set_bg_angles(r.arc, 160, 20);
    lv_arc_set_range(r.arc, 0, 100);
    lv_arc_set_value(r.arc, 0);
    lv_obj_clear_flag(r.arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(r.arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_width(r.arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(r.arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(r.arc, lv_color_hex(col_track()), LV_PART_MAIN);
    lv_obj_set_style_arc_color(r.arc, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(r.arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_border_width(r.arc, 0, LV_PART_KNOB);

    r.val = lv_label_create(r.box);
    lv_obj_add_style(r.val, &st_text, 0);
    lv_obj_set_style_text_font(r.val, &lv_font_montserrat_20, 0);
    lv_obj_set_width(r.val, 86);
    lv_obj_set_style_text_align(r.val, LV_TEXT_ALIGN_CENTER, 0);

    r.sub = lv_label_create(r.box);
    lv_obj_add_style(r.sub, &st_dim, 0);
    lv_obj_set_style_text_font(r.sub, &lv_font_montserrat_10, 0);
    lv_obj_set_width(r.sub, 80);
    lv_obj_set_style_text_align(r.sub, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(r.sub, LV_LABEL_LONG_WRAP);
}

static void build_stats_view() {
    g_stats_view = lv_obj_create(g_scr);
    lv_obj_clear_flag(g_stats_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(g_stats_view, 0, 30);
    lv_obj_set_size(g_stats_view, 320, 210);
    lv_obj_add_style(g_stats_view, &st_bg, 0);
    lv_obj_set_style_border_width(g_stats_view, 0, 0);
    lv_obj_set_style_pad_all(g_stats_view, 0, 0);

    g_stats_grid = lv_obj_create(g_stats_view);
    lv_obj_remove_style_all(g_stats_grid);
    lv_obj_set_pos(g_stats_grid, 4, 4);
    lv_obj_set_size(g_stats_grid, 312, 200);
    lv_obj_set_style_pad_all(g_stats_grid, 0, 0);
    lv_obj_set_style_pad_row(g_stats_grid, 4, 0);
    lv_obj_set_style_pad_column(g_stats_grid, 0, 0);
    lv_obj_set_flex_flow(g_stats_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(g_stats_grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g_stats_grid, LV_OBJ_FLAG_SCROLLABLE);

    build_ring(g_ring_cpu_load, g_stats_grid, "CPU",      0x4F8CFF);
    build_ring(g_ring_cpu_temp, g_stats_grid, "CPU TEMP", 0xE67E22);
    build_ring(g_ring_ram,      g_stats_grid, "RAM",      0xF1C40F);
    build_ring(g_ring_gpu_load, g_stats_grid, "GPU",      0x2ECC71);
    build_ring(g_ring_gpu_temp, g_stats_grid, "GPU TEMP", 0xE74C3C);
    build_ring(g_ring_vram,     g_stats_grid, "VRAM",     0x9B59B6);

    ring_show(g_ring_cpu_load, false);
    ring_show(g_ring_cpu_temp, false);
    ring_show(g_ring_ram, false);
    ring_show(g_ring_gpu_load, false);
    ring_show(g_ring_gpu_temp, false);
    ring_show(g_ring_vram, false);

    g_stats_msg = lv_label_create(g_stats_view);
    lv_label_set_text(g_stats_msg, "Loading...");
    lv_obj_add_style(g_stats_msg, &st_dim, 0);
    lv_obj_align(g_stats_msg, LV_ALIGN_TOP_MID, 0, 100);
    lv_obj_add_flag(g_stats_msg, LV_OBJ_FLAG_HIDDEN);

    g_stats_retry = lv_btn_create(g_stats_view);
    lv_obj_set_size(g_stats_retry, 120, 36);
    lv_obj_align(g_stats_retry, LV_ALIGN_TOP_MID, 0, 128);
    lv_obj_add_style(g_stats_retry, &st_card, 0);
    lv_obj_set_style_shadow_width(g_stats_retry, 0, 0);
    lv_obj_add_event_cb(g_stats_retry, stats_retry_cb, LV_EVENT_CLICKED, nullptr);
    {
        lv_obj_t* l = lv_label_create(g_stats_retry);
        lv_label_set_text(l, "Retry");
        lv_obj_add_style(l, &st_text, 0);
        lv_obj_center(l);
    }
    lv_obj_add_flag(g_stats_retry, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(g_stats_view, LV_OBJ_FLAG_HIDDEN);
}

static void build_settings_screen() {
    g_scr_settings = lv_obj_create(NULL);
    lv_obj_clear_flag(g_scr_settings, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(g_scr_settings, &st_bg, 0);

    lv_obj_t* hdr = lv_obj_create(g_scr_settings);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_size(hdr, 320, 34);
    lv_obj_add_style(hdr, &st_bar, 0);
    lv_obj_set_style_pad_all(hdr, 3, 0);

    lv_obj_t* back = lv_btn_create(hdr);
    lv_obj_set_size(back, 30, 26);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_style(back, &st_card, 0);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, nullptr);
    g_s_lbl_back = lv_label_create(back);
    lv_label_set_text(g_s_lbl_back, LV_SYMBOL_LEFT);
    lv_obj_add_style(g_s_lbl_back, &st_text, 0);
    lv_obj_center(g_s_lbl_back);

    lv_obj_t* title = lv_label_create(hdr);
    lv_label_set_text(title, "Settings");
    lv_obj_add_style(title, &st_text, 0);
    lv_obj_center(title);

    lv_obj_t* content = lv_obj_create(g_scr_settings);
    lv_obj_set_pos(content, 0, 34);
    lv_obj_set_size(content, 320, 206);
    lv_obj_add_style(content, &st_bg, 0);
    lv_obj_set_style_pad_all(content, 6, 0);
    lv_obj_set_style_pad_row(content, 6, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(content, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(content, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(content, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLL_ELASTIC);

    lv_obj_t* row_theme = make_row(content, 36);
    lv_obj_t* lt = lv_label_create(row_theme);
    lv_label_set_text(lt, "Dark theme");
    lv_obj_add_style(lt, &st_text, 0);
    g_switch_theme = lv_switch_create(row_theme);
    lv_obj_set_size(g_switch_theme, 52, 28);
    if (cfg().dark) lv_obj_add_state(g_switch_theme, LV_STATE_CHECKED);
    lv_obj_add_event_cb(g_switch_theme, theme_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    lv_obj_t* row_br = make_row(content, 44);
    lv_obj_t* lb = lv_label_create(row_br);
    lv_label_set_text(lb, "Brightness");
    lv_obj_add_style(lb, &st_text, 0);
    g_slider_bright = lv_slider_create(row_br);
    lv_obj_set_size(g_slider_bright, 130, 16);
    lv_slider_set_range(g_slider_bright, 5, 100);
    lv_slider_set_value(g_slider_bright, cfg().brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(g_slider_bright, bright_cb, LV_EVENT_VALUE_CHANGED, nullptr);
    g_label_bright = lv_label_create(row_br);
    String bs = String(cfg().brightness) + "%";
    lv_label_set_text(g_label_bright, bs.c_str());
    lv_obj_add_style(g_label_bright, &st_text, 0);

    lv_obj_t* sec = lv_label_create(content);
    lv_label_set_text(sec, "Connection");
    lv_obj_add_style(sec, &st_dim, 0);

    make_field_row(content, "Wi-Fi SSID",  &g_val_ssid,  F_SSID);
    make_field_row(content, "Password",    &g_val_pass,  F_PASS);
    make_field_row(content, "Server host", &g_val_host,  F_HOST);
    make_field_row(content, "Server port", &g_val_port,  F_PORT);
    make_field_row(content, "Token",       &g_val_token, F_TOKEN);

    g_s_btn_save = lv_btn_create(content);
    lv_obj_set_width(g_s_btn_save, lv_pct(100));
    lv_obj_set_height(g_s_btn_save, 40);
    lv_obj_add_style(g_s_btn_save, &st_card, 0);
    lv_obj_add_event_cb(g_s_btn_save, save_apply_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* ls = lv_label_create(g_s_btn_save);
    lv_label_set_text(ls, "Save & Reconnect");
    lv_obj_add_style(ls, &st_text, 0);
    lv_obj_center(ls);

    g_s_btn_reload = lv_btn_create(content);
    lv_obj_set_width(g_s_btn_reload, lv_pct(100));
    lv_obj_set_height(g_s_btn_reload, 40);
    lv_obj_add_style(g_s_btn_reload, &st_card, 0);
    lv_obj_add_event_cb(g_s_btn_reload, reload_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lr = lv_label_create(g_s_btn_reload);
    lv_label_set_text(lr, "Reload data");
    lv_obj_add_style(lr, &st_text, 0);
    lv_obj_center(lr);

    update_values();
}

static void build_input_screen() {
    g_scr_input = lv_obj_create(NULL);
    lv_obj_clear_flag(g_scr_input, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(g_scr_input, &st_bg, 0);

    lv_obj_t* hdr = lv_obj_create(g_scr_input);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(hdr, 0, 0);
    lv_obj_set_size(hdr, 320, 34);
    lv_obj_add_style(hdr, &st_bar, 0);
    lv_obj_set_style_pad_all(hdr, 3, 0);

    lv_obj_t* btn_cancel = lv_btn_create(hdr);
    lv_obj_set_size(btn_cancel, 30, 26);
    lv_obj_align(btn_cancel, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_style(btn_cancel, &st_card, 0);
    lv_obj_add_event_cb(btn_cancel, input_cancel_btn_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lc = lv_label_create(btn_cancel);
    lv_label_set_text(lc, LV_SYMBOL_CLOSE);
    lv_obj_add_style(lc, &st_text, 0);
    lv_obj_center(lc);

    g_i_lbl_title = lv_label_create(hdr);
    lv_label_set_text(g_i_lbl_title, "Edit");
    lv_obj_add_style(g_i_lbl_title, &st_text, 0);
    lv_obj_center(g_i_lbl_title);

    lv_obj_t* btn_save = lv_btn_create(hdr);
    lv_obj_set_size(btn_save, 30, 26);
    lv_obj_align(btn_save, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_style(btn_save, &st_card, 0);
    lv_obj_add_event_cb(btn_save, input_save_btn_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* ls = lv_label_create(btn_save);
    lv_label_set_text(ls, LV_SYMBOL_OK);
    lv_obj_add_style(ls, &st_text, 0);
    lv_obj_center(ls);

    g_ta_input = lv_textarea_create(g_scr_input);
    lv_obj_set_size(g_ta_input, 300, 40);
    lv_obj_align(g_ta_input, LV_ALIGN_TOP_MID, 0, 38);
    lv_textarea_set_one_line(g_ta_input, true);
    lv_textarea_set_max_length(g_ta_input, 64);
    lv_obj_add_style(g_ta_input, &st_card, 0);
    lv_obj_add_style(g_ta_input, &st_text, 0);
    lv_obj_set_style_pad_all(g_ta_input, 6, 0);
    lv_obj_add_event_cb(g_ta_input, input_ready_cb, LV_EVENT_READY, nullptr);
    lv_obj_add_event_cb(g_ta_input, input_cancel_cb, LV_EVENT_CANCEL, nullptr);

    g_kb_input = lv_keyboard_create(g_scr_input);
    lv_obj_set_size(g_kb_input, 320, 156);
    lv_obj_align(g_kb_input, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(g_kb_input, g_ta_input);
}

void ui_init() {
    lv_style_init(&st_bg);
    lv_style_set_bg_opa(&st_bg, LV_OPA_COVER);
    lv_style_set_bg_color(&st_bg, lv_color_hex(col_bg()));
    lv_style_set_border_width(&st_bg, 0);
    lv_style_set_radius(&st_bg, 0);

    lv_style_init(&st_bar);
    lv_style_set_bg_opa(&st_bar, LV_OPA_COVER);
    lv_style_set_bg_color(&st_bar, lv_color_hex(col_bar()));
    lv_style_set_border_width(&st_bar, 0);
    lv_style_set_radius(&st_bar, 0);

    lv_style_init(&st_card);
    lv_style_set_bg_opa(&st_card, LV_OPA_COVER);
    lv_style_set_bg_color(&st_card, lv_color_hex(col_btn()));
    lv_style_set_radius(&st_card, 6);
    lv_style_set_border_width(&st_card, 0);
    lv_style_set_shadow_width(&st_card, 0);

    lv_style_init(&st_text);
    lv_style_set_text_color(&st_text, lv_color_hex(col_text()));

    lv_style_init(&st_dim);
    lv_style_set_text_color(&st_dim, lv_color_hex(col_dim()));

    build_main_screen();
    build_weather_view();
    build_stats_view();
    build_settings_screen();
    build_input_screen();
    lv_scr_load(g_scr);

    g_applied_ssid = cfg().ssid;
    g_applied_pass = cfg().pass;

    rebuild_pages();
    set_backlight(cfg().brightness);
}

// ---------------- Отладочный оверлей ----------------
static lv_obj_t* g_dbg_dot = nullptr;
static lv_obj_t* g_dbg_label = nullptr;

void ui_touch_debug_init() {
    static const int pts[5][2] = {{0, 0}, {319, 0}, {0, 239}, {319, 239}, {160, 120}};
    for (int i = 0; i < 5; i++) {
        lv_obj_t* m = lv_obj_create(lv_scr_act());
        lv_obj_set_size(m, 8, 8);
        lv_obj_set_pos(m, pts[i][0] - 4, pts[i][1] - 4);
        lv_obj_set_style_bg_color(m, lv_color_hex(0xFF0000), 0);
        lv_obj_set_style_border_width(m, 0, 0);
        lv_obj_set_style_radius(m, 4, 0);
        lv_obj_clear_flag(m, LV_OBJ_FLAG_SCROLLABLE);
    }

    g_dbg_dot = lv_obj_create(lv_scr_act());
    lv_obj_set_size(g_dbg_dot, 14, 14);
    lv_obj_set_style_bg_color(g_dbg_dot, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(g_dbg_dot, 0, 0);
    lv_obj_set_style_radius(g_dbg_dot, 7, 0);
    lv_obj_clear_flag(g_dbg_dot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_dbg_dot, LV_OBJ_FLAG_HIDDEN);

    g_dbg_label = lv_label_create(lv_scr_act());
    lv_obj_set_style_bg_color(g_dbg_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(g_dbg_label, LV_OPA_70, 0);
    lv_obj_set_style_text_color(g_dbg_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_all(g_dbg_label, 4, 0);
    lv_label_set_text(g_dbg_label, "X:--- Y:---");
    lv_obj_align(g_dbg_label, LV_ALIGN_BOTTOM_LEFT, 4, -4);
}

void ui_touch_debug_update(uint16_t x, uint16_t y, bool pressed) {
    if (g_dbg_dot) {
        if (pressed) {
            lv_obj_set_pos(g_dbg_dot, (int16_t)x - 7, (int16_t)y - 7);
            lv_obj_clear_flag(g_dbg_dot, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(g_dbg_dot, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (g_dbg_label) {
        String s = "X:" + String(x) + " Y:" + String(y);
        lv_label_set_text(g_dbg_label, s.c_str());
    }
}

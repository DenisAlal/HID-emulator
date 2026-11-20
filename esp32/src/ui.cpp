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

#define C_GOOD 0x2ECC71
#define C_WARN 0xF1C40F
#define C_BAD  0xE74C3C
#define MAX_KEYS 64

static KeyDef g_keys[MAX_KEYS];
static int  g_count = 0;
static bool g_wifi_connected = false;
static int  g_wifi_rssi = 0;

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

static void rebuild_grid();
static void apply_theme();
static void update_values();
static void open_editor(Field f, const char* title);
static void input_apply();
static void input_cancel();

// ---------------- Обработчики ----------------
static void btn_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= g_count) return;
    bool ok = netPressKey(g_keys[idx].id);
    ui_set_status(ok);
}

static void refresh_cb(lv_event_t* e) { ui_refresh_keys(); }
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
    rebuild_grid();
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

// ---------------- Сетка / тема ----------------
static void rebuild_grid() {
    if (!g_grid) return;
    lv_obj_clean(g_grid);

    if (g_count == 0) {
        lv_obj_set_flex_align(g_grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_t* lbl = lv_label_create(g_grid);
        lv_label_set_text(lbl, "No buttons.\nCheck server.");
        lv_obj_set_style_text_color(lbl, lv_color_hex(col_dim()), 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        return;
    }

    lv_obj_set_flex_align(g_grid, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

    for (int i = 0; i < g_count; i++) {
        lv_obj_t* btn = lv_btn_create(g_grid);
        lv_obj_set_size(btn, 92, 48);
        lv_obj_set_style_bg_color(btn, lv_color_hex(col_btn()), 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);
        lv_obj_set_style_pad_all(btn, 4, 0);
        lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);

        lv_obj_t* lbl = lv_label_create(btn);
        String t = String(i + 1) + ". " + g_keys[i].label;
        lv_label_set_text(lbl, t.c_str());
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl, lv_pct(90));
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(col_text()), 0);
        lv_obj_center(lbl);
    }
}

static void apply_theme() {
    if (!g_scr) return;
    lv_obj_set_style_bg_color(g_scr, lv_color_hex(col_bg()), 0);
    lv_obj_set_style_bg_color(g_bar, lv_color_hex(col_bar()), 0);
    lv_obj_set_style_bg_color(g_grid, lv_color_hex(col_bg()), 0);
    lv_obj_set_style_bg_color(g_btn_refresh, lv_color_hex(col_btn()), 0);
    lv_obj_set_style_bg_color(g_btn_settings, lv_color_hex(col_btn()), 0);
    lv_obj_set_style_text_color(g_lbl_refresh, lv_color_hex(col_text()), 0);
    lv_obj_set_style_text_color(g_lbl_settings, lv_color_hex(col_text()), 0);
    rebuild_grid();
    ui_set_wifi(g_wifi_connected, g_wifi_rssi);

    lv_style_set_bg_color(&st_bg, lv_color_hex(col_bg()));
    lv_style_set_bg_color(&st_bar, lv_color_hex(col_bar()));
    lv_style_set_bg_color(&st_card, lv_color_hex(col_btn()));
    lv_style_set_text_color(&st_text, lv_color_hex(col_text()));
    lv_style_set_text_color(&st_dim, lv_color_hex(col_dim()));
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
    lv_obj_set_style_pad_all(g_grid, 6, 0);
    lv_obj_set_style_pad_row(g_grid, 6, 0);
    lv_obj_set_style_pad_column(g_grid, 6, 0);
    lv_obj_set_pos(g_grid, 0, 30);
    lv_obj_set_size(g_grid, 320, 210);
    lv_obj_set_flex_flow(g_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(g_grid, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_add_flag(g_grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(g_grid, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_grid, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(g_grid, LV_OBJ_FLAG_SCROLL_ELASTIC);
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
    build_settings_screen();
    build_input_screen();
    lv_scr_load(g_scr);

    g_applied_ssid = cfg().ssid;
    g_applied_pass = cfg().pass;

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

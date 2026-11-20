#include "ui.h"
#include "net.h"
#include <lvgl.h>
#include <Preferences.h>

// ---- Тема (тёмная / светлая) ----
static bool g_dark = true;
static uint8_t g_brightness = 100;

static uint32_t col_bg()   { return g_dark ? 0x151515 : 0xF2F3F5; }
static uint32_t col_bar()  { return g_dark ? 0x1E1E1E : 0xE2E4E8; }
static uint32_t col_btn()  { return g_dark ? 0x2A2A2A : 0xFFFFFF; }
static uint32_t col_text() { return g_dark ? 0xFFFFFF : 0x1B1B1B; }
static uint32_t col_dim()  { return g_dark ? 0x9E9E9E : 0x6B6B6B; }

#define C_GOOD   0x2ECC71
#define C_WARN   0xF1C40F
#define C_BAD    0xE74C3C

#define MAX_KEYS 64

static KeyDef g_keys[MAX_KEYS];
static int g_count = 0;
static bool g_wifi_connected = false;
static int g_wifi_rssi = 0;

// главный экран
static lv_obj_t* g_scr = nullptr;
static lv_obj_t* g_bar = nullptr;
static lv_obj_t* g_grid = nullptr;
static lv_obj_t* g_wifi = nullptr;
static lv_obj_t* g_toast = nullptr;
static lv_obj_t* g_btn_refresh = nullptr;
static lv_obj_t* g_btn_settings = nullptr;

// экран настроек
static lv_obj_t* g_scr_settings = nullptr;
static lv_obj_t* g_switch_theme = nullptr;
static lv_obj_t* g_slider_bright = nullptr;
static lv_obj_t* g_label_bright = nullptr;
static lv_obj_t* g_s_lbl_title = nullptr;
static lv_obj_t* g_s_lbl_theme = nullptr;
static lv_obj_t* g_s_lbl_br = nullptr;
static lv_obj_t* g_s_btn_back = nullptr;
static lv_obj_t* g_s_btn_reload = nullptr;

static Preferences prefs;

static void rebuild_grid();
static void apply_theme();

// ---------------- Обработчики ----------------

static void btn_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= g_count) return;
    bool ok = netPressKey(g_keys[idx].id);
    String t = String(ok ? "OK: " : "ERR: ") + g_keys[idx].label;
    ui_show_toast(t.c_str());
}

static void refresh_cb(lv_event_t* e) { ui_refresh_keys(); }
static void settings_cb(lv_event_t* e) { ui_show_settings(); }

static void toast_hide_cb(lv_timer_t* t) {
    lv_obj_add_flag(g_toast, LV_OBJ_FLAG_HIDDEN);
    lv_timer_del(t);
}

static void back_cb(lv_event_t* e) { ui_hide_settings(); }

static void theme_cb(lv_event_t* e) {
    g_dark = lv_obj_has_state(g_switch_theme, LV_STATE_CHECKED);
    prefs.putBool("dark", g_dark);
    apply_theme();
}

static void bright_cb(lv_event_t* e) {
    g_brightness = (uint8_t)lv_slider_get_value(g_slider_bright);
    prefs.putUChar("bright", g_brightness);
    set_backlight(g_brightness);
    String s = String(g_brightness) + "%";
    lv_label_set_text(g_label_bright, s.c_str());
}

static void reload_cb(lv_event_t* e) { ui_refresh_keys(); }

// ---------------- Публичное ----------------

void ui_show_toast(const char* text) {
    if (!g_toast) return;
    lv_label_set_text(g_toast, text);
    lv_obj_clear_flag(g_toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_toast);
    lv_timer_create(toast_hide_cb, 1300, nullptr);
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
    ui_show_toast(ok ? "Updated" : "Server offline");
}

void ui_show_settings() {
    if (g_scr_settings) lv_scr_load(g_scr_settings);
}

void ui_hide_settings() {
    if (g_scr) lv_scr_load(g_scr);
}

// ---------------- Внутреннее ----------------

static void rebuild_grid() {
    if (!g_grid) return;
    lv_obj_clean(g_grid);

    if (g_count == 0) {
        lv_obj_t* lbl = lv_label_create(g_grid);
        lv_label_set_text(lbl, "No buttons.\nCheck server.");
        lv_obj_set_style_text_color(lbl, lv_color_hex(col_dim()), 0);
        lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(lbl);
        return;
    }

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
    lv_obj_set_style_bg_color(g_toast, lv_color_hex(col_btn()), 0);
    lv_obj_set_style_text_color(g_toast, lv_color_hex(col_text()), 0);
    rebuild_grid();
    ui_set_wifi(g_wifi_connected, g_wifi_rssi);

    if (g_scr_settings) {
        lv_obj_set_style_bg_color(g_scr_settings, lv_color_hex(col_bg()), 0);
        lv_obj_set_style_text_color(g_s_lbl_title, lv_color_hex(col_text()), 0);
        lv_obj_set_style_text_color(g_s_lbl_theme, lv_color_hex(col_text()), 0);
        lv_obj_set_style_text_color(g_s_lbl_br, lv_color_hex(col_text()), 0);
        lv_obj_set_style_text_color(g_label_bright, lv_color_hex(col_text()), 0);
        lv_obj_set_style_bg_color(g_s_btn_back, lv_color_hex(col_btn()), 0);
        lv_obj_set_style_bg_color(g_s_btn_reload, lv_color_hex(col_btn()), 0);
    }
}

static void build_main_screen() {
    g_scr = lv_scr_act();
    lv_obj_clear_flag(g_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_scr, lv_color_hex(col_bg()), 0);

    // ---- Статус-бар (фиксирован сверху) ----
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

    g_wifi = lv_label_create(g_bar);
    lv_label_set_text(g_wifi, LV_SYMBOL_WIFI " ...");
    lv_obj_set_style_text_color(g_wifi, lv_color_hex(col_dim()), 0);

    lv_obj_t* right = lv_obj_create(g_bar);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);
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
    lv_obj_t* l1 = lv_label_create(g_btn_refresh);
    lv_label_set_text(l1, LV_SYMBOL_REFRESH);
    lv_obj_center(l1);

    g_btn_settings = lv_btn_create(right);
    lv_obj_set_size(g_btn_settings, 28, 24);
    lv_obj_set_style_bg_color(g_btn_settings, lv_color_hex(col_btn()), 0);
    lv_obj_set_style_radius(g_btn_settings, 6, 0);
    lv_obj_add_event_cb(g_btn_settings, settings_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* l2 = lv_label_create(g_btn_settings);
    lv_label_set_text(l2, LV_SYMBOL_SETTINGS);
    lv_obj_center(l2);

    // ---- Сетка кнопок (скролл только вертикальный) ----
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

    // ---- Тост ----
    g_toast = lv_label_create(g_scr);
    lv_obj_add_flag(g_toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(g_toast, lv_color_hex(col_btn()), 0);
    lv_obj_set_style_bg_opa(g_toast, LV_OPA_90, 0);
    lv_obj_set_style_text_color(g_toast, lv_color_hex(col_text()), 0);
    lv_obj_set_style_pad_all(g_toast, 8, 0);
    lv_obj_set_style_radius(g_toast, 6, 0);
    lv_obj_align(g_toast, LV_ALIGN_BOTTOM_MID, 0, -6);
}

static void build_settings_screen() {
    g_scr_settings = lv_obj_create(NULL);
    lv_obj_clear_flag(g_scr_settings, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(g_scr_settings, lv_color_hex(col_bg()), 0);

    g_s_lbl_title = lv_label_create(g_scr_settings);
    lv_label_set_text(g_s_lbl_title, "Settings");
    lv_obj_set_style_text_color(g_s_lbl_title, lv_color_hex(col_text()), 0);
    lv_obj_align(g_s_lbl_title, LV_ALIGN_TOP_MID, 0, 8);

    g_s_btn_back = lv_btn_create(g_scr_settings);
    lv_obj_set_size(g_s_btn_back, 36, 28);
    lv_obj_set_pos(g_s_btn_back, 6, 6);
    lv_obj_set_style_bg_color(g_s_btn_back, lv_color_hex(col_btn()), 0);
    lv_obj_add_event_cb(g_s_btn_back, back_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lb = lv_label_create(g_s_btn_back);
    lv_label_set_text(lb, LV_SYMBOL_LEFT);
    lv_obj_center(lb);

    g_s_lbl_theme = lv_label_create(g_scr_settings);
    lv_label_set_text(g_s_lbl_theme, "Dark theme");
    lv_obj_set_style_text_color(g_s_lbl_theme, lv_color_hex(col_text()), 0);
    lv_obj_align(g_s_lbl_theme, LV_ALIGN_TOP_LEFT, 16, 50);

    g_switch_theme = lv_switch_create(g_scr_settings);
    lv_obj_set_size(g_switch_theme, 52, 28);
    lv_obj_align(g_switch_theme, LV_ALIGN_TOP_RIGHT, -16, 48);
    if (g_dark) lv_obj_add_state(g_switch_theme, LV_STATE_CHECKED);
    lv_obj_add_event_cb(g_switch_theme, theme_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    g_s_lbl_br = lv_label_create(g_scr_settings);
    lv_label_set_text(g_s_lbl_br, "Brightness");
    lv_obj_set_style_text_color(g_s_lbl_br, lv_color_hex(col_text()), 0);
    lv_obj_align(g_s_lbl_br, LV_ALIGN_TOP_LEFT, 16, 95);

    g_slider_bright = lv_slider_create(g_scr_settings);
    lv_obj_set_size(g_slider_bright, 160, 10);
    lv_obj_align(g_slider_bright, LV_ALIGN_TOP_LEFT, 20, 128);
    lv_slider_set_range(g_slider_bright, 5, 100);
    lv_slider_set_value(g_slider_bright, g_brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(g_slider_bright, bright_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    g_label_bright = lv_label_create(g_scr_settings);
    String bs = String(g_brightness) + "%";
    lv_label_set_text(g_label_bright, bs.c_str());
    lv_obj_set_style_text_color(g_label_bright, lv_color_hex(col_text()), 0);
    lv_obj_align(g_label_bright, LV_ALIGN_TOP_RIGHT, -16, 126);

    g_s_btn_reload = lv_btn_create(g_scr_settings);
    lv_obj_set_size(g_s_btn_reload, 180, 44);
    lv_obj_align(g_s_btn_reload, LV_ALIGN_TOP_MID, 0, 170);
    lv_obj_set_style_bg_color(g_s_btn_reload, lv_color_hex(col_btn()), 0);
    lv_obj_add_event_cb(g_s_btn_reload, reload_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* lr = lv_label_create(g_s_btn_reload);
    lv_label_set_text(lr, "Reload data");
    lv_obj_center(lr);
}

void ui_init() {
    prefs.begin("hidemu", false);
    g_dark = prefs.getBool("dark", true);
    g_brightness = prefs.getUChar("bright", 100);
    set_backlight(g_brightness);

    build_main_screen();
    build_settings_screen();
    lv_scr_load(g_scr);
}

// ============================================================
//  Отладочный оверлей тача (для калибровки)
// ============================================================
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

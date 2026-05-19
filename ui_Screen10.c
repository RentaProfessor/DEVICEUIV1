#include "ui.h"
#include "ui_widgets.h"

lv_obj_t *ui_Screen10 = NULL;

static void s10_close(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen4, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 250, 0, &ui_Screen4_screen_init); }
static void s10_pick(lv_event_t *e)  { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen6, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 250, 0, &ui_Screen6_screen_init); }

typedef struct { const char *num; const char *title; const char *dur; bool active; } chap_t;

void ui_Screen10_screen_init(void) {
    ui_Screen10 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen10, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen10, lv_color_hex(LT_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen10, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ltw_picker_header(ui_Screen10, "NAVIGATE / OVERDUB", "Select Chapter", "CANCEL", s10_close);

    chap_t chs[5] = {
        {"01", "CHILDHOOD IN KANSAS",       "12:34", false},
        {"02", "MEETING YOUR GRANDMOTHER",  "08:17", false},
        {"03", "THE EARLY YEARS",           "32:41", true},
        {"04", "NAVY DAYS",                 "19:08", false},
        {"05", "STARTING THE BUSINESS",     "24:55", false},
    };

    lv_obj_t *list = lv_obj_create(ui_Screen10);
    lv_obj_set_size(list, 748, 360);
    lv_obj_set_pos(list, 26, 102);
    lv_obj_set_style_bg_opa(list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(list, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);

    for (int i = 0; i < 5; i++) {
        lv_obj_t *row = lv_btn_create(list);
        lv_obj_set_size(row, 720, 56);
        lv_obj_set_pos(row, 0, i * 64);
        lv_obj_set_style_bg_color(row, lv_color_hex(chs[i].active ? LT_BURGUNDY : LT_PANEL_DARK), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(row, chs[i].active ? 255 : 200, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(row, lv_color_hex(chs[i].active ? LT_INK : LT_RULE), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(row, chs[i].active ? 2 : 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(row, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(row, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(row, s10_pick, LV_EVENT_ALL, NULL);

        lv_obj_t *n = lv_label_create(row);
        lv_obj_set_pos(n, 16, 16);
        lv_label_set_text(n, chs[i].num);
        lv_obj_set_style_text_color(n, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(n, &ui_font_archivo_32, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *t = lv_label_create(row);
        lv_obj_set_pos(t, 80, 19);
        lv_label_set_text(t, chs[i].title);
        lv_obj_set_style_text_color(t, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(t, &ui_font_Arhivo_regular_22, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *d = lv_label_create(row);
        lv_obj_align(d, LV_ALIGN_RIGHT_MID, chs[i].active ? -90 : -16, 0);
        lv_label_set_text(d, chs[i].dur);
        lv_obj_set_style_text_color(d, lv_color_hex(LT_INK_DIM), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(d, &ui_font_Arhivo_regular_18, LV_PART_MAIN | LV_STATE_DEFAULT);

        if (chs[i].active) {
            lv_obj_t *a = lv_label_create(row);
            lv_obj_align(a, LV_ALIGN_RIGHT_MID, -16, 0);
            lv_label_set_text(a, "ACTIVE");
            lv_obj_set_style_text_color(a, lv_color_hex(LT_AMBER), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(a, &ui_font_Arhivo_regular_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }
}

void ui_Screen10_screen_destroy(void) {
    if (ui_Screen10) lv_obj_del(ui_Screen10);
    ui_Screen10 = NULL;
}

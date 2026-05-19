#include "ui.h"
#include "ui_widgets.h"

lv_obj_t *ui_Screen8 = NULL;

static void s8_close(lv_event_t *e)    { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen4, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 250, 0, &ui_Screen4_screen_init); }
static void s8_pick(lv_event_t *e)     { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen4, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 250, 0, &ui_Screen4_screen_init); }
static void s8_newbook(lv_event_t *e)  { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen3, LV_SCR_LOAD_ANIM_MOVE_LEFT,  250, 0, &ui_Screen3_screen_init); }

typedef struct { const char *title; const char *date; uint32_t color; bool active; } book_t;

void ui_Screen8_screen_init(void) {
    ui_Screen8 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen8, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen8, lv_color_hex(LT_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen8, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ltw_picker_header(ui_Screen8, "YOUR SHELF", "Choose a Book", "CLOSE", s8_close);

    book_t books[3] = {
        {"Grandpa's Stories",  "04 19 2026", 0xE5853A, true},
        {"Grandma's Memoirs",  "03 12 2026", 0x3A5A7A, false},
        {"Uncle Terry's Tales","02 28 2026", 0x5A7A3A, false},
    };
    for (int i = 0; i < 3; i++) {
        lv_obj_t *tile = lv_btn_create(ui_Screen8);
        lv_obj_set_size(tile, 170, 170);
        lv_obj_set_pos(tile, 30 + i * 190, 110);
        lv_obj_set_style_bg_color(tile, lv_color_hex(0x1F1510), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(tile, lv_color_hex(0x3A2418), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_color(tile, lv_color_hex(books[i].active ? 0xE5B03A : 0x3A2418), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(tile, books[i].active ? 3 : 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(tile, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(tile, 10, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_add_event_cb(tile, s8_pick, LV_EVENT_ALL, NULL);

        lv_obj_t *band = lv_obj_create(tile);
        lv_obj_set_size(band, 150, 14);
        lv_obj_set_pos(band, 0, 0);
        lv_obj_clear_flag(band, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(band, lv_color_hex(books[i].color), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(band, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(band, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(band, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(band, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *t = lv_label_create(tile);
        lv_obj_set_pos(t, 0, 24);
        lv_obj_set_width(t, 150);
        lv_label_set_text(t, books[i].title);
        lv_obj_set_style_text_color(t, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(t, &ui_font_Arhivo_regular_18, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *d = lv_label_create(tile);
        lv_obj_set_pos(d, 0, 120);
        lv_label_set_text(d, books[i].date);
        lv_obj_set_style_text_color(d, lv_color_hex(0xE8D6A8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(d, &ui_font_Arhivo_regular_16, LV_PART_MAIN | LV_STATE_DEFAULT);

        if (books[i].active) {
            lv_obj_t *a = lv_label_create(tile);
            lv_obj_align(a, LV_ALIGN_TOP_RIGHT, 4, 24);
            lv_label_set_text(a, "ACTIVE");
            lv_obj_set_style_text_color(a, lv_color_hex(LT_AMBER), LV_PART_MAIN | LV_STATE_DEFAULT);
            lv_obj_set_style_text_font(a, &ui_font_Arhivo_regular_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        }
    }

    // NEW BOOK tile
    lv_obj_t *nb = lv_btn_create(ui_Screen8);
    lv_obj_set_size(nb, 170, 170);
    lv_obj_set_pos(nb, 600, 110);
    lv_obj_set_style_bg_color(nb, lv_color_hex(LT_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(nb, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(nb, lv_color_hex(0xF6ECD4), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(nb, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(nb, 130, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(nb, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_add_event_cb(nb, s8_newbook, LV_EVENT_ALL, NULL);

    lv_obj_t *plus = lv_label_create(nb);
    lv_obj_align(plus, LV_ALIGN_CENTER, 0, -16);
    lv_label_set_text(plus, "+");
    lv_obj_set_style_text_color(plus, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(plus, &ui_font_archivo_50, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *nbl = lv_label_create(nb);
    lv_obj_align(nbl, LV_ALIGN_CENTER, 0, 36);
    lv_label_set_text(nbl, "NEW BOOK");
    lv_obj_set_style_text_color(nbl, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(nbl, &ui_font_Arhivo_regular_18, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void ui_Screen8_screen_destroy(void) {
    if (ui_Screen8) lv_obj_del(ui_Screen8);
    ui_Screen8 = NULL;
}

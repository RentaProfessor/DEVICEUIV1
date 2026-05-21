#include "ui.h"
#include "ui_widgets.h"

lv_obj_t *ui_Screen7 = NULL;
lv_obj_t *ui_S7_Timer = NULL;

static void s7_to_chapter(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen10, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen10_screen_init); }
static void s7_to_book(lv_event_t *e)    { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen8,  LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen8_screen_init); }
static void s7_to_stopped(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen6,  LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen6_screen_init); }

void ui_Screen7_screen_init(void) {
    ui_Screen7 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen7, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen7, lv_color_hex(LT_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen7, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ltw_topbar(ui_Screen7, LT_GREEN, "PLAYING", "01:22 / 12:44", NULL, NULL, &ui_S7_Timer);
    ltw_cassette(ui_Screen7, 620, 230, -16, "Grandpa's Stories", "OUT");
    // REW/FF/STOP are physical Uxcell buttons (MCP23017 A2/A3/A4) — no touch zones needed.

    ltw_hw_legend(ui_Screen7,
                  NULL,       NULL,
                  "Playing",  NULL,
                  "Rewind",   NULL,
                  "Fast Fwd", NULL,
                  "Stop",     s7_to_stopped);
    ltw_chapter_banner(ui_Screen7, "CHAPTER 03", "THE EARLY YEARS", s7_to_chapter, s7_to_book);
}

void ui_Screen7_screen_destroy(void) {
    if (ui_Screen7) lv_obj_del(ui_Screen7);
    ui_Screen7 = NULL; ui_S7_Timer = NULL;
}

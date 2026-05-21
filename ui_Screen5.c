#include "ui.h"
#include "ui_widgets.h"

lv_obj_t *ui_Screen5 = NULL;
lv_obj_t *ui_S5_Timer = NULL;
lv_obj_t *ui_S5_PilotLamp = NULL;

static void s5_to_chapter(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen10, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen10_screen_init); }
static void s5_to_book(lv_event_t *e)    { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen8,  LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen8_screen_init); }
static void s5_to_stopped(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen6,  LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen6_screen_init); }

void ui_Screen5_screen_init(void) {
    ui_Screen5 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen5, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen5, lv_color_hex(LT_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen5, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ltw_topbar(ui_Screen5, LT_RED, "RECORDING", "00:00:00", &ui_S5_PilotLamp, NULL, &ui_S5_Timer);
    ltw_cassette(ui_Screen5, 620, 230, -16, "Grandpa's Stories", "REC");
    // STOP transition is handled by the physical STOP button via MCP23017 — no touch sim needed.
    ltw_hw_legend(ui_Screen5,
                  "Recording", NULL,
                  NULL,        NULL,
                  NULL,        NULL,
                  NULL,        NULL,
                  "Stop",      s5_to_stopped);
    ltw_chapter_banner(ui_Screen5, "CHAPTER 03", "THE EARLY YEARS", s5_to_chapter, s5_to_book);
}

void ui_Screen5_screen_destroy(void) {
    if (ui_Screen5) lv_obj_del(ui_Screen5);
    ui_Screen5 = NULL; ui_S5_Timer = NULL; ui_S5_PilotLamp = NULL;
}

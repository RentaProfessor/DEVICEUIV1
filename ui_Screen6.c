#include "ui.h"
#include "ui_widgets.h"

lv_obj_t *ui_Screen6 = NULL;
lv_obj_t *ui_S6_Timer = NULL;
lv_obj_t *ui_S6_PilotLamp = NULL;

static void s6_to_chapter(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen10, LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, &ui_Screen10_screen_init); }
static void s6_to_book(lv_event_t *e)    { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen8,  LV_SCR_LOAD_ANIM_MOVE_LEFT, 250, 0, &ui_Screen8_screen_init); }

void ui_Screen6_screen_init(void) {
    ui_Screen6 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen6, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen6, lv_color_hex(LT_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen6, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ltw_topbar(ui_Screen6, LT_AMBER, "STOPPED", "00:03:47", &ui_S6_PilotLamp, NULL, &ui_S6_Timer);
    // Cassette static — tape is parked. REC/PLAY/RWD/FF transitions handled by physical buttons.
    ltw_cassette(ui_Screen6, 620, 230, -16, "Grandpa's Stories", NULL);

    ltw_hw_legend(ui_Screen6, "Overdub", "Play", "Rewind", "Fast Fwd", NULL);
    ltw_chapter_banner(ui_Screen6, "CHAPTER 03 - 03:47", "THE EARLY YEARS", s6_to_chapter, s6_to_book);
}

void ui_Screen6_screen_destroy(void) {
    if (ui_Screen6) lv_obj_del(ui_Screen6);
    ui_Screen6 = NULL; ui_S6_Timer = NULL; ui_S6_PilotLamp = NULL;
}

#include "ui.h"
#include "ui_widgets.h"
#include "book.h"

lv_obj_t *ui_Screen7 = NULL;
lv_obj_t *ui_S7_Timer = NULL;

static void s7_to_chapter(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen10, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen10_screen_init); }
static void s7_to_book(lv_event_t *e)    { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen8,  LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen8_screen_init); }
static void s7_to_ready(lv_event_t *e)   { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen4,  LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen4_screen_init); }

// NOTE: Real playback streams the recording's WAV from Supabase Storage over
// HTTPS into the I2S speaker. That requires (a) the upload_chunk +
// finalize_recording Edge Functions deployed so recordings actually exist in
// the cloud, and (b) the device-side streaming player module. Until both are
// in place this screen shows honest status instead of fake scrubber data.
// No placeholder timestamps.

void ui_Screen7_screen_init(void) {
    ui_Screen7 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen7, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen7, lv_color_hex(LT_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen7, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ltw_topbar(ui_Screen7, LT_GREEN, "PLAYBACK", "", NULL, NULL, &ui_S7_Timer);
    ltw_cassette(ui_Screen7, 620, 200, -36, book_get_name(), NULL);

    // Honest status panel — no fake scrubber / timestamps
    lv_obj_t *msg = lv_label_create(ui_Screen7);
    lv_obj_set_width(msg, 620);
    lv_obj_set_pos(msg, 90, 280);
    lv_label_set_long_mode(msg, LV_LABEL_LONG_WRAP);
    lv_label_set_text(msg,
        "Playback streams your recordings from the cloud. "
        "It becomes available once a recording has finished syncing.");
    lv_obj_set_style_text_color(msg, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(msg, &ui_font_Arhivo_regular_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    // RWD/FF dimmed (no transport yet), STOP returns to Ready
    ltw_hw_legend(ui_Screen7,
                  NULL,       NULL,
                  NULL,       NULL,
                  "Rewind",   NULL,
                  "Fast Fwd", NULL,
                  "Stop",     s7_to_ready);

    // Real active book/chapter, not placeholders
    int ach = book_get_active_chapter();
    const char *acn = book_get_chapter_name(ach);
    char chnum[16];
    snprintf(chnum, sizeof(chnum), "CHAPTER %02d", ach + 1);
    ltw_chapter_banner(ui_Screen7, chnum, acn ? acn : "Chapter 1", s7_to_chapter, s7_to_book);
}

void ui_Screen7_screen_destroy(void) {
    if (ui_Screen7) lv_obj_del(ui_Screen7);
    ui_Screen7 = NULL; ui_S7_Timer = NULL;
}

#include "ui.h"
#include "ui_widgets.h"
#include "audio_record.h"
#include "audio_upload.h"
#include "book.h"

lv_obj_t *ui_Screen5 = NULL;
lv_obj_t *ui_S5_Timer = NULL;
lv_obj_t *ui_S5_PilotLamp = NULL;

static void s5_to_chapter(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        _ui_screen_change(&ui_Screen10, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen10_screen_init);
}
static void s5_to_book(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        _ui_screen_change(&ui_Screen8, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen8_screen_init);
}

// STOP button: stop capturing. Upload module finishes streaming the
// remaining chunks + sends finalize. Advance to Screen6 immediately —
// it will show upload progress as the last chunks land.
static void s5_to_stopped(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    printf("[S5] STOP pressed - finalizing\n");
    uint32_t secs = audio_record_seconds();
    audio_upload_request_finalize(secs, book_get_active_chapter());
    audio_record_stop();
    _ui_screen_change(&ui_Screen6, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen6_screen_init);
}

// Timer + VU updater — runs every 200ms via lv_timer while Screen5 is active
static lv_timer_t *s5_ticker = NULL;
static lv_obj_t   *s5_vu_bar = NULL;

static void s5_tick(lv_timer_t *t) {
    if (!ui_Screen5) return;
    if (audio_record_state() == AUDIO_STATE_RECORDING) {
        uint32_t s  = audio_record_seconds();
        char buf[16];
        snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
                 (unsigned)(s / 3600), (unsigned)((s / 60) % 60), (unsigned)(s % 60));
        if (ui_S5_Timer) lv_label_set_text(ui_S5_Timer, buf);
        // VU meter — set bar width as % of full
        if (s5_vu_bar) lv_obj_set_width(s5_vu_bar,
            (lv_coord_t)((uint32_t)audio_record_level_percent() * 600 / 100));
    }
}

void ui_Screen5_screen_init(void) {
    ui_Screen5 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen5, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen5, lv_color_hex(LT_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen5, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ltw_topbar(ui_Screen5, LT_RED, "RECORDING", "00:00:00", &ui_S5_PilotLamp, NULL, &ui_S5_Timer);

    ltw_cassette(ui_Screen5, 620, 200, -36, book_get_name(), "REC");

    // VU meter bar above the hw legend
    lv_obj_t *vu_track = lv_obj_create(ui_Screen5);
    lv_obj_set_size(vu_track, 600, 24);
    lv_obj_set_pos(vu_track, 100, 290);
    lv_obj_clear_flag(vu_track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(vu_track, lv_color_hex(0x1A1410), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(vu_track, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(vu_track, lv_color_hex(LT_RULE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(vu_track, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(vu_track, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(vu_track, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    s5_vu_bar = lv_obj_create(vu_track);
    lv_obj_set_size(s5_vu_bar, 0, 18);
    lv_obj_set_pos(s5_vu_bar, 0, 0);
    lv_obj_set_style_bg_color(s5_vu_bar, lv_color_hex(LT_RED_CTA), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s5_vu_bar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s5_vu_bar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(s5_vu_bar, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

    ltw_hw_legend(ui_Screen5,
                  "Recording", NULL,
                  NULL,        NULL,
                  NULL,        NULL,
                  NULL,        NULL,
                  "Stop",      s5_to_stopped);
    ltw_chapter_banner(ui_Screen5, "CHAPTER 03", "THE EARLY YEARS", s5_to_chapter, s5_to_book);

    // Auto-start recording when the screen is entered.
    // audio_record_start() resets all internal state + generates a fresh
    // session_id — no separate reset call needed (legacy API removed).
    if (audio_record_start()) {
        printf("[S5] mic capture started\n");
    } else {
        printf("[S5] mic capture failed to start\n");
    }
    if (!s5_ticker) s5_ticker = lv_timer_create(s5_tick, 200, NULL);
}

void ui_Screen5_screen_destroy(void) {
    if (s5_ticker) { lv_timer_del(s5_ticker); s5_ticker = NULL; }
    if (ui_Screen5) lv_obj_del(ui_Screen5);
    ui_Screen5 = NULL; ui_S5_Timer = NULL; ui_S5_PilotLamp = NULL; s5_vu_bar = NULL;
}

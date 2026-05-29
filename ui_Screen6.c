#include "ui.h"
#include "ui_widgets.h"
#include "audio_record.h"
#include "audio_upload.h"
#include "book.h"

lv_obj_t *ui_Screen6 = NULL;
lv_obj_t *ui_S6_Timer = NULL;
lv_obj_t *ui_S6_PilotLamp = NULL;
static lv_obj_t *ui_S6_StatusLabel = NULL;
static lv_obj_t *ui_S6_ProgressBar = NULL;
static lv_timer_t *s6_ticker       = NULL;

static void s6_to_chapter(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        _ui_screen_change(&ui_Screen10, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen10_screen_init);
}
static void s6_to_book(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        _ui_screen_change(&ui_Screen8, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen8_screen_init);
}
static void s6_to_rec(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        _ui_screen_change(&ui_Screen5, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen5_screen_init);
}
static void s6_to_play(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        _ui_screen_change(&ui_Screen7, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen7_screen_init);
}

static void s6_tick(lv_timer_t *t) {
    if (!ui_S6_StatusLabel) return;
    audio_state_t st = audio_record_state();
    char buf[96];
    uint32_t uploaded = audio_upload_chunks_uploaded();
    uint32_t total    = audio_record_chunks_captured();

    if (st == AUDIO_STATE_FINALIZING) {
        if (total > 0) {
            uint8_t pct = (uint8_t)((uint32_t)uploaded * 100 / total);
            snprintf(buf, sizeof(buf), "Uploading last chunks… %u / %u (%u%%)",
                     (unsigned)uploaded, (unsigned)total, pct);
            lv_label_set_text(ui_S6_StatusLabel, buf);
            if (ui_S6_ProgressBar) lv_obj_set_width(ui_S6_ProgressBar, (lv_coord_t)(600u * pct / 100u));
        } else {
            lv_label_set_text(ui_S6_StatusLabel, "Finalizing…");
        }
    } else if (st == AUDIO_STATE_COMPLETE) {
        // COMPLETE can mean "uploaded fine" OR "gave up with nothing uploaded".
        // If there's an error string, the upload did NOT succeed — say so.
        if (audio_upload_last_error()[0] != 0) {
            snprintf(buf, sizeof(buf), "Not uploaded: %.60s", audio_upload_last_error());
            lv_label_set_text(ui_S6_StatusLabel, buf);
            if (ui_S6_ProgressBar) lv_obj_set_width(ui_S6_ProgressBar, 0);
        } else {
            lv_label_set_text(ui_S6_StatusLabel, "Uploaded — transcribing on server");
            if (ui_S6_ProgressBar) lv_obj_set_width(ui_S6_ProgressBar, 600);
        }
    } else if (audio_upload_last_error()[0] != 0) {
        snprintf(buf, sizeof(buf), "Issue: %.70s", audio_upload_last_error());
        lv_label_set_text(ui_S6_StatusLabel, buf);
    } else {
        lv_label_set_text(ui_S6_StatusLabel, "Recording saved");
    }
}

void ui_Screen6_screen_init(void) {
    ui_Screen6 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen6, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen6, lv_color_hex(LT_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen6, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Show last recording duration in the timer
    uint32_t s = audio_record_seconds();
    char dur[16];
    snprintf(dur, sizeof(dur), "%02u:%02u:%02u",
             (unsigned)(s / 3600), (unsigned)((s / 60) % 60), (unsigned)(s % 60));

    ltw_topbar(ui_Screen6, LT_AMBER, "STOPPED", dur, &ui_S6_PilotLamp, NULL, &ui_S6_Timer);
    ltw_cassette(ui_Screen6, 620, 200, -36, book_get_name(), NULL);

    // Upload status + progress bar
    ui_S6_StatusLabel = lv_label_create(ui_Screen6);
    lv_obj_set_pos(ui_S6_StatusLabel, 100, 274);
    lv_obj_set_width(ui_S6_StatusLabel, 600);
    lv_label_set_text(ui_S6_StatusLabel, "Recording saved locally");
    lv_obj_set_style_text_color(ui_S6_StatusLabel, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ui_S6_StatusLabel, &ui_font_Arhivo_regular_18, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *track = lv_obj_create(ui_Screen6);
    lv_obj_set_size(track, 600, 8);
    lv_obj_set_pos(track, 100, 304);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(track, lv_color_hex(0x1A1410), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(track, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(track, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(track, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(track, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    ui_S6_ProgressBar = lv_obj_create(track);
    lv_obj_set_size(ui_S6_ProgressBar, 0, 8);
    lv_obj_set_pos(ui_S6_ProgressBar, 0, 0);
    lv_obj_set_style_bg_color(ui_S6_ProgressBar, lv_color_hex(LT_GREEN), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_S6_ProgressBar, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(ui_S6_ProgressBar, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(ui_S6_ProgressBar, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

    ltw_hw_legend(ui_Screen6,
                  "Overdub",  s6_to_rec,
                  "Play",     s6_to_play,
                  "Rewind",   NULL,
                  "Fast Fwd", NULL,
                  NULL,       NULL);
    ltw_chapter_banner(ui_Screen6, "CHAPTER 03", "THE EARLY YEARS", s6_to_chapter, s6_to_book);

    if (!s6_ticker) s6_ticker = lv_timer_create(s6_tick, 250, NULL);
}

void ui_Screen6_screen_destroy(void) {
    if (s6_ticker) { lv_timer_del(s6_ticker); s6_ticker = NULL; }
    if (ui_Screen6) lv_obj_del(ui_Screen6);
    ui_Screen6 = NULL;
    ui_S6_Timer = NULL;
    ui_S6_PilotLamp = NULL;
    ui_S6_StatusLabel = NULL;
    ui_S6_ProgressBar = NULL;
}

#include "ui.h"
#include "ui_widgets.h"
#include "book.h"
#include "audio_playback.h"

lv_obj_t *ui_Screen7 = NULL;
lv_obj_t *ui_S7_Timer = NULL;
static lv_obj_t   *s7_status   = NULL;
static lv_obj_t   *s7_progress = NULL;
static lv_obj_t   *s7_lamp     = NULL;
static lv_obj_t   *s7_vol_lbl  = NULL;
static lv_timer_t *s7_ticker   = NULL;

static void s7_vol_refresh(void) {
    if (!s7_vol_lbl) return;
    char b[24];
    snprintf(b, sizeof(b), "VOL  %d%%", audio_playback_get_volume());
    lv_label_set_text(s7_vol_lbl, b);
}
static void s7_vol_down(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    audio_playback_volume_step(-10);
    s7_vol_refresh();
}
static void s7_vol_up(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    audio_playback_volume_step(+10);
    s7_vol_refresh();
}

static void s7_to_chapter(lv_event_t *e) { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen10, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen10_screen_init); }
static void s7_to_book(lv_event_t *e)    { if (lv_event_get_code(e) == LV_EVENT_CLICKED) _ui_screen_change(&ui_Screen8,  LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen8_screen_init); }
static void s7_stop(lv_event_t *e) {
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    audio_playback_stop();
    _ui_screen_change(&ui_Screen4, LV_SCR_LOAD_ANIM_NONE, 0, 0, &ui_Screen4_screen_init);
}

static void fmt_mmss(uint32_t s, char *out, size_t n) {
    snprintf(out, n, "%02u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
}

static void s7_tick(lv_timer_t *t) {
    if (!s7_status) return;
    playback_state_t st = audio_playback_state();
    char buf[64];

    switch (st) {
        case PLAYBACK_FETCHING:
            lv_label_set_text(s7_status, "Loading recording…");
            break;
        case PLAYBACK_PLAYING: {
            uint32_t pos = audio_playback_position_sec();
            uint32_t dur = audio_playback_duration_sec();
            char a[8], b[8]; fmt_mmss(pos, a, sizeof(a)); fmt_mmss(dur, b, sizeof(b));
            snprintf(buf, sizeof(buf), "%s / %s", a, b);
            if (ui_S7_Timer) lv_label_set_text(ui_S7_Timer, buf);
            lv_label_set_text(s7_status, "Playing");
            if (s7_progress && dur > 0)
                lv_obj_set_width(s7_progress, (lv_coord_t)((uint32_t)pos * 600 / dur));
            break;
        }
        case PLAYBACK_DONE:
            lv_label_set_text(s7_status, "Finished — press STOP to go back");
            if (s7_progress) lv_obj_set_width(s7_progress, 600);
            break;
        case PLAYBACK_NONE:
            lv_label_set_text(s7_status, "No recording for this chapter yet");
            break;
        case PLAYBACK_ERROR:
            snprintf(buf, sizeof(buf), "Playback issue: %.45s", audio_playback_last_error());
            lv_label_set_text(s7_status, buf);
            break;
        default:
            lv_label_set_text(s7_status, "");
            break;
    }
}

void ui_Screen7_screen_init(void) {
    ui_Screen7 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen7, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen7, lv_color_hex(LT_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen7, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ltw_topbar(ui_Screen7, LT_GREEN, "PLAYBACK", "00:00 / 00:00", &s7_lamp, NULL, &ui_S7_Timer);
    ltw_cassette(ui_Screen7, 620, 200, -36, book_get_name(), NULL);

    // Status line
    s7_status = lv_label_create(ui_Screen7);
    lv_obj_set_width(s7_status, 620);
    lv_obj_set_pos(s7_status, 90, 278);
    lv_label_set_long_mode(s7_status, LV_LABEL_LONG_WRAP);
    lv_label_set_text(s7_status, "Loading recording…");
    lv_obj_set_style_text_color(s7_status, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s7_status, &ui_font_Arhivo_regular_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(s7_status, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Progress bar
    lv_obj_t *track = lv_obj_create(ui_Screen7);
    lv_obj_set_size(track, 600, 10);
    lv_obj_set_pos(track, 100, 308);
    lv_obj_clear_flag(track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(track, lv_color_hex(0x1A1410), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(track, 200, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(track, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(track, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(track, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    s7_progress = lv_obj_create(track);
    lv_obj_set_size(s7_progress, 0, 10);
    lv_obj_set_pos(s7_progress, 0, 0);
    lv_obj_set_style_bg_color(s7_progress, lv_color_hex(LT_GREEN), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s7_progress, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s7_progress, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(s7_progress, 5, LV_PART_MAIN | LV_STATE_DEFAULT);

    ltw_pulse_lamp(s7_lamp, 1600);   // gentle green pulse while playing

    // ── Transport row: VOL-  [VOL xx%]  VOL+ ............... STOP ──
    // (Replaces the 5-cap hw legend — only volume + stop matter during
    //  playback; RWD/FF aren't wired in v1.)
    const int ROW_Y = 330, BTN_H = 56;

    lv_obj_t *volDown = lv_btn_create(ui_Screen7);
    lv_obj_set_size(volDown, 96, BTN_H);
    lv_obj_set_pos(volDown, 70, ROW_Y);
    lv_obj_set_style_bg_color(volDown, lv_color_hex(0x2A1D14), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(volDown, lv_color_hex(0x4A3428), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(volDown, lv_color_hex(0x8A5A3A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(volDown, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(volDown, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *vdl = lv_label_create(volDown);
    lv_label_set_text(vdl, "VOL -");
    lv_obj_set_style_text_color(vdl, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(vdl, &ui_font_Arhivo_regular_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(vdl);
    lv_obj_add_event_cb(volDown, s7_vol_down, LV_EVENT_CLICKED, NULL);

    s7_vol_lbl = lv_label_create(ui_Screen7);
    lv_obj_set_size(s7_vol_lbl, 150, 30);
    lv_obj_set_pos(s7_vol_lbl, 178, ROW_Y + 14);
    lv_obj_set_style_text_color(s7_vol_lbl, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(s7_vol_lbl, &ui_font_Arhivo_regular_22, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(s7_vol_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN | LV_STATE_DEFAULT);
    s7_vol_refresh();

    lv_obj_t *volUp = lv_btn_create(ui_Screen7);
    lv_obj_set_size(volUp, 96, BTN_H);
    lv_obj_set_pos(volUp, 338, ROW_Y);
    lv_obj_set_style_bg_color(volUp, lv_color_hex(0x2A1D14), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(volUp, lv_color_hex(0x4A3428), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(volUp, lv_color_hex(0x8A5A3A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(volUp, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(volUp, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *vul = lv_label_create(volUp);
    lv_label_set_text(vul, "VOL +");
    lv_obj_set_style_text_color(vul, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(vul, &ui_font_Arhivo_regular_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(vul);
    lv_obj_add_event_cb(volUp, s7_vol_up, LV_EVENT_CLICKED, NULL);

    lv_obj_t *stopBtn = lv_btn_create(ui_Screen7);
    lv_obj_set_size(stopBtn, 180, BTN_H);
    lv_obj_set_pos(stopBtn, 550, ROW_Y);
    lv_obj_set_style_bg_color(stopBtn, lv_color_hex(LT_RED_CTA), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(stopBtn, lv_color_hex(LT_BURGUNDY), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_width(stopBtn, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(stopBtn, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *sbl = lv_label_create(stopBtn);
    lv_label_set_text(sbl, "STOP");
    lv_obj_set_style_text_color(sbl, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(sbl, &ui_font_Arhivo_regular_22, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(sbl);
    lv_obj_add_event_cb(stopBtn, s7_stop, LV_EVENT_CLICKED, NULL);

    int ach = book_get_active_chapter();
    const char *acn = book_get_chapter_name(ach);
    char chnum[16];
    snprintf(chnum, sizeof(chnum), "CHAPTER %02d", ach + 1);
    ltw_chapter_banner(ui_Screen7, chnum, acn ? acn : "Chapter 1", s7_to_chapter, s7_to_book);

    // Kick off streaming playback of the active chapter's recording
    audio_playback_start(ach);
    if (!s7_ticker) s7_ticker = lv_timer_create(s7_tick, 250, NULL);
}

void ui_Screen7_screen_destroy(void) {
    if (s7_ticker) { lv_timer_del(s7_ticker); s7_ticker = NULL; }
    ltw_stop_lamp_pulse(s7_lamp);
    audio_playback_stop();
    if (ui_Screen7) lv_obj_del(ui_Screen7);
    ui_Screen7 = NULL; ui_S7_Timer = NULL;
    s7_status = NULL; s7_progress = NULL; s7_lamp = NULL; s7_vol_lbl = NULL;
}

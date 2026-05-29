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

// Timer + VU updater — runs every 80ms via lv_timer while Screen5 is active.
// 80ms = ~12 fps, smooth enough for a VU meter without flooding LVGL.
#define VU_SEGMENTS 20
static lv_timer_t *s5_ticker = NULL;
static lv_obj_t   *s5_segs[VU_SEGMENTS] = {0};
static uint8_t     s5_peak_hold = 0;   // decaying peak indicator

static void s5_tick(lv_timer_t *t) {
    if (!ui_Screen5) return;
    audio_state_t st = audio_record_state();

    // Timer
    uint32_t s = audio_record_seconds();
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
             (unsigned)(s / 3600), (unsigned)((s / 60) % 60), (unsigned)(s % 60));
    if (ui_S5_Timer) lv_label_set_text(ui_S5_Timer, buf);

    // Segmented VU meter, lit proportional to live mic level.
    uint8_t level = (st == AUDIO_STATE_RECORDING) ? audio_record_level_percent() : 0;
    // Peak-hold: jumps up instantly, decays slowly — classic VU feel
    if (level > s5_peak_hold) s5_peak_hold = level;
    else if (s5_peak_hold > 2) s5_peak_hold -= 2;

    int lit = (level * VU_SEGMENTS) / 100;
    int peak_seg = (s5_peak_hold * VU_SEGMENTS) / 100;
    for (int i = 0; i < VU_SEGMENTS; i++) {
        uint32_t color;
        bool on = (i < lit) || (i == peak_seg);
        if (!on) {
            color = 0x2A1E14;   // dim/off
        } else if (i < VU_SEGMENTS * 7 / 10) {
            color = 0x4AC06A;   // green (safe)
        } else if (i < VU_SEGMENTS * 9 / 10) {
            color = 0xE5B03A;   // amber (loud)
        } else {
            color = 0xE53935;   // red (clipping)
        }
        if (s5_segs[i]) lv_obj_set_style_bg_color(s5_segs[i], lv_color_hex(color), LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

void ui_Screen5_screen_init(void) {
    ui_Screen5 = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_Screen5, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(ui_Screen5, lv_color_hex(LT_BG), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(ui_Screen5, 255, LV_PART_MAIN | LV_STATE_DEFAULT);

    ltw_topbar(ui_Screen5, LT_RED, "RECORDING", "00:00:00", &ui_S5_PilotLamp, NULL, &ui_S5_Timer);
    // Pulse the red RECORDING lamp — clear "we're live" animation
    ltw_pulse_lamp(ui_S5_PilotLamp, 1100);

    ltw_cassette(ui_Screen5, 620, 200, -36, book_get_name(), NULL);

    // Segmented VU meter — VU_SEGMENTS discrete bars, lit live by s5_tick.
    lv_obj_t *vu_track = lv_obj_create(ui_Screen5);
    lv_obj_set_size(vu_track, 620, 36);
    lv_obj_set_pos(vu_track, 90, 286);
    lv_obj_clear_flag(vu_track, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(vu_track, lv_color_hex(0x140E0A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(vu_track, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(vu_track, lv_color_hex(LT_RULE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(vu_track, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(vu_track, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(vu_track, 4, LV_PART_MAIN | LV_STATE_DEFAULT);

    const int seg_gap = 3;
    const int track_inner = 620 - 8;                 // minus padding
    const int seg_w = (track_inner - (VU_SEGMENTS - 1) * seg_gap) / VU_SEGMENTS;
    for (int i = 0; i < VU_SEGMENTS; i++) {
        lv_obj_t *seg = lv_obj_create(vu_track);
        lv_obj_set_size(seg, seg_w, 28);
        lv_obj_set_pos(seg, i * (seg_w + seg_gap), 0);
        lv_obj_clear_flag(seg, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(seg, lv_color_hex(0x2A1E14), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(seg, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(seg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(seg, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(seg, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        s5_segs[i] = seg;
    }
    s5_peak_hold = 0;

    ltw_hw_legend(ui_Screen5,
                  "Recording", NULL,
                  NULL,        NULL,
                  NULL,        NULL,
                  NULL,        NULL,
                  "Stop",      s5_to_stopped);
    // Chapter banner reads the real active chapter from NVS
    int ach = book_get_active_chapter();
    const char *acn = book_get_chapter_name(ach);
    char chnum[16];
    snprintf(chnum, sizeof(chnum), "CHAPTER %02d", ach + 1);
    ltw_chapter_banner(ui_Screen5, chnum, acn ? acn : "Chapter 1", s5_to_chapter, s5_to_book);

    // Auto-start recording when the screen is entered.
    // audio_record_start() resets all internal state + generates a fresh
    // session_id — no separate reset call needed (legacy API removed).
    if (audio_record_start()) {
        printf("[S5] mic capture started\n");
    } else {
        printf("[S5] mic capture failed to start\n");
    }
    if (!s5_ticker) s5_ticker = lv_timer_create(s5_tick, 80, NULL);
}

void ui_Screen5_screen_destroy(void) {
    if (s5_ticker) { lv_timer_del(s5_ticker); s5_ticker = NULL; }
    // Stop the lamp pulse before the object is freed so the anim doesn't
    // fire its callback on a dangling pointer.
    ltw_stop_lamp_pulse(ui_S5_PilotLamp);
    if (ui_Screen5) lv_obj_del(ui_Screen5);
    ui_Screen5 = NULL; ui_S5_Timer = NULL; ui_S5_PilotLamp = NULL;
    for (int i = 0; i < VU_SEGMENTS; i++) s5_segs[i] = NULL;
}

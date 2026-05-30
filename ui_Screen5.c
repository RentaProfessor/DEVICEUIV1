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

// Timer + VU updater — runs every 60ms via lv_timer while Screen5 is active.
// VU is a single SOLID bar that grows with the mic level (no segments, no
// floating peak dots). The displayed level is smoothed (fast attack, slow
// release) so it tracks speech naturally instead of flickering.
#define VU_TRACK_W 612                 // inner width of the meter track (px)
static lv_timer_t *s5_ticker  = NULL;
static lv_obj_t   *s5_vu_fill = NULL;  // the solid level bar
static int         s5_disp    = 0;     // smoothed displayed level 0..100

static void s5_tick(lv_timer_t *t) {
    if (!ui_Screen5) return;
    audio_state_t st = audio_record_state();

    // Timer
    uint32_t s = audio_record_seconds();
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u:%02u:%02u",
             (unsigned)(s / 3600), (unsigned)((s / 60) % 60), (unsigned)(s % 60));
    if (ui_S5_Timer) lv_label_set_text(ui_S5_Timer, buf);

    // Live mic level, smoothed: jump up fast, fall slow (natural VU ballistics).
    int level = (st == AUDIO_STATE_RECORDING) ? audio_record_level_percent() : 0;
    if (level > s5_disp) s5_disp = level;                       // instant attack
    else                 s5_disp += (level - s5_disp) / 3;      // gentle release
    if (s5_disp < 0) s5_disp = 0; if (s5_disp > 100) s5_disp = 100;

    if (s5_vu_fill) {
        lv_obj_set_width(s5_vu_fill, (lv_coord_t)(s5_disp * VU_TRACK_W / 100));
        // Solid bar, whole-bar colour by level: green / amber (loud) / red (hot)
        uint32_t c = (s5_disp < 70) ? 0x4AC06A : (s5_disp < 90) ? 0xE5B03A : 0xE53935;
        lv_obj_set_style_bg_color(s5_vu_fill, lv_color_hex(c), LV_PART_MAIN | LV_STATE_DEFAULT);
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

    // Solid VU meter: a dark track with one fill bar that grows with the level.
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

    s5_vu_fill = lv_obj_create(vu_track);
    lv_obj_set_size(s5_vu_fill, 0, 28);              // width set live in s5_tick
    lv_obj_align(s5_vu_fill, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_clear_flag(s5_vu_fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s5_vu_fill, lv_color_hex(0x4AC06A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(s5_vu_fill, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(s5_vu_fill, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(s5_vu_fill, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    s5_disp = 0;

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
    // Reset the uploader's per-recording counters first so last take's
    // uploaded-count/error don't leak into this session.
    audio_upload_reset();
    if (audio_record_start()) {
        printf("[S5] mic capture started\n");
    } else {
        printf("[S5] mic capture failed to start\n");
    }
    if (!s5_ticker) s5_ticker = lv_timer_create(s5_tick, 60, NULL);
}

void ui_Screen5_screen_destroy(void) {
    if (s5_ticker) { lv_timer_del(s5_ticker); s5_ticker = NULL; }
    // Stop the lamp pulse before the object is freed so the anim doesn't
    // fire its callback on a dangling pointer.
    ltw_stop_lamp_pulse(ui_S5_PilotLamp);
    if (ui_Screen5) lv_obj_del(ui_Screen5);
    ui_Screen5 = NULL; ui_S5_Timer = NULL; ui_S5_PilotLamp = NULL;
    s5_vu_fill = NULL;
}

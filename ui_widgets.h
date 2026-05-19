// Shared widget builders for Legacy Tape screens (Screens 5-14).
// Screen4 is kept fully inlined as the canonical reference.
#ifndef UI_WIDGETS_H
#define UI_WIDGETS_H
#ifdef __cplusplus
extern "C" {
#endif

#include "ui.h"

// Lamp/status colors per spec
#define LT_BG          0xC89060
#define LT_INK         0xF6ECD4
#define LT_INK_DIM     0xE8D6A8
#define LT_ACCENT      0xC89060
#define LT_PANEL_DARK  0x1A1410
#define LT_PANEL_DK2   0x1E1610
#define LT_RULE        0x5A3424
#define LT_BURGUNDY    0x8A3024
#define LT_RED         0xE53935
#define LT_RED_CTA     0xC94A2A
#define LT_GREEN       0x4AC06A
#define LT_AMBER       0xE5B03A
#define LT_INK_DARK    0x2A1A12

// Builds the 780x44 top bar at (10,10). Returns container; also sets out_status, out_timer.
lv_obj_t *ltw_topbar(lv_obj_t *parent,
                     uint32_t lamp_color,
                     const char *status_text,
                     const char *timer_text,
                     lv_obj_t **out_lamp,
                     lv_obj_t **out_status,
                     lv_obj_t **out_timer);

// Builds the 780x74 chapter banner at (10, 396) with CHAPTER + BOOK buttons.
// Pass NULL for events to skip wiring.
lv_obj_t *ltw_chapter_banner(lv_obj_t *parent,
                             const char *chapter_num,
                             const char *chapter_title,
                             lv_event_cb_t on_chapter,
                             lv_event_cb_t on_book);

// Builds the 5-cap hw legend row at y=308 for the Uxcell piano interlock buttons.
// Layout left-to-right: REC · PLAY · RWD · FF · STOP. Pass NULL for any label to
// render that cap dimmed (= button not meaningful in the current state).
void ltw_hw_legend(lv_obj_t *parent,
                   const char *lbl_rec,
                   const char *lbl_play,
                   const char *lbl_rwd,
                   const char *lbl_ff,
                   const char *lbl_stop);

// Builds a centered cassette panel with book title. w,h sized.
// Tag is "REC" / "OUT" or NULL to skip the VU meter on the right.
lv_obj_t *ltw_cassette(lv_obj_t *parent,
                       int w, int h, int y_offset,
                       const char *book_title,
                       const char *vu_tag);

// Picker header (used by Book Picker / Chapter Picker / Settings).
void ltw_picker_header(lv_obj_t *parent,
                       const char *eyebrow,
                       const char *title,
                       const char *close_label,
                       lv_event_cb_t on_close);

#ifdef __cplusplus
}
#endif
#endif

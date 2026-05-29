// Shared widget builders. See ui_widgets.h.
#include "ui_widgets.h"

static lv_obj_t *make_panel(lv_obj_t *parent, int w, int h, int x, int y) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_set_size(o, w, h);
    lv_obj_set_pos(o, x, y);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(o, lv_color_hex(LT_PANEL_DARK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(o, lv_color_hex(LT_PANEL_DK2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(o, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(o, lv_color_hex(LT_RULE), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(o, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(o, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(o, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    return o;
}

lv_obj_t *ltw_topbar(lv_obj_t *parent, uint32_t lamp_color,
                     const char *status_text, const char *timer_text,
                     lv_obj_t **out_lamp, lv_obj_t **out_status, lv_obj_t **out_timer) {
    lv_obj_t *bar = make_panel(parent, 780, 44, 10, 10);

    lv_obj_t *lamp = lv_obj_create(bar);
    lv_obj_set_size(lamp, 14, 14);
    lv_obj_set_pos(lamp, 16, 15);
    lv_obj_clear_flag(lamp, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(lamp, lv_color_hex(lamp_color), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(lamp, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(lamp, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(lamp, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (out_lamp) *out_lamp = lamp;

    lv_obj_t *status = lv_label_create(bar);
    lv_obj_set_pos(status, 40, 12);
    lv_label_set_text(status, status_text);
    lv_obj_set_style_text_color(status, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(status, &ui_font_Arhivo_regular_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(status, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (out_status) *out_status = status;

    lv_obj_t *timer = lv_label_create(bar);
    lv_obj_align(timer, LV_ALIGN_RIGHT_MID, -16, 0);
    lv_label_set_text(timer, timer_text);
    lv_obj_set_style_text_color(timer, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(timer, &ui_font_Arhivo_regular_22, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(timer, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    if (out_timer) *out_timer = timer;

    return bar;
}

lv_obj_t *ltw_chapter_banner(lv_obj_t *parent,
                             const char *chapter_num,
                             const char *chapter_title,
                             lv_event_cb_t on_chapter,
                             lv_event_cb_t on_book) {
    lv_obj_t *banner = make_panel(parent, 780, 74, 10, 396);

    lv_obj_t *cn = lv_label_create(banner);
    lv_obj_set_pos(cn, 18, 10);
    lv_label_set_text(cn, chapter_num);
    lv_obj_set_style_text_color(cn, lv_color_hex(LT_ACCENT), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(cn, &ui_font_Arhivo_regular_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(cn, 3, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *ct = lv_label_create(banner);
    lv_obj_set_pos(ct, 18, 32);
    lv_label_set_text(ct, chapter_title);
    lv_obj_set_style_text_color(ct, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ct, &ui_font_Arhivo_regular_22, LV_PART_MAIN | LV_STATE_DEFAULT);

    const char *labels[2] = {"CHAPTER", "BOOK"};
    lv_event_cb_t cbs[2] = {on_chapter, on_book};
    for (int i = 0; i < 2; i++) {
        lv_obj_t *b = lv_btn_create(banner);
        lv_obj_set_size(b, 160, 58);
        lv_obj_align(b, LV_ALIGN_RIGHT_MID, i == 0 ? -180 : -10, 0);
        lv_obj_set_style_bg_color(b, lv_color_hex(0xD4C090), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_color(b, lv_color_hex(0xF3E3B8), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(b, lv_color_hex(0x8A5A3A), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_border_color(b, lv_color_hex(0x8A5A3A), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(b, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(b, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_t *l = lv_label_create(b);
        lv_label_set_text(l, labels[i]);
        lv_obj_set_style_text_color(l, lv_color_hex(LT_INK_DARK), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(l, &ui_font_Arhivo_regular_18, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_center(l);
        if (cbs[i]) lv_obj_add_event_cb(b, cbs[i], LV_EVENT_ALL, NULL);
    }
    return banner;
}

void ltw_hw_legend(lv_obj_t *parent,
                   const char *lbl_rec,  lv_event_cb_t cb_rec,
                   const char *lbl_play, lv_event_cb_t cb_play,
                   const char *lbl_rwd,  lv_event_cb_t cb_rwd,
                   const char *lbl_ff,   lv_event_cb_t cb_ff,
                   const char *lbl_stop, lv_event_cb_t cb_stop) {
    const char    *caps[5] = {"REC", "PLAY", "RWD", "FF", "STOP"};
    const char    *lbls[5] = {lbl_rec,  lbl_play,  lbl_rwd,  lbl_ff,  lbl_stop};
    lv_event_cb_t  cbs[5]  = {cb_rec,   cb_play,   cb_rwd,   cb_ff,   cb_stop};
    uint32_t       cs[5]   = {0xC83A2A, 0x2A4830,  0x3A2A18, 0x3A2A18, 0x2A2018};
    uint32_t       cs_p[5] = {0x8A2418, 0x1A3020,  0x2A1E10, 0x2A1E10, 0x1A1410};  // pressed colors
    // Bigger, more finger-friendly: 84x48 (vs old 50x26)
    const int W = 84, H = 48;
    const int y_caps = 332;
    const int spacing = (800 - 5 * W) / 6;       // even gaps between caps
    for (int i = 0; i < 5; i++) {
        int x = spacing + i * (W + spacing);
        bool active   = (lbls[i] != NULL);
        bool tappable = (cbs[i]  != NULL);

        // Use a real button if tappable so we get press feedback for free
        lv_obj_t *c = tappable ? lv_btn_create(parent) : lv_obj_create(parent);
        lv_obj_set_size(c, W, H);
        lv_obj_set_pos(c, x, y_caps);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(c, lv_color_hex(cs[i]),   LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(c, lv_color_hex(cs_p[i]), LV_PART_MAIN | LV_STATE_PRESSED);
        lv_obj_set_style_bg_opa(c, active ? 255 : 90, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(c, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(c, tappable ? 2 : 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(c, lv_color_hex(0xF6ECD4), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_opa(c, 80, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(c, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
        if (tappable) lv_obj_add_event_cb(c, cbs[i], LV_EVENT_CLICKED, NULL);

        lv_obj_t *cl = lv_label_create(c);
        lv_obj_center(cl);
        lv_label_set_text(cl, caps[i]);
        lv_obj_set_style_text_color(cl, lv_color_hex(0xFFF0E0), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(cl, &ui_font_Arhivo_regular_22, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_letter_space(cl, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

lv_obj_t *ltw_cassette(lv_obj_t *parent, int w, int h, int y_offset,
                       const char *book_title, const char *vu_tag) {
    int x = (800 - w) / 2;
    int y = 64 + (322 - h) / 2 + y_offset;
    lv_obj_t *cass = lv_obj_create(parent);
    lv_obj_set_size(cass, w, h);
    lv_obj_set_pos(cass, x, y);
    lv_obj_clear_flag(cass, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(cass, lv_color_hex(0x1F1510), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(cass, lv_color_hex(0x170E0B), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(cass, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cass, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(cass, lv_color_hex(0x3A2418), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(cass, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(cass, 6, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(cass, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Cream label band
    lv_obj_t *band = lv_obj_create(cass);
    int bw = w - 80;
    lv_obj_set_size(band, bw, 80);
    lv_obj_align(band, LV_ALIGN_TOP_MID, 0, 25);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(band, lv_color_hex(0xF3E3B8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(band, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(band, lv_color_hex(0x3A2418), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(band, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(band, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(band, 6, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *titleL = lv_label_create(band);
    lv_obj_center(titleL);
    lv_label_set_text(titleL, book_title);
    lv_obj_set_style_text_color(titleL, lv_color_hex(LT_INK_DARK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(titleL, &ui_font_Arhivo_regular_22, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Reels
    int rsize = h / 3;
    int ry = h - rsize - 20;
    for (int i = 0; i < 2; i++) {
        lv_obj_t *reel = lv_obj_create(cass);
        lv_obj_set_size(reel, rsize, rsize);
        lv_obj_set_pos(reel, i == 0 ? (w / 4 - rsize / 2) : (3 * w / 4 - rsize / 2), ry);
        lv_obj_clear_flag(reel, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(reel, lv_color_hex(0x100808), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(reel, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(reel, LV_RADIUS_CIRCLE, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(reel, lv_color_hex(0xC8A868), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(reel, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(reel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    // NOTE: This widget used to draw a hardcoded 12-segment "VU meter" with
    // fake static green/amber levels whenever vu_tag was set. That was pure
    // placeholder filler — it never reflected real audio. Removed. Screens that
    // need a live meter (Screen5 Recording) build their own real segmented VU
    // driven by audio_record_level_percent(). The vu_tag param is kept for
    // source compatibility but no longer renders anything.
    (void)vu_tag;
    return cass;
}

// ─── Pulsing pilot lamp animation ───────────────────────────────────────────
static void lamp_opa_anim_cb(void *obj, int32_t v) {
    lv_obj_set_style_bg_opa((lv_obj_t *)obj, (lv_opa_t)v, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void ltw_pulse_lamp(lv_obj_t *lamp, uint32_t period_ms) {
    if (!lamp) return;
    lv_anim_del(lamp, lamp_opa_anim_cb);    // cancel any prior pulse on this lamp
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, lamp);
    lv_anim_set_exec_cb(&a, lamp_opa_anim_cb);
    lv_anim_set_values(&a, 255, 70);
    lv_anim_set_time(&a, period_ms / 2);
    lv_anim_set_playback_time(&a, period_ms / 2);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

void ltw_stop_lamp_pulse(lv_obj_t *lamp) {
    if (!lamp) return;
    lv_anim_del(lamp, lamp_opa_anim_cb);
    lv_obj_set_style_bg_opa(lamp, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void ltw_picker_header(lv_obj_t *parent,
                       const char *eyebrow, const char *title,
                       const char *close_label, lv_event_cb_t on_close) {
    lv_obj_t *eb = lv_label_create(parent);
    lv_obj_set_pos(eb, 26, 18);
    lv_label_set_text(eb, eyebrow);
    lv_obj_set_style_text_color(eb, lv_color_hex(0xE8D6A8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(eb, &ui_font_Arhivo_regular_16, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_letter_space(eb, 3, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *ti = lv_label_create(parent);
    lv_obj_set_pos(ti, 26, 40);
    lv_label_set_text(ti, title);
    lv_obj_set_style_text_color(ti, lv_color_hex(LT_INK), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(ti, &ui_font_archivo_42, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *cl = lv_btn_create(parent);
    lv_obj_set_size(cl, 140, 46);
    lv_obj_align(cl, LV_ALIGN_TOP_RIGHT, -20, 16);
    lv_obj_set_style_bg_color(cl, lv_color_hex(0x2A1D14), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(cl, lv_color_hex(0x4A3428), LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_border_color(cl, lv_color_hex(0x0A0604), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(cl, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(cl, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_t *cll = lv_label_create(cl);
    lv_label_set_text(cll, close_label);
    lv_obj_set_style_text_color(cll, lv_color_hex(0xE8D6A8), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(cll, &ui_font_Arhivo_regular_18, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(cll);
    if (on_close) {
        lv_obj_add_event_cb(cl, on_close, LV_EVENT_PRESSED, NULL);
        lv_obj_add_event_cb(cl, on_close, LV_EVENT_CLICKED, NULL);
    }
}

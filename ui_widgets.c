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

// Small filled circle helper for cassette detailing (hubs, teeth, screws, holes).
static lv_obj_t *cass_circle(lv_obj_t *p, int cx, int cy, int d, uint32_t color,
                            uint32_t border_color, int border_w) {
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_set_size(o, d, d);
    lv_obj_set_pos(o, cx - d / 2, cy - d / 2);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(o, 255, 0);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(o, border_w, 0);
    if (border_w) lv_obj_set_style_border_color(o, lv_color_hex(border_color), 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    return o;
}

// One tape reel: wound-tape disc + ivory hub + 6 teeth holes + spindle.
static void cass_reel(lv_obj_t *cass, int cx, int cy, int rsize) {
    // Hexagonal unit offsets for the 6 hub teeth (no math.h needed).
    static const float TX[6] = { 1.0f, 0.5f, -0.5f, -1.0f, -0.5f,  0.5f };
    static const float TY[6] = { 0.0f, 0.866f, 0.866f, 0.0f, -0.866f, -0.866f };

    cass_circle(cass, cx, cy, rsize,            0x241712, 0x3E2C1E, 2);   // wound tape pack
    cass_circle(cass, cx, cy, rsize * 64 / 100, 0xE8D6A8, 0x8A6A3A, 1);   // ivory hub
    int tr = rsize * 22 / 100;                                            // teeth ring radius
    int td = rsize * 13 / 100; if (td < 6) td = 6;                        // tooth diameter
    for (int k = 0; k < 6; k++)
        cass_circle(cass, cx + (int)(TX[k] * tr), cy + (int)(TY[k] * tr), td, 0x140C08, 0, 0);
    cass_circle(cass, cx, cy, rsize * 16 / 100, 0x0E0907, 0, 0);          // spindle hole
}

lv_obj_t *ltw_cassette(lv_obj_t *parent, int w, int h, int y_offset,
                       const char *book_title, const char *vu_tag) {
    (void)vu_tag;
    int x = (800 - w) / 2;
    int y = 64 + (322 - h) / 2 + y_offset;

    // ── Shell: molded warm-grey plastic with a soft vertical sheen ──
    lv_obj_t *cass = lv_obj_create(parent);
    lv_obj_set_size(cass, w, h);
    lv_obj_set_pos(cass, x, y);
    lv_obj_clear_flag(cass, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(cass, lv_color_hex(0x2C2622), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(cass, lv_color_hex(0x171210), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(cass, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cass, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(cass, lv_color_hex(0x0C0908), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(cass, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(cass, 12, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(cass, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    // Top edge highlight (thin lighter strip) for a glossy molded look
    lv_obj_t *sheen = lv_obj_create(cass);
    lv_obj_set_size(sheen, w - 24, 3);
    lv_obj_set_pos(sheen, 12, 6);
    lv_obj_clear_flag(sheen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(sheen, lv_color_hex(0x4A423C), 0);
    lv_obj_set_style_bg_opa(sheen, 160, 0);
    lv_obj_set_style_border_width(sheen, 0, 0);
    lv_obj_set_style_radius(sheen, 2, 0);

    // Four corner screws
    cass_circle(cass, 15,     15,     10, 0x0E0B09, 0x46403A, 2);
    cass_circle(cass, w - 15, 15,     10, 0x0E0B09, 0x46403A, 2);
    cass_circle(cass, 15,     h - 15, 10, 0x0E0B09, 0x46403A, 2);
    cass_circle(cass, w - 15, h - 15, 10, 0x0E0B09, 0x46403A, 2);

    // ── Cream J-card label with a colored header band + ruled lines ──
    int labelW = w - 96, labelH = h * 2 / 5, labelX = 48, labelY = 16;
    lv_obj_t *label = lv_obj_create(cass);
    lv_obj_set_size(label, labelW, labelH);
    lv_obj_set_pos(label, labelX, labelY);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(label, lv_color_hex(0xF5E8C2), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_color(label, lv_color_hex(0xE4CF9C), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_grad_dir(label, LV_GRAD_DIR_VER, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(label, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(label, lv_color_hex(0x8A6A3A), LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(label, 1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_radius(label, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(label, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    // Burgundy header band
    lv_obj_t *band = lv_obj_create(label);
    lv_obj_set_size(band, labelW, 18);
    lv_obj_set_pos(band, 0, 0);
    lv_obj_clear_flag(band, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(band, lv_color_hex(LT_BURGUNDY), 0);
    lv_obj_set_style_bg_opa(band, 255, 0);
    lv_obj_set_style_border_width(band, 0, 0);
    lv_obj_set_style_radius(band, 0, 0);
    lv_obj_set_style_pad_all(band, 0, 0);
    lv_obj_t *bl = lv_label_create(band);
    lv_obj_align(bl, LV_ALIGN_LEFT_MID, 8, 0);
    lv_label_set_text(bl, "SIDE A");
    lv_obj_set_style_text_color(bl, lv_color_hex(0xF6ECD4), 0);
    lv_obj_set_style_text_font(bl, &ui_font_Arhivo_regular_16, 0);
    lv_obj_set_style_text_letter_space(bl, 2, 0);
    lv_obj_t *br = lv_label_create(band);
    lv_obj_align(br, LV_ALIGN_RIGHT_MID, -8, 0);
    lv_label_set_text(br, "LEGACY TAPE");
    lv_obj_set_style_text_color(br, lv_color_hex(0xE8C9A0), 0);
    lv_obj_set_style_text_font(br, &ui_font_Arhivo_regular_16, 0);
    lv_obj_set_style_text_letter_space(br, 1, 0);

    // Title (the book name) centered in the label body
    lv_obj_t *titleL = lv_label_create(label);
    lv_obj_set_width(titleL, labelW - 24);
    lv_label_set_long_mode(titleL, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(titleL, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(titleL, LV_ALIGN_CENTER, 0, 6);
    lv_label_set_text(titleL, book_title);
    lv_obj_set_style_text_color(titleL, lv_color_hex(0x2A1A12), 0);
    lv_obj_set_style_text_font(titleL, &ui_font_Arhivo_regular_22, 0);
    // Two faint ruled lines under the title (handwriting guide feel)
    for (int r = 0; r < 2; r++) {
        lv_obj_t *rule = lv_obj_create(label);
        lv_obj_set_size(rule, labelW - 40, 1);
        lv_obj_align(rule, LV_ALIGN_BOTTOM_MID, 0, -10 + r * 9);
        lv_obj_clear_flag(rule, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(rule, lv_color_hex(0xB89A60), 0);
        lv_obj_set_style_bg_opa(rule, 130, 0);
        lv_obj_set_style_border_width(rule, 0, 0);
        lv_obj_set_style_radius(rule, 0, 0);
    }

    // ── Recessed tape window framing both reels ──
    int rsize  = h * 42 / 100;
    int reelCy = labelY + labelH + (h - (labelY + labelH)) / 2 - 4;
    int winH   = rsize + 14;
    lv_obj_t *win = lv_obj_create(cass);
    lv_obj_set_size(win, w * 76 / 100, winH);
    lv_obj_set_pos(win, w * 12 / 100, reelCy - winH / 2);
    lv_obj_clear_flag(win, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(win, lv_color_hex(0x0A0605), 0);
    lv_obj_set_style_bg_opa(win, 255, 0);
    lv_obj_set_style_border_color(win, lv_color_hex(0x37291E), 0);
    lv_obj_set_style_border_width(win, 1, 0);
    lv_obj_set_style_radius(win, 8, 0);
    lv_obj_set_style_pad_all(win, 0, 0);

    int leftCx  = w * 30 / 100;
    int rightCx = w * 70 / 100;
    // Exposed tape spanning between the two hubs
    lv_obj_t *tape = lv_obj_create(cass);
    lv_obj_set_size(tape, rightCx - leftCx, 5);
    lv_obj_set_pos(tape, leftCx, reelCy - 2);
    lv_obj_clear_flag(tape, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(tape, lv_color_hex(0x4A3320), 0);
    lv_obj_set_style_bg_opa(tape, 255, 0);
    lv_obj_set_style_border_width(tape, 0, 0);
    lv_obj_set_style_radius(tape, 0, 0);

    cass_reel(cass, leftCx,  reelCy, rsize);
    cass_reel(cass, rightCx, reelCy, rsize);

    // Bottom alignment holes (the little row along a cassette's lower edge)
    for (int k = -2; k <= 2; k++)
        cass_circle(cass, w / 2 + k * 16, h - 14, 6, 0x0A0605, 0, 0);

    return cass;
}

// ─── Pilot lamp ──────────────────────────────────────────────────────────────
// IMPORTANT: this used to run an INFINITE opacity animation to "pulse" the lamp.
// On this RGB panel the LVGL display is full_refresh=1, so every animation frame
// re-pushes the ENTIRE 800x480 framebuffer into the continuously-scanned panel
// memory — and because screens are never destroyed, the pulse kept running on
// hidden Recording/Playback screens forever, forcing a full-frame repaint
// ~30x/sec on whatever screen was actually visible. That is the on-tap/idle
// "glitch" (tearing). The lamp is now a SOLID indicator: liveness is shown by
// the topbar colour (red=rec, green=play), the live VU meter, and the timer —
// none of which churn the whole screen when idle.
void ltw_pulse_lamp(lv_obj_t *lamp, uint32_t period_ms) {
    (void)period_ms;
    if (!lamp) return;
    lv_obj_set_style_bg_opa(lamp, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
}

void ltw_stop_lamp_pulse(lv_obj_t *lamp) {
    if (!lamp) return;
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
    // CLICKED only. Previously this also bound LV_EVENT_PRESSED, so each tap
    // fired the close/navigation callback twice (once on touch-down, once on
    // release) — a double screen-change landmine.
    if (on_close) lv_obj_add_event_cb(cl, on_close, LV_EVENT_CLICKED, NULL);
}

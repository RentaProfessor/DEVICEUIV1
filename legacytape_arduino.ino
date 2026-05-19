/*
 * Legacy Tape — main Arduino sketch
 * Target: Elecrow CrowPanel Advance 5" HMI · ESP32-S3-WROOM-1-N16R8 · 800x480 IPS · GT911 single-touch
 *
 * Verified hardware bindings (from components.pdf + CrowPanel readme + LovyanGFX_Driver.h):
 *   Display:    RGB parallel (16 data + HSYNC/VSYNC/DE/PCLK). Driver ST7262. PSRAM-backed.
 *   Touch:      GT911 single-touch on Wire (SDA=15, SCL=16) @ 400 kHz, addr 0x5D.
 *               Power-up sequence: GPIO 1 OUTPUT LOW 120 ms, then INPUT (selects 0x5D).
 *   I2C bus:    RTC BM8563 · TCA9534 IO expander (0x18, V1.0/V1.1 backlight enable) ·
 *               STC8H1K28 backlight + buzzer µC (0x30, V1.2) ·
 *               Waveshare MCP23017 IO expander (0x20, our 5-button input) ·
 *               GT911 touch (0x5D).
 *
 *   Buttons:    Uxcell 5-position piano interlock — REC · PLAY · RWD · FF · STOP.
 *               Wired to MCP23017 port A pins 0-4 (active LOW with internal pull-ups).
 *               Polled every 20 ms with software debounce.
 *   Mic:        INMP441 I2S MEMS (BCLK=19, WS=2, DIN=20). 26 dBA, mono.
 *   Speaker:    MAX98357 I2S Class-D amp on panel SPK port (DOUT=4, BCLK=5, LRC=6).
 *               NOTE: shares pins with SD card SPI. Pick one at a time.
 *   Backlight:  TCA9534 pin 1 HIGH (V1.0/V1.1) AND I2C 0x30 brightness byte (V1.2). Both attempted —
 *               whichever IC is on your board responds, the other harmlessly NACKs.
 *
 * NOT wired yet (firmware modules to add later):
 *   I2S record/playback · SD card mount · WiFi/BLE pairing · RTC sync · rotary encoder for volume
 */

#include "LovyanGFX_Driver.h"
#include <lvgl.h>
#include <Wire.h>
#include "ui.h"
#include "pins_config.h"

// No external I/O-expander libraries needed for the preliminary build —
// we talk to every I2C peripheral (backlight, MCP23017) directly through Wire.

LGFX gfx;

// ─── LVGL plumbing ──────────────────────────────────────────────────────────
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = NULL;
static lv_color_t *buf2 = NULL;
static uint16_t touch_x = 0, touch_y = 0;

static void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    gfx.pushImageDMA(area->x1, area->y1, w, h, (uint16_t *)&color_p->full);
    lv_disp_flush_ready(disp);
}

static void touchpad_read(lv_indev_drv_t *indev, lv_indev_data_t *data) {
    bool touched = gfx.getTouch(&touch_x, &touch_y);
    if (touched) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = touch_x;
        data->point.y = touch_y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

// ─── Backlight + buzzer: try both V1.0/V1.1 (TCA9534 @0x18) and V1.2 (STC8H1K28 @0x30) paths ──
// Whichever IC is on your panel ACKs; the other NACKs harmlessly. No library required.
static void backlight_init() {
    // V1.0/V1.1 path: TCA9534 IO expander at 0x18.
    //   Config register (0x03): 1=input, 0=output. Set pins 1-4 as outputs → 0xE1.
    //   Output register (0x01): drive pin 1 (backlight rail), 2 (panel reset deassert), 4 high.
    Wire.beginTransmission(0x18); Wire.write(0x03); Wire.write(0xE1); Wire.endTransmission();
    Wire.beginTransmission(0x18); Wire.write(0x01); Wire.write(0x16); Wire.endTransmission();

    // V1.2 path: STC8H1K28 µC at 0x30. Byte 0 = max brightness.
    Wire.beginTransmission(0x30); Wire.write((uint8_t)0); Wire.endTransmission();
}

static void backlight_set(uint8_t level) {
    Wire.beginTransmission(0x30); Wire.write(level); Wire.endTransmission();
}

// ─── MCP23017 IO expander (0x20) — Uxcell piano interlock buttons on PORTA pins 0-4 ──
#define MCP_ADDR     0x20
#define MCP_IODIRA   0x00
#define MCP_GPPUA    0x0C
#define MCP_GPIOA    0x12

#define BTN_REC  0x01    // MCP A0
#define BTN_PLAY 0x02    // MCP A1
#define BTN_RWD  0x04    // MCP A2
#define BTN_FF   0x08    // MCP A3
#define BTN_STOP 0x10    // MCP A4
#define BTN_MASK 0x1F    // pins 0-4

static uint8_t mcp_write_reg(uint8_t reg, uint8_t val) {
    Wire.beginTransmission(MCP_ADDR);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission();
}

static uint8_t mcp_read_reg(uint8_t reg) {
    Wire.beginTransmission(MCP_ADDR);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return 0xFF;   // no MCP on bus → return "no buttons"
    Wire.requestFrom((uint8_t)MCP_ADDR, (uint8_t)1);
    return Wire.available() ? Wire.read() : 0xFF;
}

static void buttons_init() {
    // Pins 0-4 as input (1 bits in IODIR = input)
    mcp_write_reg(MCP_IODIRA, BTN_MASK);
    // Internal pull-ups on the same pins
    mcp_write_reg(MCP_GPPUA, BTN_MASK);
}

// State-machine: read raw button state, debounce, dispatch transitions on the falling edge
// (i.e. the moment a button locks down — the mechanical interlock guarantees only one is held).
static lv_obj_t *current_active() {
    return lv_disp_get_scr_act(lv_disp_get_default());
}

static void on_button_pressed(uint8_t btn) {
    lv_obj_t *cur = current_active();
    switch (btn) {
        case BTN_REC:
            if (cur == ui_Screen4 || cur == ui_Screen6 || cur == ui_Screen12)
                _ui_screen_change(&ui_Screen5, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, &ui_Screen5_screen_init);
            break;
        case BTN_PLAY:
            if (cur == ui_Screen4 || cur == ui_Screen6)
                _ui_screen_change(&ui_Screen7, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, &ui_Screen7_screen_init);
            break;
        case BTN_STOP:
            if (cur == ui_Screen5 || cur == ui_Screen7 || cur == ui_Screen9 || cur == ui_Screen13)
                _ui_screen_change(&ui_Screen6, LV_SCR_LOAD_ANIM_FADE_ON, 200, 0, &ui_Screen6_screen_init);
            break;
        case BTN_RWD:
        case BTN_FF:
            // In firmware these adjust playback/scrub position. No screen change today.
            // TODO: when audio module exists, emit a seek event here.
            break;
    }
}

static void buttons_poll() {
    static uint8_t last_state = BTN_MASK;     // all high = nothing pressed
    static uint32_t last_change_ms = 0;
    const uint32_t DEBOUNCE_MS = 20;

    uint8_t raw = mcp_read_reg(MCP_GPIOA);
    if (raw == 0xFF) return;                  // bus error / MCP not present
    uint8_t state = raw & BTN_MASK;

    if (state == last_state) return;
    if (millis() - last_change_ms < DEBOUNCE_MS) return;
    last_change_ms = millis();

    // Detect falling edges: bits that went from 1 (released) to 0 (pressed)
    uint8_t falling = last_state & ~state;
    last_state = state;

    if (falling & BTN_REC)  on_button_pressed(BTN_REC);
    if (falling & BTN_PLAY) on_button_pressed(BTN_PLAY);
    if (falling & BTN_RWD)  on_button_pressed(BTN_RWD);
    if (falling & BTN_FF)   on_button_pressed(BTN_FF);
    if (falling & BTN_STOP) on_button_pressed(BTN_STOP);
}

// ─── Onboarding nav glue (Screen1 → Screen2 → Screen3 → Screen4) ───────────
// Wires the existing SquareLine screens without modifying the generated files.
static void s1_advance(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_CLICKED)
        _ui_screen_change(&ui_Screen2, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, &ui_Screen2_screen_init);
}
static void s3_kb_done(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_READY || lv_event_get_code(e) == LV_EVENT_CANCEL)
        _ui_screen_change(&ui_Screen4, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, &ui_Screen4_screen_init);
}

static void wire_existing_screens() {
    if (ui_Screen1) lv_obj_add_event_cb(ui_Screen1, s1_advance, LV_EVENT_CLICKED, NULL);
    if (ui_Screen3 && ui_Keyboard2) {
        lv_keyboard_set_textarea(ui_Keyboard2, ui_TextArea1);
        lv_obj_add_event_cb(ui_Keyboard2, s3_kb_done, LV_EVENT_ALL, NULL);
    }
}

// ─── Setup / loop ──────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[LegacyTape] booting");

    // I2C up first so touch + backlight + MCP23017 can ACK.
    Wire.begin(15, 16);
    Wire.setClock(400000);
    delay(50);

    // GT911 power-on address selection (selects 0x5D over 0x14)
    pinMode(1, OUTPUT);
    digitalWrite(1, LOW);
    delay(120);
    pinMode(1, INPUT);

    backlight_init();
    buttons_init();

    // Display & touch (RGB parallel + GT911 via LovyanGFX).
    gfx.init();
    gfx.initDMA();
    gfx.setBrightness(255);
    gfx.fillScreen(TFT_BLACK);
    backlight_set(0);          // 0 = full brightness on STC8H1K28

    // LVGL init + framebuffers in PSRAM (8 MB available, 1.5 MB double buffer is fine).
    lv_init();
    buf1 = (lv_color_t *)heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t *)heap_caps_malloc(LCD_H_RES * LCD_V_RES * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!buf1 || !buf2) {
        Serial.println("[LegacyTape] PSRAM alloc failed! Enable OPI PSRAM in board settings.");
        while (1) delay(100);
    }
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, LCD_H_RES * LCD_V_RES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = LCD_H_RES;
    disp_drv.ver_res = LCD_V_RES;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.full_refresh = 1;             // avoid tearing on RGB panel
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);

    ui_init();
    wire_existing_screens();

    Serial.println("[LegacyTape] ready");
}

void loop() {
    lv_timer_handler();
    buttons_poll();
    delay(5);
}

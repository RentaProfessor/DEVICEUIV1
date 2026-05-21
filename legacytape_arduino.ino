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
#include "pairing.h"
#include "pairing_ble.h"
#include "cloud_sync.h"
#include "book.h"
#include "audio_record.h"
#include "audio_upload.h"

// No external I/O-expander libraries needed for the preliminary build —
// we talk to every I2C peripheral (backlight, MCP23017) directly through Wire.

LGFX gfx;

// ─── LVGL plumbing ──────────────────────────────────────────────────────────
static lv_disp_draw_buf_t draw_buf;
static lv_color_t *buf1 = NULL;
static lv_color_t *buf2 = NULL;
static uint16_t touch_x = 0, touch_y = 0;

static void disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    // Factory pattern: gfx.startWrite() is held open after setup, so we must
    // close the existing transaction before pushing DMA pixels.
    if (gfx.getStartCount() > 0) {
        gfx.endWrite();
    }
    gfx.pushImageDMA(area->x1, area->y1,
                     area->x2 - area->x1 + 1,
                     area->y2 - area->y1 + 1,
                     (lgfx::rgb565_t *)&color_p->full);
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

// ─── Backlight init: V1.1 protocol (also works on V1.0 / V1.2) ────────────
// V1.1 requires a two-step dance:
//   1. The STC8H1K28 µC at I2C 0x30 boots slower than the ESP32.
//      We must wait for both the µC (0x30) and GT911 touch (0x5D) to ACK on I2C.
//      While waiting, we send command 0x19 to the µC to kick it into a responsive state.
//   2. Once the µC is up, send byte 0x10 to 0x30 → max brightness backlight ON.
//
// V1.0 hardware uses TCA9534 @ 0x18 for backlight rail enable — also done here harmlessly.
static void sendI2CCommand(uint8_t command) {
    Wire.beginTransmission(0x30);
    Wire.write(command);
    Wire.endTransmission();
}

static bool i2cScanForAddress(uint8_t address) {
    Wire.beginTransmission(address);
    return Wire.endTransmission() == 0;
}

static void backlight_init() {
    // V1.0 path: TCA9534 IO expander — drive pins 1, 2, 4 HIGH (backlight rail + reset)
    Wire.beginTransmission(0x18); Wire.write(0x03); Wire.write(0xE1); Wire.endTransmission();
    Wire.beginTransmission(0x18); Wire.write(0x01); Wire.write(0x16); Wire.endTransmission();

    // V1.1 + V1.2 path: wake the STC8H1K28 µC, wait for it + GT911 to come online,
    // then send max-brightness command. Up to ~5 second timeout.
    int tries = 0;
    while (tries < 50) {
        if (i2cScanForAddress(0x30) && i2cScanForAddress(0x5D)) {
            Serial.println("[LegacyTape] microcontroller (0x30) + touch (0x5D) ready");
            break;
        }
        Serial.printf("[LegacyTape] waking microcontroller (try %d)\n", tries);
        sendI2CCommand(0x19);                  // wake the µC
        pinMode(1, OUTPUT);                    // GT911 reset sequence
        digitalWrite(1, LOW);
        delay(120);
        pinMode(1, INPUT);
        delay(100);
        tries++;
    }

    sendI2CCommand(0x10);                      // backlight ON, max brightness
}

// V1.1 valid range: 0x05 (off) .. 0x10 (max). V1.2 valid range: 0 (max) .. 244 (min), 245 (off).
static void backlight_set(uint8_t level) {
    sendI2CCommand(level);
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
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        // Capture whatever the user typed in the textarea and persist it
        // so Screen4's cassette label shows the real book name.
        if (ui_TextArea1) {
            const char *typed = lv_textarea_get_text(ui_TextArea1);
            if (typed && strlen(typed) > 0) {
                book_set_name(typed);
            }
        }
        _ui_screen_change(&ui_Screen4, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, &ui_Screen4_screen_init);
    }
}

// Debug: log every event on Screen2's "NAME YOUR BOOK" button so we can see
// whether LVGL is receiving touch presses on it after BLE/WiFi teardown.
static void s2_button_debug(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED)        Serial.println("[debug] Screen2 button PRESSED");
    else if (code == LV_EVENT_RELEASED)  Serial.println("[debug] Screen2 button RELEASED");
    else if (code == LV_EVENT_CLICKED)   Serial.println("[debug] Screen2 button CLICKED (should advance to Screen3)");
}

static void wire_existing_screens() {
    if (ui_Screen1) lv_obj_add_event_cb(ui_Screen1, s1_advance, LV_EVENT_CLICKED, NULL);
    if (ui_Screen3 && ui_Keyboard2) {
        lv_keyboard_set_textarea(ui_Keyboard2, ui_TextArea1);
        lv_obj_add_event_cb(ui_Keyboard2, s3_kb_done, LV_EVENT_ALL, NULL);
    }
    // Diagnostic: log all events on Screen2's button so we know if taps register
    if (ui_Button1) lv_obj_add_event_cb(ui_Button1, s2_button_debug, LV_EVENT_ALL, NULL);
}

// ─── Setup / loop ──────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("[LegacyTape] booting");

    // I2S mic BCLK pin must be set as output before I2C (factory code does this)
    pinMode(19, OUTPUT);

    // I2C up first so touch + backlight + MCP23017 can ACK.
    Wire.begin(15, 16);
    Wire.setClock(400000);
    delay(50);

    // Wake the backlight µC, wait for it + GT911, then enable backlight.
    // (GT911 reset on GPIO 1 happens inside backlight_init's wait loop.)
    backlight_init();
    buttons_init();

    // Display & touch (RGB parallel + GT911 via LovyanGFX).
    gfx.init();
    gfx.initDMA();
    gfx.startWrite();
    gfx.fillScreen(TFT_BLACK);

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
    // RGB panels + many widgets: full_refresh pushes one complete frame per cycle
    // instead of many small DMA bursts that overlap with the panel's scanout and
    // cause visible tearing. Both buffers are full-screen sized, so this works.
    disp_drv.full_refresh = 1;
    lv_disp_drv_register(&disp_drv);

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);

    ui_init();
    wire_existing_screens();

    // Load book name + chapter list from NVS before anything renders, so
    // Screen4's cassette + Screen10's chapter list show real data.
    book_begin();

    // Allocate the recording PSRAM buffers + unmute mic. Runs once at boot.
    audio_record_begin();
    // Start the background chunk-upload task. Idle until audio_record produces chunks.
    audio_upload_begin();

    // Initialize pairing token + persist it in NVS. Render a live QR code on
    // Screen1 encoding the pairing URL. iOS app scans this and uses BLE to
    // deliver WiFi credentials back to the device.
    pairing_begin();

    // Start BLE advertising for the pairing service. No-op if already paired.
    pairing_ble_begin();

    if (ui_Screen1) {
        // Hide the static QR-card placeholder image — we draw our own card + QR.
        if (ui_Image1) lv_obj_add_flag(ui_Image1, LV_OBJ_FLAG_HIDDEN);

        // Cream card on the right side of Screen1. Sized to fit QR + 2 labels
        // with even visual breathing room top-to-bottom.
        lv_obj_t *card = lv_obj_create(ui_Screen1);
        lv_obj_set_size(card, 280, 320);
        lv_obj_align(card, LV_ALIGN_RIGHT_MID, -30, 0);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_color(card, lv_color_hex(0xF6ECD4), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_bg_opa(card, 255, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_color(card, lv_color_hex(0x8A3024), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_border_width(card, 2, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_radius(card, 4, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_pad_all(card, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

        // QR code positioned with explicit offsets so spacing is predictable
        // regardless of LVGL flex/padding behavior.
        // Card is 320 tall. Layout target:
        //   16 px top margin
        //   210 px QR
        //   24 px gap
        //   ~22 px "SCAN WITH APP"
        //   ~18 px "LT-XXXXXX"
        //   30 px bottom margin
        lv_obj_t *qr = lv_qrcode_create(card, 210,
                                        lv_color_hex(0x2A1A12),
                                        lv_color_hex(0xF6ECD4));
        lv_obj_align(qr, LV_ALIGN_TOP_MID, 0, 16);
        const char *url = pairing_get_qr_url();
        lv_qrcode_update(qr, url, strlen(url));

        lv_obj_t *cap = lv_label_create(card);
        lv_obj_align(cap, LV_ALIGN_TOP_MID, 0, 250);
        lv_label_set_text(cap, "SCAN WITH APP");
        lv_obj_set_style_text_color(cap, lv_color_hex(0x2A1A12), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(cap, &ui_font_Arhivo_regular_18, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_letter_space(cap, 2, LV_PART_MAIN | LV_STATE_DEFAULT);

        lv_obj_t *did = lv_label_create(card);
        lv_obj_align(did, LV_ALIGN_TOP_MID, 0, 278);
        lv_label_set_text(did, pairing_get_device_id());
        lv_obj_set_style_text_color(did, lv_color_hex(0x8A3024), LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(did, &ui_font_Arhivo_regular_16, LV_PART_MAIN | LV_STATE_DEFAULT);
        lv_obj_set_style_text_letter_space(did, 3, LV_PART_MAIN | LV_STATE_DEFAULT);
    }

    Serial.println("[LegacyTape] ready");
}

void loop() {
    // Suspend LVGL refresh while the BLE module is doing WiFi.begin() —
    // RGB panel DMA otherwise contends with WiFi's PSRAM buffers and the
    // screen visibly glitches. Static frame stays on screen for ~5-10s.
    if (!pairing_ble_is_busy()) {
        lv_timer_handler();
    }
    buttons_poll();
    pairing_ble_loop();   // publishes WiFi scan results when ready
    cloud_sync_loop();    // polls Supabase for onboarding_complete (no-op until cloud_sync_begin)

    // pairing_mark_complete() is now fired by cloud_sync when Supabase reports
    // onboarding_complete=true (after the iOS app finishes the survey), NOT by
    // BLE 0x03 status. When the event fires, advance Screen1 -> Screen2 and
    // tear down the BLE stack to free ~30 KB RAM for the rest of the UI.
    if (pairing_consume_complete_event()) {
        Serial.println("[LegacyTape] onboarding complete -> advancing to Screen2");
        cloud_sync_stop();
        pairing_ble_stop();
        // ANIM_NONE = instant cut. The MOVE_LEFT animation was visibly laggy
        // because it runs 400ms of LVGL frame redraws right after BLE deinit
        // frees ~30 KB and WiFi is still doing background work in PSRAM.
        _ui_screen_change(&ui_Screen2, LV_SCR_LOAD_ANIM_NONE, 0, 0,
                          &ui_Screen2_screen_init);
    }

    delay(1);
}

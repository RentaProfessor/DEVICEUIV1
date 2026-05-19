# Legacy Tape — Arduino sketch for CrowPanel ESP32-S3 5" HMI

Self-contained Arduino sketch with all 14 screens of the Legacy Tape UI, ready to flash to the Elecrow CrowPanel Advance 5" HMI (ESP32-S3-WROOM-1-N16R8, 800×480 IPS, GT911 touch).

## Preliminary flash — what you can test without any external hardware wired up

Plug just the panel via USB-C. No MCP23017, no Uxcell buttons, no MAX98357, no mic, no SD card needed. You'll get:

- **Screen1 Pairing** loads on boot.
- **Tap anywhere on Screen1** → Screen2 Setup Complete.
- **Tap "NAME YOUR BOOK"** on Screen2 → Screen3 Name Book.
- **Type a name on the keyboard, then tap the keyboard's checkmark (✓)** → Screen4 Ready.
- On **Screen4**, three buttons in the chapter banner + top bar are reachable:
  - `CHAPTER ▾` → Screen10 Chapter Picker (tap any row → returns to Screen6 Stopped, which then has no exit without hardware buttons)
  - `BOOK ▾` → Screen8 Book Picker (tap any tile → returns to Screen4, or `NEW BOOK +` → Screen3)
  - `⚙ SETTINGS` (top bar) → Screen14 Settings (tap `× CLOSE` → Screen4)

That covers **Screens 1, 2, 3, 4, 8, 10, 14** through touch alone. Screens 5/6/7/9/11/12/13 need the physical Uxcell buttons wired through the MCP23017 to reach, *or* you can temporarily change the default screen in `ui.c` (the last line of `ui_init()`):

```c
lv_disp_load_scr(ui_Screen1);   // change to ui_Screen5, ui_Screen6, etc.
```

This is enough to confirm the display init, touch input, font rendering, navigation animations, and PSRAM allocation all work before you wire up the rest of the BOM.

## What's inside

```
legacytape_arduino/
├── legacytape_arduino.ino     ← main sketch: display init, touch, backlight, hardware buttons, LVGL pump
├── LovyanGFX_Driver.h         ← RGB parallel display + GT911 driver (from CrowPanel factory code)
├── pins_config.h              ← 800×480 resolution constants
├── ui.h / ui.c                ← UI lifecycle (init + destroy for all 14 screens)
├── ui_widgets.h / ui_widgets.c← Shared widget builders (top bar, chapter banner, hw legend, cassette, picker header)
├── ui_Screen1.c .. Screen14.c ← One file per screen (1-3 from your SquareLine export, 4-14 hand-written to spec)
├── ui_comp_*                  ← SquareLine component infrastructure (unchanged)
├── ui_helpers.* / ui_events.h ← SquareLine helpers (unchanged)
├── ui_font_*.c                ← Compiled LVGL fonts
└── ui_img_*.c                 ← Compiled images (QR card, welcome screenshot)
```

## Required Arduino libraries

Install in Arduino IDE → Sketch → Include Library → Manage Libraries:

| Library | Version | Source |
|---|---|---|
| **lvgl** | 8.3.3 (or 8.3.x) | `lvgl/lvgl` — exactly this version family, others may not be ABI-compatible |
| **LovyanGFX** | latest | `lovyan03/LovyanGFX` |

That's it. **No TCA9534, no MCP23017, no Adafruit libraries.** The sketch talks to every I2C peripheral (backlight controller, MCP23017 button expander, GT911 touch) directly over `Wire` with raw register reads/writes. If the chip isn't on your board (e.g., MCP23017 not wired yet), the calls NACK and the sketch silently continues — your UI still boots and touch still works.

ESP32 board package: `esp32` by Espressif Systems, **2.0.14 or 3.0.x** (anything newer that supports ESP32-S3-WROOM-1 with PSRAM).

## lv_conf.h — critical settings

LVGL needs an `lv_conf.h` in your `~/Documents/Arduino/libraries/lvgl/` folder. **Don't skip this.** Open `lv_conf_template.h`, copy it to `lv_conf.h`, then set:

```c
#define LV_CONF_INCLUDE_SIMPLE 1
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0
#define LV_MEM_CUSTOM 1                 // use heap_caps_malloc (PSRAM)
#define LV_USE_PERF_MONITOR 0
#define LV_DPI_DEF 130
#define LV_FONT_MONTSERRAT_14 1         // default fallback
#define LV_USE_LOG 0                    // set 1 only when debugging
#define LV_USE_DEMO_WIDGETS 0
```

If you want timer-based animations (lamp pulse, reels) you also need:
```c
#define LV_USE_ANIM 1
#define LV_USE_TIMER 1
```

## Arduino IDE board settings

```
Board:           ESP32S3 Dev Module
USB CDC On Boot: Enabled
CPU Frequency:   240MHz (WiFi)
Flash Mode:      QIO 80MHz
Flash Size:      16MB (128Mb)
Partition:       16M Flash (3MB APP/9.9MB FATFS)   ← or "Huge APP (3MB No OTA/1MB SPIFFS)"
PSRAM:           OPI PSRAM                          ← MUST be set, otherwise framebuffer alloc fails
Upload Speed:    921600
Core Debug:      None
```

## Flash steps

1. **Verify the libraries**: open `legacytape_arduino.ino` in Arduino IDE 2.x.
2. **Select the board**: Tools → Board → ESP32 Arduino → "ESP32S3 Dev Module" with the settings above.
3. **Hold BOOT** on the panel and tap **RESET** to enter download mode.
4. Tools → Port → pick the panel's `cu.usbmodem*` (Mac) or `COMx` (Windows).
5. **Verify** first (✓ button) — should compile clean.
6. **Upload** (→ button). Takes 30–60 s.
7. Panel will reboot and land on **Screen1 (Pairing)**.

## Verified hardware bindings (from CrowPanel docs + components.pdf)

| What | How / pin |
|---|---|
| Display | RGB parallel, ST7262 driver, 16-bit color, full-frame double-buffered in PSRAM |
| Touch  | GT911 **single-touch** (NOT multi-touch), I2C `0x5D` on Wire (SDA 15, SCL 16) @ 400 kHz. Power-up sequence: GPIO 1 OUTPUT LOW for 120 ms, then INPUT — selects 0x5D over the 0x14 fallback. |
| Backlight enable | TCA9534 IO expander (I2C `0x18`) pin 1 HIGH **(V1.0/V1.1)** |
| Backlight level  | I2C `0x30` (STC8H1K28 µC), byte `0` = max, `244` = min, `245` = off **(V1.2)** |
| Buzzer | I2C `0x30`, command `0x15` = on, `0x16` = off |
| **Transport buttons** | **Uxcell 5-position piano interlock — REC · PLAY · RWD · FF · STOP**. Wired to **Waveshare MCP23017** IO expander on I2C `0x20` (all A jumpers open), pins A0–A4. Internal pull-ups enabled. Active LOW. Polled every 20 ms from `loop()`. |
| Speaker amp | **MAX98357** external I2S Class-D amp. Inputs: DOUT=4, BCLK=5, LRC=6 from the panel's `SPK` PH2.0-2P port. Gain pin default 9 dB. |
| Mic | INMP441 I2S MEMS: BCLK=19, WS=2, DIN=20. 1-ch mono. |
| SD card | SPI MOSI=6, MISO=4, SCK=5. **⚠ Shares pins with I2S speaker out** — pick one at a time, buffer to PSRAM, swap modes between record/playback. |
| Rotary encoder (volume) | Not wired in this sketch yet — when added, route through 2 more MCP23017 pins (e.g., A5/A6) or the panel's unused I2C-OUT expansion. No free direct ESP32 GPIOs on N16R8. |

### Why ESP32 GPIOs aren't available for buttons

`ESP32-S3-WROOM-1-N16R8` reserves GPIO 26–37 for the **octal SPI flash + OPI PSRAM**. After the panel claims its RGB display pins (3, 7, 9–14, 17–18, 21, 38–48), I2C (15, 16), I2S mic (2, 19, 20), I2S amp / SD card (4, 5, 6), and strap pin 1 (touch reset), there are essentially **no free GPIOs left**. That's why an I2C-driven I/O expander is the *only* viable path for adding transport buttons.

### Panel hardware version detection

The sketch's `backlight_init()` attempts both V1.0/V1.1 (TCA9534 at 0x18) and V1.2 (STC8H1K28 at 0x30) controls. Whichever IC is on your panel ACKs; the other NACKs harmlessly. Check the version sticker on the back of your panel if backlight comes up dim or off.

## Navigation flow

Physical interlock buttons drive transport state. On-screen `CHAPTER ▾` and `BOOK ▾` drive picker overlays. There are no touch-simulated-button zones anymore — the 5 Uxcell keys on MCP23017 are the only way to move between transport states.

```
Screen1 Pairing ──(tap anywhere)──> Screen2 Setup ──(NAME btn)──> Screen3 Name Book ──(keyboard OK)──> Screen4 Ready
                                                                                                          │
                                                                                                          ▼
                                                                                       Screen4 Ready ◀────────────────┐
                                                                                          │                            │
                          ┌──────── physical Uxcell interlock keys ─────────┐              │                            │
                          │                                                 │              │                            │
                          ▼                                                 ▼              ▼                            │
                  REC  ──> Screen5 Recording                          PLAY ──> Screen7 Playback                          │
                                  │                                              │                                       │
                                  │ STOP                                         │ STOP                                  │
                                  ▼                                              ▼                                       │
                            Screen6 Stopped ◀──────────── (mid-session park) ────┘                                       │
                                  │                                                                                      │
                                  │ REC → Screen5 Recording (overdub)                                                    │
                                  │ PLAY → Screen7 Playback (from position)                                              │
                                  │ RWD/FF → seek (no screen change — position update only)                              │
                                  │                                                                                      │
                          ┌───── touch nav (CHAPTER ▾ / BOOK ▾ / ⚙ SETTINGS in top bar) ────┐                            │
                          ▼                                                                  ▼                            │
                  Screen10 Chapter Picker ──(row tap)──> Screen6 Stopped                Screen8 Book Picker               │
                                                                                          │                               │
                                                                                          │ tile tap                      │
                                                                                          │ NEW BOOK + ──> Screen3        │
                                                                                          ▼                               │
                                                                                       Screen4 Ready ───> Screen14 Settings┘

Screen9 AI Active     <── (firmware fires while AI is speaking; tap caption → Screen13 Advance)
Screen11 Volume        <── (rotary encoder triggers this; auto-dismiss to previous screen)
Screen12 Error         <── (WiFi/SD failure; RETRY → Screen4)
Screen13 Chapter Adv   <── (AI advances chapter momentarily; tap → back to Screen9)
```

### Button → state-machine map (in `legacytape_arduino.ino`)

| Current screen | REC | PLAY | RWD | FF | STOP |
|---|---|---|---|---|---|
| S4 Ready, S6 Stopped, S12 Error | → S5 Recording | → S7 Playback (S4/S6 only) | seek back | seek fwd | (no-op) |
| S5 Recording, S9 AI Active, S13 Advance | (interlock — pops) | (interlock — pops) | n/a | n/a | → S6 Stopped |
| S7 Playback | (overdub) | (no-op — already playing) | rewind speed up | ffwd speed up | → S6 Stopped |

## What's stubbed / TODO for real firmware

The sketch boots the UI, lights the backlight, and wires the touch + transport buttons. It does **not** yet:

- Actually record/play I2S audio (UI screen changes only)
- Mount the SD card
- Connect to WiFi / pair via BLE
- Track real chapter timing / position
- Animate reels or pulse the pilot lamp (the screens render static — animation hooks belong in firmware once the I2S level monitor is live)
- Drive the VU meters from real audio peaks (currently shows static "67%" reference levels)
- Generate or display the real pairing QR (`ui_img_qr_card_empty_2_png` is the placeholder you exported)

Add those one module at a time. Suggested order:
1. SD card mount + WAV writer
2. I2S mic capture → VU meter update
3. WiFi connect → AI streaming
4. RTC time-of-day on top bar
5. Reel rotation animation (`lv_anim_set_exec_cb` with `lv_img_set_angle`)

## Troubleshooting

| Symptom | Fix |
|---|---|
| Compile error `'LV_COLOR_DEPTH should be 16bit'` | Set `LV_COLOR_DEPTH 16` in your `lv_conf.h`. |
| Screen white, no UI | PSRAM not enabled. Tools → PSRAM → OPI PSRAM. |
| Touch doesn't respond | Wire bus probably not started before `gfx.init()`. The sketch does this, but custom changes may break it. Try `0x14` in `LovyanGFX_Driver.h` if `0x5D` NACKs (some panel revisions). |
| Touch coordinates off | `cfg.offset_rotation` in the driver — should be 0 for landscape with USB at the right. |
| Stalls / freezes after a few minutes | LVGL tick missing. The default ESP32 Arduino loop provides ms ticks via `millis()`, but if you switch to FreeRTOS make sure to call `lv_tick_inc(1)` at 1 ms intervals. |
| `undefined reference to ui_font_archivo_32` | Don't include that line in `LV_FONT_DECLARE` — it was an empty file in your export and is already removed. |
| Buttons don't do anything | MCP23017 isn't responding. (1) Verify the I2C-OUT port wiring to the panel. (2) Run an I2C scanner sketch — you should see `0x18` (TCA9534), `0x20` (MCP23017), `0x30` (STC8H1K28 backlight, V1.2 only), `0x51` or `0x68` (RTC), `0x5D` (GT911). (3) Check the A0/A1/A2 jumpers on the MCP23017 board — all open = 0x20. If you soldered any, recalculate the address (0x20 + (A0) + (A1<<1) + (A2<<2)) and update `#define MCP_ADDR` in the `.ino`. |
| Only some buttons register | Confirm the Uxcell switch's common pin is wired to MCP23017 GND, and each button output goes to A0/A1/A2/A3/A4 in the order **REC/PLAY/RWD/FF/STOP**. Reorder the wires (or remap `BTN_REC` … `BTN_STOP` constants in the `.ino`) until the labels line up. |
| Buttons all "stuck pressed" | The MCP23017's internal pull-ups aren't enabled. The sketch writes `GPPUA = 0x1F` at startup — make sure that line ran (check Serial output). |

## Where to add audio/SD/WiFi

Create new `.ino`-adjacent files in the same folder:
- `audio.cpp` / `audio.h` — I2S record + playback
- `storage.cpp` / `storage.h` — SD card mount + WAV chunking
- `connectivity.cpp` / `connectivity.h` — WiFi + BLE + AI streaming
- `rtc.cpp` / `rtc.h` — BM8563 time-of-day

Reference: `ABOUT DEVICE/5.0/factory_code/factory_code.ino` already contains working examples of every peripheral on this exact board — copy snippets from there into the modules above.

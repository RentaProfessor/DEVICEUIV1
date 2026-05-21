// Legacy Tape — audio recording from built-in PDM mic.
//
// CrowPanel V1.1 has an onboard PDM microphone wired to GPIO 19 (CLK) and
// GPIO 20 (DATA). It's muted at boot; sending I2C byte sequence 0x00 0x17
// to the STC8H1K28 µC at I2C 0x30 unmutes it.
//
// Default capture: 16 kHz mono 16-bit. At this rate one minute of audio is
// ~1.9 MB. With ~5 MB usable PSRAM after LVGL+BLE+WiFi, we cap a single
// recording at AUDIO_MAX_SECONDS (180s by default).
//
// Recording runs on a dedicated FreeRTOS task on Core 1, so it can keep up
// with I2S samples while the main loop continues to run LVGL.
#ifndef LEGACYTAPE_AUDIO_RECORD_H
#define LEGACYTAPE_AUDIO_RECORD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define AUDIO_SAMPLE_RATE   16000
#define AUDIO_MAX_SECONDS   180       // 3 minutes; one chapter take

typedef enum {
    AUDIO_STATE_IDLE,
    AUDIO_STATE_RECORDING,
    AUDIO_STATE_FINISHED,            // recording stopped, WAV is ready
    AUDIO_STATE_ERROR
} audio_state_t;

// Call once at boot. Allocates the PSRAM buffer + unmutes mic.
// Returns false if PSRAM alloc fails.
bool audio_record_begin(void);

// Start a fresh recording. Reuses the buffer (no realloc). Safe to call
// when state is IDLE or FINISHED (resets to RECORDING). Returns false if
// already recording or if mic init fails.
bool audio_record_start(void);

// Stop the current recording. Finalizes the WAV header in the buffer.
// State becomes FINISHED. Safe to call when not recording (no-op).
void audio_record_stop(void);

// Reset state to IDLE and discard the captured audio. Use after a
// successful upload to free the buffer for the next recording.
void audio_record_reset(void);

// State + stats accessors (safe to call at any time, including mid-record).
audio_state_t audio_record_state(void);
uint32_t      audio_record_seconds(void);         // current/final duration
uint8_t       audio_record_level_percent(void);   // 0..100 for VU meter
const uint8_t *audio_record_wav_data(void);       // valid after FINISHED
size_t        audio_record_wav_size(void);        // valid after FINISHED

#ifdef __cplusplus
}
#endif
#endif

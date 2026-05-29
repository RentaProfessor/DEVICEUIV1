// Legacy Tape — stream a recording from Supabase Storage to the I2S speaker.
//
// Flow:
//   1. POST /functions/v1/get_recording with hw_id + token + chapter idx
//      → server returns { found, url (signed Storage URL), duration_sec }
//   2. HTTP GET the signed URL, skip the 44-byte WAV header
//   3. Stream PCM bytes straight into the I2S output (MAX98357 on the SPK
//      port: BCLK=5, WS/LRC=6, DOUT=4). i2s.write() blocks on the DMA buffer,
//      which paces playback in real time — WiFi pulls far faster than 32 KB/s
//      so the speaker never starves.
//
// 16 kHz mono 16-bit — matches what the device recorded.
//
// v1 is sequential play/stop only. RWD/FF (HTTP Range seeks) come in v2.
// Runs on a FreeRTOS task on core 0 so LVGL stays smooth on core 1.
#ifndef LEGACYTAPE_AUDIO_PLAYBACK_H
#define LEGACYTAPE_AUDIO_PLAYBACK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PLAYBACK_IDLE,
    PLAYBACK_FETCHING,     // requesting URL / connecting
    PLAYBACK_PLAYING,
    PLAYBACK_DONE,         // reached end of file
    PLAYBACK_NONE,         // no recording exists for this chapter
    PLAYBACK_ERROR
} playback_state_t;

// Start streaming the recording for the given chapter index. Non-blocking.
void audio_playback_start(int chapter_idx);

// Stop playback + free the I2S output. Safe to call any time.
void audio_playback_stop(void);

playback_state_t audio_playback_state(void);
uint32_t         audio_playback_position_sec(void);   // seconds played so far
uint32_t         audio_playback_duration_sec(void);   // total, from server
const char      *audio_playback_last_error(void);

#ifdef __cplusplus
}
#endif
#endif

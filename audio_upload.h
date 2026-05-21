// Legacy Tape — upload a finished WAV to Supabase.
//
// The device POSTs the WAV bytes + metadata to a Supabase Edge Function
// named `upload_recording`. The Edge Function (cofounder deploys this):
//   1. Validates hardware_id + pairing_token
//   2. Writes the WAV to the 'recordings' storage bucket at a deterministic path
//   3. Inserts a row into the 'recordings' table linking storage_path -> chapter
//   4. Returns the recording_id + storage_url
//
// Transcription happens server-side on a separate trigger watching the
// recordings table (or chained from the upload function). Device fires
// and forgets — it doesn't wait for the transcription to complete.
//
// Upload runs on a FreeRTOS task on Core 1 so the main loop keeps drawing.
#ifndef LEGACYTAPE_AUDIO_UPLOAD_H
#define LEGACYTAPE_AUDIO_UPLOAD_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    UPLOAD_IDLE,
    UPLOAD_RUNNING,
    UPLOAD_SUCCESS,
    UPLOAD_FAILED
} upload_state_t;

// Begin an async upload of a WAV blob. Non-blocking — kicks off a task.
// chapter_idx is the 0-based active chapter index for the metadata.
// Returns false if already uploading or no WiFi.
bool audio_upload_begin(const uint8_t *wav, size_t wav_size,
                        uint32_t duration_sec,
                        int chapter_idx);

// State + progress for the UI.
upload_state_t audio_upload_state(void);
uint8_t        audio_upload_progress_percent(void);   // 0..100
const char    *audio_upload_last_error(void);         // human-readable

#ifdef __cplusplus
}
#endif
#endif

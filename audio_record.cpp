// PDM mic recording. Streams 16 kHz mono 16-bit samples into a PSRAM
// buffer in WAV format. Mic is unmuted via I2C 0x30 cmd 0x00 0x17.
#include "audio_record.h"
#include <Arduino.h>
#include <ESP_I2S.h>
#include <Wire.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define MIC_CLK_PIN    19
#define MIC_DATA_PIN   20

// 44-byte WAV header + audio samples
#define WAV_HEADER_LEN 44
static const size_t PCM_BYTES_PER_SEC = AUDIO_SAMPLE_RATE * 2; // 16-bit mono
static const size_t MAX_PCM_BYTES     = (size_t)AUDIO_MAX_SECONDS * PCM_BYTES_PER_SEC;
static const size_t WAV_BUF_SIZE      = WAV_HEADER_LEN + MAX_PCM_BYTES;

static I2SClass        g_i2s;
static uint8_t        *g_wav        = nullptr;     // PSRAM-backed
static volatile size_t g_pcm_written = 0;
static volatile audio_state_t g_state = AUDIO_STATE_IDLE;
static volatile uint8_t g_level = 0;               // 0..100
static TaskHandle_t    g_task   = nullptr;

static void write_wav_header(uint8_t *buf, uint32_t pcm_bytes) {
    uint32_t file_size = pcm_bytes + WAV_HEADER_LEN - 8;
    memcpy(buf,     "RIFF", 4);
    buf[4] = file_size & 0xFF;  buf[5] = (file_size >> 8) & 0xFF;
    buf[6] = (file_size >> 16) & 0xFF; buf[7] = (file_size >> 24) & 0xFF;
    memcpy(buf + 8, "WAVE", 4);
    memcpy(buf + 12, "fmt ", 4);
    buf[16] = 16; buf[17] = 0; buf[18] = 0; buf[19] = 0;   // fmt chunk size
    buf[20] = 1;  buf[21] = 0;                              // PCM
    buf[22] = 1;  buf[23] = 0;                              // mono
    uint32_t sr = AUDIO_SAMPLE_RATE;
    buf[24] = sr & 0xFF; buf[25] = (sr >> 8) & 0xFF;
    buf[26] = (sr >> 16) & 0xFF; buf[27] = (sr >> 24) & 0xFF;
    uint32_t br = sr * 2;                                   // byte rate
    buf[28] = br & 0xFF; buf[29] = (br >> 8) & 0xFF;
    buf[30] = (br >> 16) & 0xFF; buf[31] = (br >> 24) & 0xFF;
    buf[32] = 2; buf[33] = 0;                               // block align
    buf[34] = 16; buf[35] = 0;                              // bits per sample
    memcpy(buf + 36, "data", 4);
    buf[40] = pcm_bytes & 0xFF; buf[41] = (pcm_bytes >> 8) & 0xFF;
    buf[42] = (pcm_bytes >> 16) & 0xFF; buf[43] = (pcm_bytes >> 24) & 0xFF;
}

static bool unmute_mic(void) {
    // V1.1 mic is muted by default. Send 0x00 0x17 to 0x30 (STC8H1K28).
    Wire.beginTransmission(0x30);
    Wire.write((uint8_t)0x00);
    Wire.write((uint8_t)0x17);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        Serial.printf("[audio] mic unmute I2C error=%u (continuing — some hw revs auto-unmute)\n", err);
    } else {
        Serial.println("[audio] mic unmuted");
    }
    return true;   // never fatal — older boards may not need this
}

// Recording task pinned to Core 1 so it doesn't fight LVGL on Core 0
static void record_task(void *) {
    const size_t CHUNK = 1024;   // bytes per I2S read iteration
    uint8_t tmp[CHUNK];

    while (g_state == AUDIO_STATE_RECORDING) {
        size_t want = CHUNK;
        // Don't overrun the buffer
        if (g_pcm_written + want > MAX_PCM_BYTES) {
            want = MAX_PCM_BYTES - g_pcm_written;
            if (want == 0) {
                Serial.println("[audio] buffer full, auto-stopping recording");
                g_state = AUDIO_STATE_FINISHED;
                break;
            }
        }
        size_t got = g_i2s.readBytes((char *)tmp, want);
        if (got > 0) {
            memcpy(g_wav + WAV_HEADER_LEN + g_pcm_written, tmp, got);
            g_pcm_written += got;

            // Compute peak level for VU meter (sample is int16_t little-endian)
            int16_t peak = 0;
            for (size_t i = 0; i + 1 < got; i += 2) {
                int16_t s = (int16_t)(tmp[i] | (tmp[i + 1] << 8));
                int16_t a = s < 0 ? -s : s;
                if (a > peak) peak = a;
            }
            g_level = (uint8_t)((uint32_t)peak * 100 / 32768);
        }
        vTaskDelay(1);
    }

    // Recording stopped — finalize header
    write_wav_header(g_wav, (uint32_t)g_pcm_written);
    Serial.printf("[audio] recording finished: %u bytes (%us)\n",
                  (unsigned)(g_pcm_written + WAV_HEADER_LEN),
                  (unsigned)(g_pcm_written / PCM_BYTES_PER_SEC));
    g_task = nullptr;
    vTaskDelete(NULL);
}

bool audio_record_begin(void) {
    if (g_wav) return true;
    g_wav = (uint8_t *)heap_caps_malloc(WAV_BUF_SIZE, MALLOC_CAP_SPIRAM);
    if (!g_wav) {
        Serial.println("[audio] PSRAM alloc failed!");
        g_state = AUDIO_STATE_ERROR;
        return false;
    }
    Serial.printf("[audio] PSRAM buffer allocated: %u bytes (cap %us @ %u Hz)\n",
                  (unsigned)WAV_BUF_SIZE, AUDIO_MAX_SECONDS, AUDIO_SAMPLE_RATE);
    unmute_mic();
    return true;
}

bool audio_record_start(void) {
    if (!g_wav) {
        if (!audio_record_begin()) return false;
    }
    if (g_state == AUDIO_STATE_RECORDING) return false;

    g_pcm_written = 0;
    g_level = 0;

    g_i2s.setPinsPdmRx(MIC_CLK_PIN, MIC_DATA_PIN);
    if (!g_i2s.begin(I2S_MODE_PDM_RX, AUDIO_SAMPLE_RATE,
                     I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
        Serial.println("[audio] I2S PDM init failed");
        g_state = AUDIO_STATE_ERROR;
        return false;
    }
    Serial.println("[audio] recording started");
    g_state = AUDIO_STATE_RECORDING;

    // Spin up the capture task on Core 1
    xTaskCreatePinnedToCore(record_task, "audio_rec", 4096, NULL,
                            5, &g_task, 1);
    return true;
}

void audio_record_stop(void) {
    if (g_state != AUDIO_STATE_RECORDING) return;
    g_state = AUDIO_STATE_FINISHED;
    // Wait briefly for the task to finalize
    for (int i = 0; i < 50 && g_task != nullptr; i++) delay(10);
    g_i2s.end();
}

void audio_record_reset(void) {
    if (g_state == AUDIO_STATE_RECORDING) audio_record_stop();
    g_pcm_written = 0;
    g_level = 0;
    g_state = AUDIO_STATE_IDLE;
}

audio_state_t audio_record_state(void)        { return g_state; }
uint32_t      audio_record_seconds(void)      { return (uint32_t)(g_pcm_written / PCM_BYTES_PER_SEC); }
uint8_t       audio_record_level_percent(void){ return g_level; }
const uint8_t *audio_record_wav_data(void)    { return g_wav; }
size_t        audio_record_wav_size(void)     { return g_pcm_written + WAV_HEADER_LEN; }

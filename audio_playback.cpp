// Streaming playback: Supabase Storage WAV -> I2S speaker (MAX98357).
#include "audio_playback.h"
#include "pairing.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESP_I2S.h>
#include <ArduinoJson.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *SUPABASE_URL      = "https://uunxgkhhjjczeeawqflh.supabase.co";
static const char *SUPABASE_ANON_KEY = "sb_publishable_cBhuF6-XRJJxNv9hxHBUDw_M3lBpwqF";

// Speaker I2S pins (MAX98357 on the panel SPK port) — from factory pin map.
#define SPK_BCLK 5
#define SPK_LRC  6
#define SPK_DOUT 4

#define PLAY_SAMPLE_RATE 16000

static I2SClass        g_spk;
static volatile playback_state_t g_state = PLAYBACK_IDLE;
static volatile uint32_t g_pos_sec   = 0;
static volatile uint32_t g_dur_sec   = 0;
static volatile bool     g_stop_req  = false;
static char              g_err[96]   = {0};
static char              g_url[512]  = {0};
static int               g_chapter   = 0;
static TaskHandle_t      g_task      = nullptr;

static void set_err(const char *m) {
    strncpy(g_err, m, sizeof(g_err) - 1);
    g_err[sizeof(g_err) - 1] = 0;
    Serial.printf("[playback] error: %s\n", g_err);
}

// Ask the server for a signed URL for this device's recording of `chapter`.
// Returns true + fills g_url/g_dur_sec, or false (sets state NONE/ERROR).
static bool fetch_recording_url(int chapter) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(20);
    HTTPClient http;
    http.setTimeout(20000);
    String url = String(SUPABASE_URL) + "/functions/v1/get_recording";
    if (!http.begin(client, url)) { set_err("http.begin failed"); return false; }
    http.addHeader("apikey",        SUPABASE_ANON_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
    http.addHeader("Content-Type",  "application/json");
    http.addHeader("x-hardware-id", pairing_get_device_id());
    http.addHeader("x-pair-token",  pairing_get_token_hex());
    http.addHeader("x-chapter",     String(chapter));

    int code = http.POST("{}");
    if (code != 200) {
        char m[64]; snprintf(m, sizeof(m), "get_recording HTTP %d", code);
        set_err(m); http.end(); return false;
    }
    String resp = http.getString();
    http.end();

    StaticJsonDocument<768> doc;
    if (deserializeJson(doc, resp)) { set_err("bad JSON from get_recording"); return false; }
    bool found = doc["found"] | false;
    if (!found) { Serial.println("[playback] no recording for this chapter"); return false; }
    const char *u = doc["url"] | (const char *)nullptr;
    if (!u) { set_err("server returned no url"); return false; }
    strncpy(g_url, u, sizeof(g_url) - 1);
    g_url[sizeof(g_url) - 1] = 0;
    g_dur_sec = doc["duration_sec"] | 0;
    return true;
}

static void playback_task(void *) {
    g_state = PLAYBACK_FETCHING;
    g_pos_sec = 0;
    g_err[0] = 0;

    if (!fetch_recording_url(g_chapter)) {
        g_state = (g_err[0] == 0) ? PLAYBACK_NONE : PLAYBACK_ERROR;
        g_task = nullptr;
        vTaskDelete(NULL);
        return;
    }

    // Open the signed Storage URL (HTTPS, may redirect to the storage CDN)
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(20);
    HTTPClient http;
    http.setTimeout(20000);
    http.begin(client, g_url);
    const char *hdrs[] = {"Location"};
    http.collectHeaders(hdrs, 1);
    int code = http.GET();
    if (code == 302 || code == 301 || code == 307) {
        String loc = http.header("Location");
        http.end();
        http.begin(client, loc);
        code = http.GET();
    }
    if (code != 200) {
        char m[48]; snprintf(m, sizeof(m), "WAV GET HTTP %d", code);
        set_err(m); http.end();
        g_state = PLAYBACK_ERROR; g_task = nullptr; vTaskDelete(NULL); return;
    }

    // Init I2S output to the speaker. (Recording's I2S was ended when its
    // capture task stopped, so the I2S peripheral is free.)
    g_spk.setPins(SPK_BCLK, SPK_LRC, SPK_DOUT);
    if (!g_spk.begin(I2S_MODE_STD, PLAY_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
        set_err("I2S output begin failed");
        http.end(); g_state = PLAYBACK_ERROR; g_task = nullptr; vTaskDelete(NULL); return;
    }

    WiFiClient *stream = http.getStreamPtr();
    g_state = PLAYBACK_PLAYING;
    Serial.printf("[playback] streaming, duration=%us\n", (unsigned)g_dur_sec);

    // Skip the 44-byte WAV header
    uint8_t hdr[44];
    int hdr_read = 0;
    while (hdr_read < 44 && http.connected() && !g_stop_req) {
        if (stream->available()) hdr_read += stream->readBytes(hdr + hdr_read, 44 - hdr_read);
        else vTaskDelay(1);
    }

    // Stream PCM -> I2S. i2s.write() blocks on the DMA buffer => real-time pacing.
    uint8_t buf[2048];
    uint64_t bytes_played = 0;
    const uint64_t bytes_per_sec = PLAY_SAMPLE_RATE * 2;
    uint32_t idle_spins = 0;
    while (http.connected() && !g_stop_req) {
        size_t avail = stream->available();
        if (avail == 0) {
            if (++idle_spins > 2000) break;   // ~2s with no data => assume EOF
            vTaskDelay(1);
            continue;
        }
        idle_spins = 0;
        size_t n = stream->readBytes(buf, avail > sizeof(buf) ? sizeof(buf) : avail);
        if (n == 0) { vTaskDelay(1); continue; }
        g_spk.write(buf, n);
        bytes_played += n;
        g_pos_sec = (uint32_t)(bytes_played / bytes_per_sec);
    }

    g_spk.end();
    http.end();
    g_state = g_stop_req ? PLAYBACK_IDLE : PLAYBACK_DONE;
    Serial.printf("[playback] finished (%llu bytes, %us)\n",
                  (unsigned long long)bytes_played, (unsigned)g_pos_sec);
    g_task = nullptr;
    vTaskDelete(NULL);
}

void audio_playback_start(int chapter_idx) {
    if (g_state == PLAYBACK_PLAYING || g_state == PLAYBACK_FETCHING) return;
    if (WiFi.status() != WL_CONNECTED) { set_err("WiFi not connected"); g_state = PLAYBACK_ERROR; return; }
    g_chapter  = chapter_idx;
    g_stop_req = false;
    g_pos_sec  = 0;
    xTaskCreatePinnedToCore(playback_task, "audio_play", 8192, NULL, 4, &g_task, 0);
}

void audio_playback_stop(void) {
    g_stop_req = true;
    // Let the task notice + tear down I2S itself (avoids double-end races)
    for (int i = 0; i < 50 && g_task != nullptr; i++) delay(10);
}

playback_state_t audio_playback_state(void)       { return g_state; }
uint32_t         audio_playback_position_sec(void){ return g_pos_sec; }
uint32_t         audio_playback_duration_sec(void){ return g_dur_sec; }
const char      *audio_playback_last_error(void)  { return g_err; }

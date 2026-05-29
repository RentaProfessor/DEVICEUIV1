// Streaming playback: a whole chapter (oldest->newest takes) -> I2S speaker.
//
// get_recording now returns the full chapter timeline:
//   { found, count, total_duration_sec, recordings: [ {url, duration_sec}, ... ] }
// We play the clips back to back as one continuous timeline. I2S stays open
// across clips (all are 16k/mono/16-bit) so there's no gap/pop between takes.
#include "audio_playback.h"
#include "pairing.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ESP_I2S.h>
#include <ArduinoJson.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char *SUPABASE_URL      = "https://uunxgkhhjjczeeawqflh.supabase.co";
static const char *SUPABASE_ANON_KEY = "sb_publishable_cBhuF6-XRJJxNv9hxHBUDw_M3lBpwqF";

#define SPK_BCLK 5
#define SPK_LRC  6
#define SPK_DOUT 4
#define PLAY_SAMPLE_RATE 16000

#define MAX_CLIPS  64
#define URL_LEN    480
typedef struct { char url[URL_LEN]; uint32_t dur; } clip_t;

static I2SClass        g_spk;
static volatile playback_state_t g_state = PLAYBACK_IDLE;
static volatile uint32_t g_pos_sec  = 0;
static volatile uint32_t g_dur_sec  = 0;
static volatile bool     g_stop_req = false;
static char              g_err[96]  = {0};
static int               g_chapter  = 0;
static TaskHandle_t      g_task     = nullptr;

static clip_t *g_clips = nullptr;     // PSRAM
static int     g_clip_count = 0;

static void set_err(const char *m) {
    strncpy(g_err, m, sizeof(g_err) - 1);
    g_err[sizeof(g_err) - 1] = 0;
    Serial.printf("[playback] error: %s\n", g_err);
}

// Fetch the chapter's clip list. Fills g_clips/g_clip_count/g_dur_sec.
// Returns: 1 = have clips, 0 = empty chapter (found:false), -1 = error.
static int fetch_chapter(int chapter) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(20);
    HTTPClient http;
    http.setTimeout(20000);
    String url = String(SUPABASE_URL) + "/functions/v1/get_recording";
    if (!http.begin(client, url)) { set_err("http.begin failed"); return -1; }
    http.addHeader("apikey",        SUPABASE_ANON_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
    http.addHeader("Content-Type",  "application/json");
    http.addHeader("x-hardware-id", pairing_get_device_id());
    http.addHeader("x-pair-token",  pairing_get_token_hex());
    http.addHeader("x-chapter",     String(chapter));

    int code = http.POST("{}");
    if (code != 200) {
        char m[48]; snprintf(m, sizeof(m), "get_recording HTTP %d", code);
        set_err(m); http.end(); return -1;
    }
    String resp = http.getString();
    http.end();

    // Response can be large (many signed URLs). Parse with a filter so we only
    // keep the fields we need, keeping memory bounded.
    DynamicJsonDocument doc(32768);
    if (deserializeJson(doc, resp)) { set_err("bad JSON from get_recording"); return -1; }
    if (!(doc["found"] | false)) return 0;

    g_dur_sec = doc["total_duration_sec"] | 0;
    JsonArray arr = doc["recordings"].as<JsonArray>();
    g_clip_count = 0;
    for (JsonObject c : arr) {
        if (g_clip_count >= MAX_CLIPS) break;
        const char *u = c["url"] | (const char *)nullptr;
        if (!u) continue;
        strncpy(g_clips[g_clip_count].url, u, URL_LEN - 1);
        g_clips[g_clip_count].url[URL_LEN - 1] = 0;
        g_clips[g_clip_count].dur = c["duration_sec"] | 0;
        g_clip_count++;
    }
    if (g_clip_count == 0) return 0;
    if (g_dur_sec == 0) {  // fall back to summing if server omitted total
        for (int i = 0; i < g_clip_count; i++) g_dur_sec += g_clips[i].dur;
    }
    Serial.printf("[playback] chapter has %d clip(s), total %us\n", g_clip_count, (unsigned)g_dur_sec);
    return 1;
}

// Stream one clip's WAV to the (already-open) I2S output. base_sec is the
// global timeline offset of this clip's start, for the position readout.
// Returns false if the user requested stop mid-clip.
static bool stream_clip(const char *url, uint32_t base_sec) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(20);
    HTTPClient http;
    http.setTimeout(20000);
    http.begin(client, url);
    const char *hdrs[] = {"Location"};
    http.collectHeaders(hdrs, 1);
    int code = http.GET();
    if (code == 301 || code == 302 || code == 307) {
        String loc = http.header("Location");
        http.end();
        http.begin(client, loc);
        code = http.GET();
    }
    if (code != 200) {
        char m[40]; snprintf(m, sizeof(m), "clip GET HTTP %d", code);
        set_err(m); http.end(); return true;   // skip this clip, keep going
    }

    WiFiClient *stream = http.getStreamPtr();

    // Skip 44-byte WAV header
    uint8_t hdr[44]; int hr = 0;
    while (hr < 44 && http.connected() && !g_stop_req) {
        if (stream->available()) hr += stream->readBytes(hdr + hr, 44 - hr);
        else vTaskDelay(1);
    }

    uint8_t buf[2048];
    uint64_t played = 0;
    const uint64_t bps = PLAY_SAMPLE_RATE * 2;
    uint32_t idle = 0;
    while (http.connected() && !g_stop_req) {
        size_t avail = stream->available();
        if (avail == 0) { if (++idle > 2000) break; vTaskDelay(1); continue; }
        idle = 0;
        size_t n = stream->readBytes(buf, avail > sizeof(buf) ? sizeof(buf) : avail);
        if (n == 0) { vTaskDelay(1); continue; }
        g_spk.write(buf, n);
        played += n;
        g_pos_sec = base_sec + (uint32_t)(played / bps);
    }
    http.end();
    return !g_stop_req;
}

static void playback_task(void *) {
    g_state = PLAYBACK_FETCHING;
    g_pos_sec = 0;
    g_err[0] = 0;

    int r = fetch_chapter(g_chapter);
    if (r == 0) { g_state = PLAYBACK_NONE;  g_task = nullptr; vTaskDelete(NULL); return; }
    if (r < 0)  { g_state = PLAYBACK_ERROR; g_task = nullptr; vTaskDelete(NULL); return; }

    // Open I2S output once for the whole chapter (all clips share the format).
    g_spk.setPins(SPK_BCLK, SPK_LRC, SPK_DOUT);
    if (!g_spk.begin(I2S_MODE_STD, PLAY_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO)) {
        set_err("I2S output begin failed");
        g_state = PLAYBACK_ERROR; g_task = nullptr; vTaskDelete(NULL); return;
    }

    g_state = PLAYBACK_PLAYING;
    uint32_t base = 0;
    for (int i = 0; i < g_clip_count && !g_stop_req; i++) {
        Serial.printf("[playback] clip %d/%d (base %us)\n", i + 1, g_clip_count, (unsigned)base);
        stream_clip(g_clips[i].url, base);
        base += g_clips[i].dur;
    }

    g_spk.end();
    g_state = g_stop_req ? PLAYBACK_IDLE : PLAYBACK_DONE;
    Serial.printf("[playback] chapter finished at %us\n", (unsigned)g_pos_sec);
    g_task = nullptr;
    vTaskDelete(NULL);
}

void audio_playback_start(int chapter_idx) {
    if (g_state == PLAYBACK_PLAYING || g_state == PLAYBACK_FETCHING) return;
    if (WiFi.status() != WL_CONNECTED) { set_err("WiFi not connected"); g_state = PLAYBACK_ERROR; return; }
    if (!g_clips) {
        g_clips = (clip_t *)heap_caps_malloc(sizeof(clip_t) * MAX_CLIPS, MALLOC_CAP_SPIRAM);
        if (!g_clips) { set_err("PSRAM alloc failed"); g_state = PLAYBACK_ERROR; return; }
    }
    g_chapter  = chapter_idx;
    g_stop_req = false;
    g_pos_sec  = 0;
    g_dur_sec  = 0;
    xTaskCreatePinnedToCore(playback_task, "audio_play", 12288, NULL, 4, &g_task, 0);
}

void audio_playback_stop(void) {
    g_stop_req = true;
    for (int i = 0; i < 60 && g_task != nullptr; i++) delay(10);
}

playback_state_t audio_playback_state(void)        { return g_state; }
uint32_t         audio_playback_position_sec(void) { return g_pos_sec; }
uint32_t         audio_playback_duration_sec(void) { return g_dur_sec; }
const char      *audio_playback_last_error(void)   { return g_err; }

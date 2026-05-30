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
#include <Wire.h>
#include <Preferences.h>
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
// Supabase signed URLs are /storage/v1/object/sign/...?token=<JWT>; the JWT
// alone is ~200 chars, so the full URL runs ~300-380. 768 gives safe headroom
// — a truncated URL would silently fail the GET.
#define URL_LEN    768
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

// ─── Volume (software scale, persisted) ─────────────────────────────────────
// Default 45%: the MAX98357 adds ~9 dB hardware gain, so full-scale samples
// clip hard. Scaling to ~45% keeps peaks clean. User-adjustable on Screen7.
static int g_volume = -1;   // -1 = not loaded from NVS yet

static void volume_load(void) {
    if (g_volume >= 0) return;
    Preferences p;
    p.begin("ltvol", true);
    g_volume = p.getInt("v", 45);
    p.end();
    if (g_volume < 0) g_volume = 0;
    if (g_volume > 100) g_volume = 100;
}

int audio_playback_get_volume(void) { volume_load(); return g_volume; }

void audio_playback_set_volume(int vol) {
    if (vol < 0) vol = 0; if (vol > 100) vol = 100;
    g_volume = vol;
    Preferences p;
    p.begin("ltvol", false);
    p.putInt("v", g_volume);
    p.end();
}

void audio_playback_volume_step(int delta) {
    volume_load();
    audio_playback_set_volume(g_volume + delta);
}

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

// Stream one clip's WAV to the (already-open, STEREO) I2S output. base_sec is
// the global timeline offset of this clip's start, for the position readout.
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

    int content_len = http.getSize();   // -1 if chunked/unknown
    Serial.printf("[playback] clip GET 200, content-length=%d\n", content_len);

    WiFiClient *stream = http.getStreamPtr();
    stream->setTimeout(2000);            // blocking readBytes up to 2s

    // Read the whole WAV header (44 bytes) — readBytes blocks until it has them
    uint8_t hdr[44];
    int hr = 0;
    while (hr < 44 && !g_stop_req) {
        int r = stream->readBytes(hdr + hr, 44 - hr);
        if (r <= 0) { if (!stream->connected()) break; continue; }
        hr += r;
    }

    // Mono PCM in -> duplicate each 16-bit sample to L+R for stereo I2S out.
    // The MAX98357 needs audio on the channel(s) it reads; mono frames don't
    // reach it reliably. This matches the factory playback path.
    uint8_t  mono[1024];
    int16_t  stereo[1024];               // 512 mono samples -> 512 L/R pairs
    uint64_t played = 0;                 // PCM bytes consumed (mono)
    const uint64_t bps = PLAY_SAMPLE_RATE * 2;
    int remaining = (content_len > 44) ? (content_len - 44) : -1;

    while (!g_stop_req) {
        int want = sizeof(mono);
        if (remaining > 0 && remaining < want) want = remaining;
        int n = stream->readBytes(mono, want);
        if (n <= 0) {
            if (!stream->connected() && stream->available() == 0) break;  // EOF
            continue;
        }
        // Expand mono16 -> stereo16, applying the software volume scale.
        // Scaling down never clips (vol<=100); it tames the full-scale samples
        // that were clipping against the amp's hardware gain.
        int samples = n / 2;
        int vol = g_volume;   // snapshot (may change mid-playback via buttons)
        for (int i = 0; i < samples; i++) {
            int16_t raw = (int16_t)(mono[i * 2] | (mono[i * 2 + 1] << 8));
            int16_t s = (int16_t)(((int32_t)raw * vol) / 100);
            stereo[i * 2]     = s;
            stereo[i * 2 + 1] = s;
        }
        g_spk.write((uint8_t *)stereo, samples * 4);   // 2 ch * 2 bytes
        played += n;
        if (remaining > 0) { remaining -= n; if (remaining <= 0) break; }
        g_pos_sec = base_sec + (uint32_t)(played / bps);
    }
    Serial.printf("[playback] clip done: %llu PCM bytes\n", (unsigned long long)played);
    http.end();
    return !g_stop_req;
}

static void playback_task(void *) {
    g_state = PLAYBACK_FETCHING;
    g_pos_sec = 0;
    g_err[0] = 0;
    volume_load();   // make sure g_volume is ready before we scale samples

    int r = fetch_chapter(g_chapter);
    if (r == 0) { g_state = PLAYBACK_NONE;  g_task = nullptr; vTaskDelete(NULL); return; }
    if (r < 0)  { g_state = PLAYBACK_ERROR; g_task = nullptr; vTaskDelete(NULL); return; }

    // Open I2S output once for the whole chapter (all clips share the format).
    // STEREO out: we duplicate the mono recording into both channels so the
    // MAX98357 amp gets audio regardless of which channel it reads. (Matches
    // the factory playback path — mono frames didn't reach the amp.)
    // Unmute the panel audio path (STC8H1K28 µC @ 0x30) in case the SPK port
    // routes through the onboard amp, which boots muted. Restore backlight
    // brightness right after (the 0x30 µC also controls it).
    Wire.beginTransmission(0x30); Wire.write((uint8_t)0x00); Wire.write((uint8_t)0x17); Wire.endTransmission();
    delay(10);
    Wire.beginTransmission(0x30); Wire.write((uint8_t)0x10); Wire.endTransmission();

    g_spk.setPins(SPK_BCLK, SPK_LRC, SPK_DOUT);
    if (!g_spk.begin(I2S_MODE_STD, PLAY_SAMPLE_RATE, I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO)) {
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

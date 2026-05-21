// Async WAV upload to Supabase Edge Function `upload_recording`.
#include "audio_upload.h"
#include "pairing.h"
#include "book.h"
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Same Supabase project + anon key used by cloud_sync.cpp
static const char *SUPABASE_URL      = "https://uunxgkhhjjczeeawqflh.supabase.co";
static const char *SUPABASE_ANON_KEY = "sb_publishable_cBhuF6-XRJJxNv9hxHBUDw_M3lBpwqF";

static volatile upload_state_t g_state = UPLOAD_IDLE;
static volatile uint8_t        g_progress = 0;
static char                    g_err[96] = {0};

// Snapshot of upload args — captured into statics so the task can read them
static const uint8_t *g_wav         = nullptr;
static size_t         g_wav_size    = 0;
static uint32_t       g_duration    = 0;
static int            g_chapter_idx = 0;
static TaskHandle_t   g_task        = nullptr;

static void set_err(const char *msg) {
    strncpy(g_err, msg, sizeof(g_err) - 1);
    g_err[sizeof(g_err) - 1] = 0;
    Serial.printf("[upload] error: %s\n", g_err);
}

static void upload_task(void *) {
    g_state = UPLOAD_RUNNING;
    g_progress = 0;
    g_err[0] = 0;
    Serial.printf("[upload] starting: %u bytes, %us, chapter %d\n",
                  (unsigned)g_wav_size, (unsigned)g_duration, g_chapter_idx);

    WiFiClientSecure client;
    client.setInsecure();   // TODO: pin Supabase root CA in production
    client.setTimeout(60);  // seconds — large uploads can be slow

    HTTPClient http;
    http.setTimeout(60000);

    // POST to the Edge Function. Body is the raw WAV; metadata in headers.
    String url = String(SUPABASE_URL) + "/functions/v1/upload_recording";
    if (!http.begin(client, url)) {
        set_err("http.begin failed");
        g_state = UPLOAD_FAILED;
        g_task = nullptr;
        vTaskDelete(NULL);
        return;
    }
    http.addHeader("apikey",        SUPABASE_ANON_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
    http.addHeader("Content-Type",  "audio/wav");
    http.addHeader("x-hardware-id", pairing_get_device_id());
    http.addHeader("x-pair-token",  pairing_get_token_hex());
    http.addHeader("x-acct",        pairing_get_account());
    http.addHeader("x-duration",    String(g_duration));
    http.addHeader("x-chapter",     String(g_chapter_idx));
    http.addHeader("x-book-name",   book_get_name());
    http.addHeader("x-chapter-name", book_get_chapter_name(g_chapter_idx) ?: "Chapter");

    int code = http.POST(const_cast<uint8_t *>(g_wav), g_wav_size);
    if (code < 0) {
        set_err(http.errorToString(code).c_str());
        g_state = UPLOAD_FAILED;
    } else if (code >= 200 && code < 300) {
        String resp = http.getString();
        Serial.printf("[upload] success HTTP %d: %s\n", code, resp.c_str());
        g_progress = 100;
        g_state    = UPLOAD_SUCCESS;
    } else {
        String resp = http.getString();
        char msg[96];
        snprintf(msg, sizeof(msg), "HTTP %d: %.60s", code, resp.c_str());
        set_err(msg);
        g_state = UPLOAD_FAILED;
    }

    http.end();
    g_task = nullptr;
    vTaskDelete(NULL);
}

bool audio_upload_begin(const uint8_t *wav, size_t wav_size,
                        uint32_t duration_sec, int chapter_idx) {
    if (g_state == UPLOAD_RUNNING) {
        Serial.println("[upload] already running");
        return false;
    }
    if (WiFi.status() != WL_CONNECTED) {
        set_err("WiFi not connected");
        g_state = UPLOAD_FAILED;
        return false;
    }
    if (!wav || wav_size == 0) {
        set_err("empty wav");
        g_state = UPLOAD_FAILED;
        return false;
    }

    g_wav         = wav;
    g_wav_size    = wav_size;
    g_duration    = duration_sec;
    g_chapter_idx = chapter_idx;

    xTaskCreatePinnedToCore(upload_task, "audio_up", 8192, NULL, 4, &g_task, 1);
    return true;
}

upload_state_t audio_upload_state(void)            { return g_state; }
uint8_t        audio_upload_progress_percent(void) { return g_progress; }
const char    *audio_upload_last_error(void)       { return g_err; }

// Supabase polling for onboarding_complete=true.
//
// Polls every 5 seconds. Uses anon key + a stored procedure that takes
// (hardware_id, pairing_token) and returns the row's onboarding_complete
// flag. The RPC must be deployed by the cofounder before this works:
//
//   create or replace function public.device_status(hw_id text, tok text)
//   returns table (onboarding_complete boolean)
//   language sql security definer stable as $$
//     select onboarding_complete from devices
//     where hardware_id = hw_id and pairing_token = tok
//     limit 1;
//   $$;
//
// If the RPC isn't deployed yet, this module logs failures and keeps polling
// — when the cofounder deploys it, the next poll succeeds and the screen
// transitions. No firmware reflash needed.

#include "cloud_sync.h"
#include "pairing.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// Supabase project + anon key — matches the iOS app's src/lib/supabase.ts
static const char *SUPABASE_URL      = "https://uunxgkhhjjczeeawqflh.supabase.co";
static const char *SUPABASE_ANON_KEY = "sb_publishable_cBhuF6-XRJJxNv9hxHBUDw_M3lBpwqF";

static const unsigned long POLL_INTERVAL_MS = 5000;
static bool          g_active     = false;
static unsigned long g_last_poll  = 0;
static unsigned int  g_poll_count = 0;

void cloud_sync_begin(void) {
    g_active = true;
    g_last_poll = 0;   // poll immediately on first loop tick
    g_poll_count = 0;
    Serial.println("[cloud] polling started — waiting for onboarding_complete=true");
}

void cloud_sync_stop(void) {
    if (g_active) Serial.println("[cloud] polling stopped");
    g_active = false;
}

static void do_poll(void) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[cloud] WiFi not connected, skipping poll");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();   // TODO: pin Supabase root cert for production

    HTTPClient http;
    String url = String(SUPABASE_URL) + "/rest/v1/rpc/device_status";
    if (!http.begin(client, url)) {
        Serial.println("[cloud] http.begin failed");
        return;
    }
    http.addHeader("apikey", SUPABASE_ANON_KEY);
    http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
    http.addHeader("Content-Type", "application/json");

    StaticJsonDocument<192> req;
    req["hw_id"] = pairing_get_device_id();
    req["tok"]   = pairing_get_token_hex();
    String body;
    serializeJson(req, body);

    g_poll_count++;
    int code = http.POST(body);

    if (code == 200) {
        String resp = http.getString();
        StaticJsonDocument<256> respDoc;
        DeserializationError err = deserializeJson(respDoc, resp);
        if (err) {
            Serial.printf("[cloud] poll #%u: JSON parse error: %s\n", g_poll_count, err.c_str());
        } else {
            bool complete = false;
            // RPC returns an array of rows
            if (respDoc.is<JsonArray>()) {
                JsonArray arr = respDoc.as<JsonArray>();
                if (arr.size() > 0) {
                    complete = arr[0]["onboarding_complete"] | false;
                }
            } else {
                complete = respDoc["onboarding_complete"] | false;
            }

            if (complete) {
                Serial.println("[cloud] onboarding_complete=true — firing screen advance");
                pairing_mark_complete();
                g_active = false;
            } else if (g_poll_count % 6 == 1) {
                // Every ~30s, log we're still waiting
                Serial.printf("[cloud] poll #%u: onboarding not yet complete\n", g_poll_count);
            }
        }
    } else if (code == 404) {
        Serial.printf("[cloud] poll #%u: 404 — device_status RPC not deployed yet (cofounder TODO)\n",
                      g_poll_count);
    } else {
        Serial.printf("[cloud] poll #%u: HTTP %d\n", g_poll_count, code);
        if (code < 0) {
            Serial.printf("[cloud] error: %s\n", http.errorToString(code).c_str());
        }
    }

    http.end();
}

void cloud_sync_loop(void) {
    if (!g_active) return;
    unsigned long now = millis();
    if (now - g_last_poll < POLL_INTERVAL_MS) return;
    g_last_poll = now;
    do_poll();
}

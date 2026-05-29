// BLE pairing receiver — implements the iOS app contract per PAIRING_SPEC.md
//
// Connection flow:
//   1. App scans for service 1ec0de7a-...0001
//   2. App connects, discovers 3 characteristics:
//        ...0002 (write)  — credentials payload
//        ...0003 (notify) — status bytes
//        ...0004 (read/notify) — JSON array of visible WiFi networks
//   3. App reads ...0004 to populate its WiFi picker
//   4. App subscribes to notifications on ...0003
//   5. App writes JSON to ...0002:
//        {"token":"...","ssid":"...","pw":"...","acct":"...","ssid2":"...","pw2":"..."}
//      ssid2/pw2 are optional ("setup for someone else" flow).
//   6. Device validates token  → 0x01
//   7. Device stores creds in NVS → 0x02
//   8. Device tries WiFi.begin(primary), falls back to secondary
//   9. On WiFi success: 0x03  (NOT yet "paired" from device's POV — the iOS app
//      still has to walk the user through onboarding + write onboarding_complete=true
//      to Supabase. The cloud_sync module polls for that and triggers screen
//      advance. See cloud_sync.cpp.)
//
// JSON parsing uses ArduinoJson (must be installed via Arduino Library Manager).

#include "pairing_ble.h"
#include "pairing.h"
#include "cloud_sync.h"            // declares cloud_sync_begin() with C linkage
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <WiFi.h>

static BLEServer         *g_server        = nullptr;
static BLECharacteristic *g_pair_char     = nullptr;
static BLECharacteristic *g_status_char   = nullptr;
static BLECharacteristic *g_wifilist_char = nullptr;
static volatile bool      g_client_connected = false;
static bool               g_wifi_scan_published = false;
static bool               g_scan_active   = false;   // an async scan is in flight
static uint32_t           g_scan_start_ms = 0;
#define SCAN_TIMEOUT_MS   5000                        // publish empty if no result by then
// True while wifi_try() is mid-connect. Main loop checks this and skips
// lv_timer_handler() so the RGB panel DMA doesn't fight WiFi's PSRAM use.
static volatile bool      g_busy = false;
static uint32_t           g_busy_since = 0;     // millis() when g_busy went true
#define BUSY_MAX_MS       90000                 // hard ceiling: never freeze >90s

// ─── Status notifications ───────────────────────────────────────────────────
static void notify_status(uint8_t code) {
    if (!g_status_char) return;
    uint8_t b = code;
    g_status_char->setValue(&b, 1);
    g_status_char->notify();
    Serial.printf("[ble] status notify: 0x%02X\n", code);
    delay(20);
}

// ─── WiFi list characteristic ───────────────────────────────────────────────
// Kick off an async 2.4GHz scan. The ESP32-S3 radio is 2.4GHz-only, so the
// results are inherently 2.4GHz networks the device can actually join — no
// 5GHz filtering needed (it physically can't see them).
static void start_wifi_scan() {
    Serial.println("[ble] starting WiFi scan");
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, false);      // ensure not associated, so scan is clean
    WiFi.scanDelete();                  // drop any stale prior results
    WiFi.scanNetworks(true /*async*/, false /*hidden*/);
    g_scan_active = true;
    g_wifi_scan_published = false;
    g_scan_start_ms = millis();
}

// Build + publish the network list to char 0x0004. Called when the async scan
// completes (n>=0) OR on timeout/failure (publishes an EMPTY array so the app
// can distinguish "no networks" from "scan broken" — it must never go silent).
// A BLE characteristic value maxes at 512 bytes (ATT), and a single notify is
// capped at ATT_MTU-3. We keep the published JSON under ~480 bytes so a full
// readValue (iOS reassembles reads to 512B) returns the COMPLETE list. We add
// the STRONGEST networks first (sorted by RSSI) and stop before the size cap,
// so the cap only ever drops weak networks the user wouldn't pick.
#define WIFI_JSON_MAX 480

static void publish_wifi_list(int n) {
    StaticJsonDocument<1024> doc;
    JsonArray arr = doc.to<JsonArray>();
    int kept = 0;

    if (n > 0) {
        // Sort scan indices by RSSI descending (simple selection — n is small)
        int order[40];
        int cnt = (n > 40) ? 40 : n;
        for (int i = 0; i < cnt; i++) order[i] = i;
        for (int i = 0; i < cnt - 1; i++) {
            int best = i;
            for (int j = i + 1; j < cnt; j++)
                if (WiFi.RSSI(order[j]) > WiFi.RSSI(order[best])) best = j;
            int tmp = order[i]; order[i] = order[best]; order[best] = tmp;
        }
        for (int k = 0; k < cnt; k++) {
            String ssid = WiFi.SSID(order[k]);
            if (ssid.length() == 0) continue;          // skip hidden/blank
            JsonObject net = arr.createNestedObject();
            net["ssid"] = ssid;
            net["rssi"] = WiFi.RSSI(order[k]);
            // Stop before we exceed the readable-attribute size budget
            if (measureJson(doc) > WIFI_JSON_MAX) {
                arr.remove(arr.size() - 1);            // drop the one that overflowed
                break;
            }
            kept++;
        }
    }

    char buf[512];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    if (g_wifilist_char) {
        g_wifilist_char->setValue((uint8_t *)buf, len);
        g_wifilist_char->notify();
    }
    Serial.printf("[ble] published %d of %d network(s) to 0x0004 (%u bytes, fits 512 read)\n",
                  kept, n < 0 ? 0 : n, (unsigned)len);

    g_wifi_scan_published = true;
    g_scan_active = false;
    WiFi.scanDelete();
}

// ─── WiFi credentials handler ───────────────────────────────────────────────
// Attempt WiFi connection. Returns true if connected within timeout.
//
// IMPORTANT: We deliberately do NOT call lv_timer_handler() during the wait.
// The ESP32-S3 RGB panel framebuffer lives in PSRAM and is read continuously
// by the LCD DMA. WiFi.begin() / packet activity also hits PSRAM heavily for
// its own buffers. Triggering an LVGL frame redraw while WiFi is active
// causes visible PSRAM bandwidth contention -> horizontal jitter / glitching.
//
// So during this 5-15 second WiFi connect window, we let LVGL freeze on its
// last frame. BLE notifications still flow (they're handled on a separate
// FreeRTOS task, not the main loop). Touch input is ignored during the wait,
// which is fine — the user just submitted credentials and is waiting.
static bool wifi_try(const char *ssid, const char *pw, uint32_t timeout_ms) {
    if (!ssid || strlen(ssid) == 0) return false;
    Serial.printf("[ble] connecting to WiFi SSID='%s'\n", ssid);
    // g_busy is already true (set in onConnect). LVGL stays suspended.

    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, true);
    delay(100);
    WiFi.begin(ssid, pw);

    uint32_t start = millis();
    uint32_t last_log = start;
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
        delay(100);
        if (millis() - last_log > 1000) {
            Serial.print('.');
            last_log = millis();
        }
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[ble] WiFi connected, IP=%s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.printf("[ble] WiFi '%s' connect failed, status=%d\n", ssid, WiFi.status());
    return false;
}

bool pairing_ble_is_busy(void) {
    // Watchdog: g_busy should clear via onDisconnect or pairing_ble_stop. If
    // any code path ever leaves it stuck (forced BLE deinit, dropped callback,
    // etc.), self-heal after BUSY_MAX_MS so the UI can never be permanently
    // frozen. The legitimate busy window (WiFi connect) is ~10s; the whole
    // pairing session is well under 90s.
    if (g_busy && g_busy_since != 0 && (millis() - g_busy_since) > BUSY_MAX_MS) {
        Serial.println("[ble] g_busy watchdog tripped — force-clearing");
        g_busy = false;
    }
    return g_busy;
}

// ─── BLE callbacks ──────────────────────────────────────────────────────────
class PairCharCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *chr) override {
        // ESP32 Arduino core 3.x returns Arduino String here (was std::string in 2.x)
        String raw = chr->getValue();
        Serial.printf("[ble] pair char write: %u bytes\n", (unsigned)raw.length());
        if (raw.length() == 0 || raw.length() > 480) {
            notify_status(LT_STATUS_ERR_JSON);
            return;
        }

        notify_status(LT_STATUS_VALIDATING);

        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, raw.c_str());
        if (err) {
            Serial.printf("[ble] JSON parse failed: %s\n", err.c_str());
            notify_status(LT_STATUS_ERR_JSON);
            return;
        }

        const char *token = doc["token"] | (const char *)nullptr;
        const char *ssid  = doc["ssid"]  | (const char *)nullptr;
        const char *pw    = doc["pw"]    | "";
        const char *acct  = doc["acct"]  | (const char *)nullptr;
        const char *ssid2 = doc["ssid2"] | "";   // optional
        const char *pw2   = doc["pw2"]   | "";   // optional

        if (!token || !ssid || !acct) {
            Serial.println("[ble] payload missing required field (token/ssid/acct)");
            notify_status(LT_STATUS_ERR_JSON);
            return;
        }

        const char *expected = pairing_get_token_hex();
        if (strcmp(token, expected) != 0) {
            Serial.printf("[ble] token mismatch (got '%s', expected '%s')\n", token, expected);
            notify_status(LT_STATUS_ERR_TOKEN);
            return;
        }

        // Persist all credentials BEFORE WiFi attempt so they survive a crash
        if (!pairing_store_credentials(ssid, pw, ssid2, pw2, acct)) {
            notify_status(LT_STATUS_ERR_INTERNAL);
            return;
        }

        notify_status(LT_STATUS_WIFI_CONNECTING);

        // Try primary, then secondary if provided
        bool connected = wifi_try(ssid, pw, 20000);
        if (!connected && strlen(ssid2) > 0) {
            Serial.printf("[ble] primary failed, trying secondary SSID='%s'\n", ssid2);
            connected = wifi_try(ssid2, pw2, 20000);
        }

        if (!connected) {
            notify_status(LT_STATUS_ERR_WIFI);
            return;
        }

        // WiFi up. 0x03 means "WiFi connected" per the iOS contract. The actual
        // device-side "paired" state (screen advance) waits for Supabase to flip
        // onboarding_complete = true after the user finishes the survey.
        notify_status(LT_STATUS_PAIRED);

        // Start polling Supabase for onboarding_complete
        cloud_sync_begin();
    }
};

class WifiListCharCallbacks : public BLECharacteristicCallbacks {
    void onRead(BLECharacteristic *chr) override {
        Serial.println("[ble] wifi list char read");
        // The app reads this after connecting to populate its picker. If no
        // scan is running and we haven't published a result yet, kick a fresh
        // scan (covers the app's "Rescan" which re-reads this characteristic).
        // The current value is returned now; the fresh list is notify'd when
        // the scan completes.
        if (!g_scan_active && !g_wifi_scan_published) {
            start_wifi_scan();
        }
    }
};

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *srv) override {
        g_client_connected = true;
        // Suspend LVGL for the ENTIRE BLE session, not just WiFi connect.
        // Reason: the credential write, the BLE notify acks, the JSON parse,
        // and any incidental BLE traffic all spike PSRAM use. The RGB panel
        // DMA + LVGL frame redraws fight that for bandwidth → visible glitches.
        // Phone-side user is staring at their phone the whole time, so the
        // frozen device screen isn't seen.
        g_busy = true;
        g_busy_since = millis();
        Serial.println("[ble] client connected (LVGL suspended)");

        // The iOS app now DEPENDS on the BLE scan list (no manual SSID entry).
        // Kick off a fresh 2.4GHz scan on every connect so the picker is
        // populated, and a reconnect ('Rescan' in the app) re-scans for newly
        // powered-on routers. Result is notify'd to 0x0004 when it completes.
        start_wifi_scan();
    }
    void onDisconnect(BLEServer *srv) override {
        g_client_connected = false;
        g_busy = false;
        Serial.println("[ble] client disconnected (LVGL resumed)");
        if (!pairing_is_complete()) {
            BLEDevice::startAdvertising();
        }
    }
};

// ─── Init / teardown ────────────────────────────────────────────────────────
void pairing_ble_begin(void) {
    if (pairing_is_complete()) {
        Serial.println("[ble] already paired, skipping BLE init");
        return;
    }

    char ble_name[24];
    snprintf(ble_name, sizeof(ble_name), "LegacyTape-%s", pairing_get_device_id());

    BLEDevice::init(ble_name);
    BLEDevice::setMTU(247);

    g_server = BLEDevice::createServer();
    g_server->setCallbacks(new ServerCallbacks());

    BLEService *svc = g_server->createService(LT_BLE_SERVICE_UUID);

    g_pair_char = svc->createCharacteristic(
        LT_BLE_PAIR_CHAR_UUID,
        BLECharacteristic::PROPERTY_WRITE);
    g_pair_char->setCallbacks(new PairCharCallbacks());

    g_status_char = svc->createCharacteristic(
        LT_BLE_STATUS_UUID,
        BLECharacteristic::PROPERTY_NOTIFY | BLECharacteristic::PROPERTY_READ);
    g_status_char->addDescriptor(new BLE2902());
    uint8_t idle = LT_STATUS_IDLE;
    g_status_char->setValue(&idle, 1);

    g_wifilist_char = svc->createCharacteristic(
        LT_BLE_WIFILIST_UUID,
        BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
    g_wifilist_char->addDescriptor(new BLE2902());
    g_wifilist_char->setValue("[]");   // empty until scan completes
    g_wifilist_char->setCallbacks(new WifiListCharCallbacks());

    svc->start();

    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(LT_BLE_SERVICE_UUID);
    adv->setScanResponse(true);
    adv->setMinPreferred(0x06);
    adv->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.printf("[ble] advertising as '%s' with service %s\n", ble_name, LT_BLE_SERVICE_UUID);
}

void pairing_ble_stop(void) {
    Serial.println("[ble] stopping BLE stack");
    // Force-clear the LVGL-suspend flag BEFORE deinit. onDisconnect would
    // normally clear it, but BLEDevice::deinit(true) tears down the stack
    // without firing the disconnect callback, leaving g_busy stuck true
    // and LVGL frozen — every touch ignored — until next reboot.
    g_busy = false;
    g_client_connected = false;
    if (g_server) {
        BLEDevice::stopAdvertising();
        BLEDevice::deinit(true);
        g_server = nullptr;
        g_pair_char = nullptr;
        g_status_char = nullptr;
        g_wifilist_char = nullptr;
    }
}

// Called from main loop — publishes WiFi scan results when the async scan
// finishes, or an empty list on timeout so the app never hangs on a spinner.
void pairing_ble_loop(void) {
    if (!g_server) return;
    if (!g_scan_active) return;

    int n = WiFi.scanComplete();   // >=0 done, -1 running, -2 not started/failed
    if (n >= 0) {
        publish_wifi_list(n);                       // includes empty (n==0)
    } else if (millis() - g_scan_start_ms > SCAN_TIMEOUT_MS) {
        Serial.println("[ble] scan timed out — publishing empty list");
        publish_wifi_list(0);                       // empty array, not silence
    }
}

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
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ArduinoJson.h>
#include <WiFi.h>

extern void cloud_sync_begin();   // implemented in cloud_sync.cpp

static BLEServer         *g_server        = nullptr;
static BLECharacteristic *g_pair_char     = nullptr;
static BLECharacteristic *g_status_char   = nullptr;
static BLECharacteristic *g_wifilist_char = nullptr;
static volatile bool      g_client_connected = false;
static bool               g_wifi_scan_published = false;

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
// Builds the JSON array of nearby networks and publishes it to characteristic 0x0004.
static void publish_wifi_list() {
    int n = WiFi.scanComplete();
    if (n < 0) return;   // scan not done yet or no scan was started

    StaticJsonDocument<2048> doc;
    JsonArray arr = doc.to<JsonArray>();
    int max_n = (n > 20) ? 20 : n;   // cap at 20 networks to keep payload reasonable
    for (int i = 0; i < max_n; i++) {
        JsonObject net = arr.createNestedObject();
        net["ssid"] = WiFi.SSID(i);
        net["rssi"] = WiFi.RSSI(i);
    }
    char buf[2048];
    size_t len = serializeJson(doc, buf, sizeof(buf));

    if (g_wifilist_char) {
        g_wifilist_char->setValue((uint8_t *)buf, len);
        g_wifilist_char->notify();
    }
    Serial.printf("[ble] published %d WiFi networks to char 0x0004 (%u bytes)\n", n, (unsigned)len);
    g_wifi_scan_published = true;
}

// ─── WiFi credentials handler ───────────────────────────────────────────────
// Attempt WiFi connection. Returns true if connected within timeout.
static bool wifi_try(const char *ssid, const char *pw, uint32_t timeout_ms) {
    if (!ssid || strlen(ssid) == 0) return false;
    Serial.printf("[ble] connecting to WiFi SSID='%s'\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, true);   // clear any cached state
    delay(100);
    WiFi.begin(ssid, pw);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout_ms) {
        delay(200);
        Serial.print('.');
    }
    Serial.println();

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[ble] WiFi connected, IP=%s\n", WiFi.localIP().toString().c_str());
        return true;
    }
    Serial.printf("[ble] WiFi '%s' connect failed, status=%d\n", ssid, WiFi.status());
    return false;
}

// ─── BLE callbacks ──────────────────────────────────────────────────────────
class PairCharCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *chr) override {
        std::string raw = chr->getValue();
        Serial.printf("[ble] pair char write: %u bytes\n", (unsigned)raw.size());
        if (raw.empty() || raw.size() > 480) {
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
        // If the scan completed since last read, refresh the value
        if (!g_wifi_scan_published && WiFi.scanComplete() >= 0) {
            publish_wifi_list();
        }
        // Whatever's in the characteristic value gets returned automatically
    }
};

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *srv) override {
        g_client_connected = true;
        Serial.println("[ble] client connected");
        // Kick off a fresh WiFi scan so the list is current when the app reads 0x0004
        g_wifi_scan_published = false;
        WiFi.mode(WIFI_STA);
        WiFi.scanNetworks(true);   // async, non-blocking
    }
    void onDisconnect(BLEServer *srv) override {
        g_client_connected = false;
        Serial.println("[ble] client disconnected");
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
    if (g_server) {
        BLEDevice::stopAdvertising();
        BLEDevice::deinit(true);
        g_server = nullptr;
        g_pair_char = nullptr;
        g_status_char = nullptr;
        g_wifilist_char = nullptr;
    }
}

// Called from main loop — publishes WiFi scan results when they arrive
void pairing_ble_loop(void) {
    if (!g_server) return;
    if (g_wifi_scan_published) return;
    if (WiFi.scanComplete() >= 0) {
        publish_wifi_list();
    }
}

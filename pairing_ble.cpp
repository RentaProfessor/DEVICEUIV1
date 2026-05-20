// BLE pairing receiver — implements the iOS app contract:
//   1. App connects, discovers service 1ec0de7a-...0001
//   2. App subscribes to notify on status characteristic ...0003
//   3. App writes JSON to pair characteristic ...0002:
//        {"token":"...","ssid":"...","pw":"...","acct":"..."}
//   4. Device validates token, sends 0x01 (validating)
//   5. Device stores creds in NVS, sends 0x02 (wifi connecting)
//   6. Device calls WiFi.begin(), waits up to 20s
//   7. On success: sends 0x03, calls pairing_mark_complete()
//      On failure: sends 0xE3
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

static BLEServer        *g_server    = nullptr;
static BLECharacteristic *g_pair_char = nullptr;
static BLECharacteristic *g_status_char = nullptr;
static volatile bool     g_client_connected = false;

// Send a single byte over the status characteristic notify (no-op if no subscriber)
static void notify_status(uint8_t code) {
    if (!g_status_char) return;
    uint8_t b = code;
    g_status_char->setValue(&b, 1);
    g_status_char->notify();
    Serial.printf("[ble] status notify: 0x%02X\n", code);
    delay(20);   // give the stack a tick to actually flush
}

// Attempt WiFi connection with provided creds. Returns true if connected within timeout.
static bool wifi_connect_with(const char *ssid, const char *pw, uint32_t timeout_ms) {
    Serial.printf("[ble] connecting to WiFi SSID='%s'\n", ssid);
    WiFi.mode(WIFI_STA);
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
    Serial.printf("[ble] WiFi connect failed, status=%d\n", WiFi.status());
    WiFi.disconnect(true);
    return false;
}

class PairCharCallbacks : public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *chr) override {
        std::string raw = chr->getValue();
        Serial.printf("[ble] pair char write: %u bytes\n", (unsigned)raw.size());
        if (raw.empty() || raw.size() > 480) {
            notify_status(LT_STATUS_ERR_JSON);
            return;
        }

        notify_status(LT_STATUS_VALIDATING);

        // Parse JSON: {"token":"...","ssid":"...","pw":"...","acct":"..."}
        StaticJsonDocument<512> doc;
        DeserializationError err = deserializeJson(doc, raw.c_str());
        if (err) {
            Serial.printf("[ble] JSON parse failed: %s\n", err.c_str());
            notify_status(LT_STATUS_ERR_JSON);
            return;
        }

        const char *token = doc["token"] | (const char *)nullptr;
        const char *ssid  = doc["ssid"]  | (const char *)nullptr;
        const char *pw    = doc["pw"]    | "";   // empty string for open networks
        const char *acct  = doc["acct"]  | (const char *)nullptr;
        if (!token || !ssid || !acct) {
            Serial.println("[ble] payload missing required field");
            notify_status(LT_STATUS_ERR_JSON);
            return;
        }

        // Token must match the one in the QR code we displayed
        const char *expected = pairing_get_token_hex();
        if (strcmp(token, expected) != 0) {
            Serial.printf("[ble] token mismatch (got '%s', expected '%s')\n", token, expected);
            notify_status(LT_STATUS_ERR_TOKEN);
            return;
        }

        // Stash credentials in NVS BEFORE attempting WiFi so they survive a crash
        if (!pairing_store_credentials(ssid, pw, acct)) {
            notify_status(LT_STATUS_ERR_INTERNAL);
            return;
        }

        notify_status(LT_STATUS_WIFI_CONNECTING);

        // Try the network. 20s timeout (the iOS app waits up to 30s end-to-end).
        if (!wifi_connect_with(ssid, pw, 20000)) {
            notify_status(LT_STATUS_ERR_WIFI);
            return;
        }

        // All good — flip the paired flag and fire the complete event for the
        // main loop to consume and transition the UI.
        pairing_mark_complete();
        notify_status(LT_STATUS_PAIRED);
    }
};

class ServerCallbacks : public BLEServerCallbacks {
    void onConnect(BLEServer *srv) override {
        g_client_connected = true;
        Serial.println("[ble] client connected");
    }
    void onDisconnect(BLEServer *srv) override {
        g_client_connected = false;
        Serial.println("[ble] client disconnected");
        // Restart advertising in case it was a failed pairing attempt and
        // the app wants to retry. No-op if pairing already completed (the
        // main loop will call pairing_ble_stop() on completion event).
        if (!pairing_is_complete()) {
            BLEDevice::startAdvertising();
        }
    }
};

void pairing_ble_begin(void) {
    if (pairing_is_complete()) {
        Serial.println("[ble] already paired, skipping BLE init");
        return;
    }

    // Advertise name = "LegacyTape-LT-XXXXXX" so the iOS app can confirm match
    char ble_name[24];
    snprintf(ble_name, sizeof(ble_name), "LegacyTape-%s", pairing_get_device_id());

    BLEDevice::init(ble_name);
    BLEDevice::setMTU(247);   // requested; iOS may or may not honor

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

    svc->start();

    // Advertisement: include service UUID in PRIMARY packet so the iOS app
    // can use scanForPeripherals(withServices:) for battery-efficient scans
    // and background BLE on iOS.
    BLEAdvertising *adv = BLEDevice::getAdvertising();
    adv->addServiceUUID(LT_BLE_SERVICE_UUID);
    adv->setScanResponse(true);             // full name goes in scan response
    adv->setMinPreferred(0x06);             // 7.5 ms — match iOS friendly interval
    adv->setMinPreferred(0x12);
    BLEDevice::startAdvertising();

    Serial.printf("[ble] advertising as '%s' with service %s\n", ble_name, LT_BLE_SERVICE_UUID);
}

void pairing_ble_stop(void) {
    Serial.println("[ble] stopping BLE stack");
    if (g_server) {
        BLEDevice::stopAdvertising();
        // Deinit frees the entire BLE stack (~30 KB). Safe — we're paired now.
        BLEDevice::deinit(true);
        g_server = nullptr;
        g_pair_char = nullptr;
        g_status_char = nullptr;
    }
}

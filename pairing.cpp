#include "pairing.h"
#include <Preferences.h>
#include <esp_system.h>
#include <esp_random.h>
#include <Esp.h>           // ESP.getEfuseMac() — reads MAC from efuse without needing WiFi.begin()

static Preferences prefs;

// NVS namespace + keys
static const char *NS         = "legacytape";
static const char *KEY_TOKEN  = "tok";        // 32 hex chars
static const char *KEY_PAIRED = "paired";     // bool

// In-memory cached strings (stable pointers for callers).
static char s_device_id[12]  = {0};   // "LT-XXXXXX" + null
static char s_token_hex[33]  = {0};   // 32 hex + null
static char s_qr_url[96]     = {0};   // "legacytape://pair?d=LT-XXXXXX&t=<32>"

static void hex_encode(const uint8_t *bytes, size_t n, char *out) {
    static const char H[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        out[i * 2]     = H[(bytes[i] >> 4) & 0xF];
        out[i * 2 + 1] = H[bytes[i] & 0xF];
    }
    out[n * 2] = 0;
}

static void compute_device_id(void) {
    // ESP.getEfuseMac() reads the 48-bit chip MAC from efuse — works at boot,
    // no WiFi.begin() required. Bytes are in reversed order: if real MAC is
    // AA:BB:CC:DD:EE:FF then getEfuseMac() returns 0xFFEEDDCCBBAA.
    // We want the last 3 bytes of the real MAC (DD, EE, FF) = shifts 24, 32, 40.
    uint64_t mac = ESP.getEfuseMac();
    snprintf(s_device_id, sizeof(s_device_id), "LT-%02X%02X%02X",
             (uint8_t)((mac >> 24) & 0xFF),
             (uint8_t)((mac >> 32) & 0xFF),
             (uint8_t)((mac >> 40) & 0xFF));
}

void pairing_begin(void) {
    compute_device_id();

    prefs.begin(NS, false);   // RW namespace

    // Token: load or generate fresh 16 random bytes → 32 hex chars
    String stored = prefs.getString(KEY_TOKEN, "");
    if (stored.length() == 32) {
        strncpy(s_token_hex, stored.c_str(), 32);
        s_token_hex[32] = 0;
        Serial.println("[pairing] loaded existing token from NVS");
    } else {
        uint8_t raw[16];
        esp_fill_random(raw, sizeof(raw));
        hex_encode(raw, sizeof(raw), s_token_hex);
        prefs.putString(KEY_TOKEN, s_token_hex);
        Serial.println("[pairing] generated new token, persisted to NVS");
    }

    // Build the QR URL once
    snprintf(s_qr_url, sizeof(s_qr_url),
             "legacytape://pair?d=%s&t=%s",
             s_device_id, s_token_hex);

    Serial.printf("[pairing] device ID: %s\n", s_device_id);
    Serial.printf("[pairing] QR URL:    %s\n", s_qr_url);
    Serial.printf("[pairing] paired:    %s\n",
                  prefs.getBool(KEY_PAIRED, false) ? "yes" : "no");

    prefs.end();
}

bool pairing_is_complete(void) {
    prefs.begin(NS, true);    // read-only
    bool p = prefs.getBool(KEY_PAIRED, false);
    prefs.end();
    return p;
}

void pairing_mark_complete(void) {
    prefs.begin(NS, false);
    prefs.putBool(KEY_PAIRED, true);
    prefs.end();
    Serial.println("[pairing] marked complete in NVS");
}

void pairing_factory_reset(void) {
    prefs.begin(NS, false);
    prefs.clear();
    prefs.end();
    s_token_hex[0] = 0;
    s_qr_url[0] = 0;
    Serial.println("[pairing] factory reset — token + paired flag wiped");
}

const char *pairing_get_qr_url(void)     { return s_qr_url; }
const char *pairing_get_device_id(void)  { return s_device_id; }
const char *pairing_get_token_hex(void)  { return s_token_hex; }

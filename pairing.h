// Legacy Tape — pairing token + QR URL generator.
//
// Minimal device-side support for an eventual app-driven BLE pairing flow.
// Generates a stable per-device random token on first boot, persists it in
// NVS (Preferences), and exposes the deep-link URL the QR code encodes.
//
// The actual BLE handshake (receiving WiFi creds + account info from the app)
// is intentionally NOT implemented here — that's the app/cofounder side. This
// module just gives us a token to render and gives us the API surface to
// flip "paired" later when the firmware receives the credentials.
#ifndef LEGACYTAPE_PAIRING_H
#define LEGACYTAPE_PAIRING_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// Call once in setup() AFTER ui_init() and BEFORE the first display refresh.
// Initializes NVS, loads-or-generates the device token + device ID.
void pairing_begin(void);

// Returns true if pairing has been completed previously (NVS flag set).
// Currently always false until the app/firmware writes the flag.
bool pairing_is_complete(void);

// Mark the device as paired. Called from the BLE-receive callback once the
// app delivers WiFi credentials. Persisted in NVS, survives reboot.
void pairing_mark_complete(void);

// Wipe stored token + paired flag. Forces a fresh pairing on next boot.
// Useful for a Settings → Reset Pairing button.
void pairing_factory_reset(void);

// Pairing URL the QR code encodes, e.g.:
//   legacytape://pair?d=LT-A1B2C3&t=8c4a7b9e6f1d2a3b5c4d6e7f8a9b0c1d
// Pointer stays valid for the lifetime of the program.
const char *pairing_get_qr_url(void);

// Human-readable device ID derived from the ESP32 MAC.
// Format: "LT-XXXXXX" (last 3 bytes of MAC as hex).
const char *pairing_get_device_id(void);

// Raw token (32 lowercase hex chars, null-terminated).
const char *pairing_get_token_hex(void);

#ifdef __cplusplus
}
#endif
#endif

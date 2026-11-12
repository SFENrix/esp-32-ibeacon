#pragma once

// ============================================================
//  SHARED CONTRACT — must match the iOS app and the ESP32 hub.
// ============================================================
#define PROJECT_BEACON_UUID "BEE80189-496A-4F70-8BDA-C68CAAF94EE2"

// Which zone this beacon marks. major = area/floor, minor = room.
// Each env in platformio.ini sets its own, so two boards = two zones.
#ifndef DEFAULT_ZONE_MAJOR
#define DEFAULT_ZONE_MAJOR  1
#endif
#ifndef DEFAULT_ZONE_MINOR
#define DEFAULT_ZONE_MINOR  1
#endif

// ============================================================
//  RADIO DEFAULTS — all changeable at runtime from the Serial
//  Monitor (type "help"). Changes are saved and survive reboot.
// ============================================================

// Transmit power in dBm. Higher = longer range, but also more likely to be
// heard through walls in the neighbouring zone. Usable steps (3 dB apart):
// ESP32-S3: -24 … +20 dBm, ESP32-C6: -15 … +20 dBm.
#define DEFAULT_TX_DBM        0

// How often the beacon broadcasts, in milliseconds (20 to 10240).
// Shorter = phone notices it faster; longer = less power used.
#define DEFAULT_INTERVAL_MS   100

// "Measured power": the RSSI an iPhone sees at 1 m from this beacon.
// iOS uses it to estimate distance. -59 is a common starting value;
// calibrate it with the app's Staff tab (see README).
#define DEFAULT_MEASURED_POWER -59

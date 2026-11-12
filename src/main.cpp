// Zone Beacon — ESP32-S3 / ESP32-C6 iBeacon that marks one zone/room (test build)
//
// Broadcasts:  UUID BEE80189-…  major/minor = zone ID  (Apple iBeacon format)
// The staff iPhone picks this up in the background and reports the zone.
//
// Type commands in the Serial Monitor (115200 baud) to tune it live:
//   help                  list commands
//   status                show current settings
//   tx <dBm>              transmit power, S3: -24 … 20, C6: -15 … 20 (e.g. tx -12)
//   int <ms>              broadcast interval, 20 … 10240 (e.g. int 300)
//   zone <major> <minor>  change the zone ID        (e.g. zone 1 2)
//   mp <dBm>              measured power at 1 m     (e.g. mp -62)
//   off / on              stop / start broadcasting (simulate leaving the zone)
//   defaults              go back to the values in config.h
// Settings are saved to flash and survive a reboot.

#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEAdvertising.h>
#include <Preferences.h>
#include <esp_bt.h>
#include <stdarg.h>
#include "config.h"

// ---------- settings ----------
struct Settings {
  uint16_t major;
  uint16_t minor;
  int8_t   txDbm;
  uint16_t intervalMs;
  int8_t   measuredPower;
  bool     on;
};
static Settings cfg;
static Preferences prefs;

static void loadDefaults() {
  cfg = { DEFAULT_ZONE_MAJOR, DEFAULT_ZONE_MINOR, DEFAULT_TX_DBM,
          DEFAULT_INTERVAL_MS, DEFAULT_MEASURED_POWER, true };
}

static void loadSettings() {
  loadDefaults();
  prefs.begin("beacon", true);
  cfg.major         = prefs.getUShort("major", cfg.major);
  cfg.minor         = prefs.getUShort("minor", cfg.minor);
  cfg.txDbm         = prefs.getChar("tx", cfg.txDbm);
  cfg.intervalMs    = prefs.getUShort("int", cfg.intervalMs);
  cfg.measuredPower = prefs.getChar("mp", cfg.measuredPower);
  cfg.on            = prefs.getBool("on", cfg.on);
  prefs.end();
}

static void saveSettings() {
  prefs.begin("beacon", false);
  prefs.putUShort("major", cfg.major);
  prefs.putUShort("minor", cfg.minor);
  prefs.putChar("tx", cfg.txDbm);
  prefs.putUShort("int", cfg.intervalMs);
  prefs.putChar("mp", cfg.measuredPower);
  prefs.putBool("on", cfg.on);
  prefs.end();
}

// ---------- logging to both USB ports ----------
static void logf(const char* fmt, ...) {
  char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.print(buf);    // native USB port
  Serial0.print(buf);   // UART (USB-to-serial chip) port
}

// ---------- radio ----------
// BLE power steps from the ESP-IDF headers (3 dB apart; the top step is +20 dBm):
//   ESP32-S3: level 0 = -24 dBm … level 15 = +20 dBm
//   ESP32-C6: level 3 = -15 dBm … level 15 = +20 dBm (no steps below -15)
#if defined(CONFIG_IDF_TARGET_ESP32C6)
static const int MIN_LEVEL = 3;
#else
static const int MIN_LEVEL = 0;
#endif
static const int MIN_DBM = MIN_LEVEL * 3 - 24;
static const int MAX_DBM = 20;

static esp_power_level_t dbmToLevel(int dbm) {
  if (dbm >= 19) return (esp_power_level_t)15;
  int lvl = (int)lround((dbm + 24) / 3.0);
  if (lvl < MIN_LEVEL) lvl = MIN_LEVEL;
  if (lvl > 14) lvl = 14;
  return (esp_power_level_t)lvl;
}
static int levelToDbm(int lvl) { return lvl >= 15 ? 20 : lvl * 3 - 24; }

static bool parseUuid(const char* s, uint8_t out[16]) {
  int n = 0;
  for (const char* p = s; *p && n < 32; p++) {
    char c = *p;
    if (c == '-') continue;
    int v = (c >= '0' && c <= '9') ? c - '0'
          : (c >= 'a' && c <= 'f') ? c - 'a' + 10
          : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
    if (v < 0) return false;
    if (n % 2 == 0) out[n / 2] = v << 4; else out[n / 2] |= v;
    n++;
  }
  return n == 32;
}

static uint8_t uuidBytes[16];

static void applyRadio() {
  BLEAdvertising* adv = BLEDevice::getAdvertising();
  adv->stop();

  // Apple iBeacon payload (manufacturer-specific data, all big-endian):
  // 4C 00 | 02 15 | UUID (16) | major (2) | minor (2) | measured power (1)
  uint8_t m[25];
  m[0] = 0x4C; m[1] = 0x00;           // Apple company ID (little-endian)
  m[2] = 0x02; m[3] = 0x15;           // iBeacon type, 21-byte length
  memcpy(&m[4], uuidBytes, 16);
  m[20] = cfg.major >> 8; m[21] = cfg.major & 0xFF;
  m[22] = cfg.minor >> 8; m[23] = cfg.minor & 0xFF;
  m[24] = (uint8_t)cfg.measuredPower;

  BLEAdvertisementData data;
  data.setFlags(0x06);                // LE general discoverable, no classic BT
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  data.setManufacturerData(String((const char*)m, sizeof(m)));       // core 3.x
#else
  data.setManufacturerData(std::string((const char*)m, sizeof(m)));  // core 2.x
#endif
  // 3 + 27 = 30 of 31 bytes used

  adv->setAdvertisementData(data);
  adv->setScanResponse(false);
#if defined(CONFIG_NIMBLE_ENABLED)
  adv->setAdvertisementType(BLE_GAP_CONN_MODE_NON);   // broadcast only (NimBLE stack)
#else
  adv->setAdvertisementType(ADV_TYPE_NONCONN_IND);    // broadcast only (Bluedroid stack)
#endif
  uint16_t units = (uint16_t)(cfg.intervalMs / 0.625); // BLE counts 0.625 ms steps
  adv->setMinInterval(units);
  adv->setMaxInterval(units);

  esp_power_level_t lvl = dbmToLevel(cfg.txDbm);
  BLEDevice::setPower(lvl, ESP_BLE_PWR_TYPE_ADV);
  BLEDevice::setPower(lvl, ESP_BLE_PWR_TYPE_DEFAULT);

  if (cfg.on) adv->start();
}

// ---------- TX power readback ----------
// Asks the BLE controller what power it is actually using, to confirm setPower() applied.
//   S3: esp_ble_tx_power_get(type)
//   C6: esp_ble_tx_power_get_enhanced(type, handle). Handle 0 = the single
//       legacy advertising set this beacon uses (0 is also used for "default").
struct PowerReadback { int adv; int def; const char* api; };

static PowerReadback readTxPower() {
#if defined(CONFIG_IDF_TARGET_ESP32C6)
  return { (int)esp_ble_tx_power_get_enhanced(ESP_BLE_ENHANCED_PWR_TYPE_ADV, 0),
           (int)esp_ble_tx_power_get_enhanced(ESP_BLE_ENHANCED_PWR_TYPE_DEFAULT, 0),
           "esp_ble_tx_power_get_enhanced()" };
#else
  return { (int)esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_ADV),
           (int)esp_ble_tx_power_get(ESP_BLE_PWR_TYPE_DEFAULT),
           "esp_ble_tx_power_get()" };
#endif
}

static bool validLevel(int lvl) { return lvl >= MIN_LEVEL && lvl <= 15; }

// "0 dBm (level 8)" or "invalid (raw 255)"
static void fmtLevel(char* out, size_t len, int lvl) {
  if (validLevel(lvl)) snprintf(out, len, "%d dBm (level %d)", levelToDbm(lvl), lvl);
  else                 snprintf(out, len, "invalid (raw %d)", lvl);
}

static void printStatus() {
  const int wantLvl = (int)dbmToLevel(cfg.txDbm);
  const PowerReadback rb = readTxPower();
  char advTxt[32], defTxt[32];
  fmtLevel(advTxt, sizeof(advTxt), rb.adv);
  fmtLevel(defTxt, sizeof(defTxt), rb.def);
  const char* verdict = !validLevel(rb.adv) ? "CHECK: could not read advertising power"
                      : (rb.adv == wantLvl)  ? "OK, applied"
                                             : "MISMATCH: radio is not using the requested power";

  logf("\n--- Zone Beacon status (%s) ---\n", CONFIG_IDF_TARGET);
  logf("Broadcasting : %s\n", cfg.on ? "ON" : "OFF");
  logf("UUID         : %s\n", PROJECT_BEACON_UUID);
  logf("Zone         : major %u, minor %u\n", cfg.major, cfg.minor);
  logf("TX requested : %d dBm (level %d)\n", cfg.txDbm, wantLvl);
  logf("TX readback  : adv %s, default %s\n", advTxt, defTxt);
  logf("               via %s -> %s\n", rb.api, verdict);
  logf("Interval     : %u ms\n", cfg.intervalMs);
  logf("Measured pwr : %d dBm at 1 m\n", cfg.measuredPower);
  logf("BLE address  : %s\n", BLEDevice::getAddress().toString().c_str());
  logf("Uptime       : %lu s\n\n", millis() / 1000);
}

static void printHelp() {
  logf("\nCommands:\n"
       "  status                 show settings\n"
       "  tx <dBm>               transmit power, %d..%d  (e.g. tx -12)\n"
       "  int <ms>               interval 20..10240      (e.g. int 300)\n"
       "  zone <major> <minor>   zone ID                 (e.g. zone 1 2)\n"
       "  mp <dBm>               measured power at 1 m   (e.g. mp -62)\n"
       "  off | on               stop / start broadcasting\n"
       "  defaults               restore config.h values\n\n", MIN_DBM, MAX_DBM);
}

// ---------- serial commands ----------
static void handleCommand(char* line) {
  char cmd[12] = "";
  long a = 0, b = 0;
  int n = sscanf(line, "%11s %ld %ld", cmd, &a, &b);
  if (n < 1) return;

  bool changed = true;
  if (!strcmp(cmd, "help"))          { printHelp(); changed = false; }
  else if (!strcmp(cmd, "status"))   { printStatus(); changed = false; }
  else if (!strcmp(cmd, "on"))       { cfg.on = true; }
  else if (!strcmp(cmd, "off"))      { cfg.on = false; }
  else if (!strcmp(cmd, "defaults")) { loadDefaults(); }
  else if (!strcmp(cmd, "tx") && n >= 2 && a >= -24 && a <= 21) {
    // values below this chip's minimum snap to the lowest step
    cfg.txDbm = (int8_t)levelToDbm(dbmToLevel(a));   // snap to a real step
  }
  else if (!strcmp(cmd, "int") && n >= 2 && a >= 20 && a <= 10240) {
    cfg.intervalMs = (uint16_t)a;
  }
  else if (!strcmp(cmd, "zone") && n >= 3 && a >= 0 && a <= 65535 && b >= 0 && b <= 65535) {
    cfg.major = (uint16_t)a; cfg.minor = (uint16_t)b;
  }
  else if (!strcmp(cmd, "mp") && n >= 2 && a >= -100 && a <= 0) {
    cfg.measuredPower = (int8_t)a;
  }
  else {
    logf("?? not understood: \"%s\" (type help)\n", line);
    changed = false;
  }

  if (changed) {
    applyRadio();
    saveSettings();
    printStatus();
  }
}

static char lineBuf[64];
static size_t lineLen = 0;

static void feedChar(int c) {
  if (c == '\r' || c == '\n') {
    if (lineLen > 0) {
      lineBuf[lineLen] = '\0';
      handleCommand(lineBuf);
      lineLen = 0;
    }
  } else if (lineLen < sizeof(lineBuf) - 1) {
    lineBuf[lineLen++] = (char)c;
  }
}

// ---------- main ----------
void setup() {
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // don't stall when the native USB port is unplugged
  Serial0.begin(115200);
  delay(1500);

  logf("\n=== Zone Beacon (%s) ===\n", CONFIG_IDF_TARGET);
  if (!parseUuid(PROJECT_BEACON_UUID, uuidBytes)) {
    logf("!! PROJECT_BEACON_UUID in config.h is not a valid UUID. Halting.\n");
    while (true) delay(1000);
  }

  loadSettings();
  BLEDevice::init("ZoneBeacon");
  applyRadio();
  printStatus();
  printHelp();
}

void loop() {
  while (Serial.available())  feedChar(Serial.read());
  while (Serial0.available()) feedChar(Serial0.read());

  static uint32_t lastBeat = 0;
  if (millis() - lastBeat > 60000) {
    lastBeat = millis();
    logf("-- up %lus | zone %u-%u | %s | tx %d dBm | every %u ms\n",
         millis() / 1000, cfg.major, cfg.minor, cfg.on ? "broadcasting" : "OFF",
         cfg.txDbm, cfg.intervalMs);
  }
  delay(10);
}

# Zone Beacon (ESP32-S3), test build

Temporary setup while there's only one ESP32:

```
[ESP32-S3 beacon] --BLE iBeacon--> [iPhone: Staff tab] --MQTT--> [broker] --> [iPad/iPhone: Hub tab]
```
The ESP32-S3 hub firmware (`zone-hub`) still works unchanged. When you get a second board, flash it as the hub and it receives the same messages.

## 1. Flash it
1. Open this folder in VS Code with PlatformIO, then **Upload** and **Monitor**.
2. The first build downloads platform v7.1.3 (Arduino core 2.0.17). This is pinned deliberately; leave it as is.
3. You should see:
```
=== Zone Beacon (ESP32-S3) ===
Broadcasting : ON
UUID         : BEE80189-496A-4F70-8BDA-C68CAAF94EE2
Zone         : major 1, minor 1
TX power     : 0 dBm (radio reports 0 dBm)
```

**Quick check without the app:** install **nRF Connect** (free) on the iPhone and scan. iOS hides iBeacon contents from normal Bluetooth scanners, so you may only see an unnamed device or nothing at all. That's expected, and the app's Staff tab is the real check. On Android, nRF Connect shows it as an iBeacon with the UUID.

## 2. Live tuning (type in the Serial Monitor)
| Command | What it does |
|---|---|
| `status` | show settings |
| `tx -12` | transmit power in dBm (-24 to 21, 3 dB steps) |
| `int 300` | broadcast interval in ms (20 to 10240) |
| `zone 1 2` | change the zone ID, to pretend you walked into another room |
| `off` / `on` | stop/start broadcasting, to test "exit" without walking away |
| `mp -62` | measured power at 1 m (see calibration below) |
| `defaults` | back to `config.h` values |

Settings are saved to flash and survive a reboot.

## 3. Feasibility tests worth running
1. **Range per power level.** At `tx 0`, `tx -12` and `tx 9`, walk away until the Staff tab stops seeing the beacon. Note the distance and RSSI.
2. **Wall bleed.** Stand in the next room. If the phone still detects the zone, lower `tx`. This is the main tuning knob for keeping zones separate.
3. **Enter delay.** Phone locked in your pocket, beacon `off` → `on`. Time how long until the enter event shows on the Hub tab.
4. **Exit delay.** Beacon `on` → `off`. iOS normally waits a while before declaring "exit" (often around 30 s in public reports). Measure your actual figure.
5. **Interval vs. speed.** Repeat test 3 at `int 100` and `int 1000`.

## 4. Calibrate measured power
Hold the iPhone exactly 1 m from the beacon with the Staff tab open. Wait about 30 s and read the average RSSI, then set it with `mp <value>`, e.g. `mp -63`. This only affects iOS's distance estimate, not detection.

## Shared contract
| Item | Value |
|---|---|
| UUID | `BEE80189-496A-4F70-8BDA-C68CAAF94EE2` |
| Test zone | major `1`, minor `1` |

## Notes
- The datasheet's BLE power range for the ESP32-S3-WROOM-1 is -25 to +20 dBm (Table 20). The SDK's top step is labelled +21 dBm; treat it as the module's maximum.
- Compile-checked on Arduino-ESP32 2.0.17 for the ESP32-S3 (69% flash). Not yet tested on a physical board.

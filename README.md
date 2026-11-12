# ESP32 iBeacon

PlatformIO firmware for broadcasting an Apple iBeacon from either an
ESP32-S3 or ESP32-C6. Each board advertises the same UUID and uses its
major/minor values to identify a zone. The configured defaults make it
possible to use one of each as separate zones:

```
[ESP32-S3 beacon: zone 1-1] --\
                                +--BLE iBeacon--> [phone: Staff tab] --MQTT--> [broker] --> [Hub tab]
[ESP32-C6 beacon: zone 1-2] --/
```

The beacon firmware only broadcasts BLE advertisements; the phone app reports
zone events to the broker. The ESP32 hub firmware can continue to receive the
same messages unchanged.

## Boards and PlatformIO environments

| Environment | Board | Default zone |
|---|---|---|
| `beacon_s3` | ESP32-S3 DevKit (`esp32-s3-devkitc-1`) | major `1`, minor `1` |
| `beacon_c6` | ESP32-C6 DevKit (`esp32-c6-devkitc-1`) | major `1`, minor `2` |

The project uses the PlatformIO `espressif32` platform pinned to `8.0.0` and
the Arduino framework. The defaults build both environments. To build or
upload a specific board, select its environment in PlatformIO or use:

```sh
pio run -e beacon_s3
pio run -e beacon_c6
pio run -e beacon_s3 -t upload
pio run -e beacon_c6 -t upload
```

Open the project in VS Code with the PlatformIO extension. Connect the
corresponding board, select its environment, then use **Upload** and
**Monitor**. The serial monitor uses 115200 baud. Firmware logs are sent to
both the native USB serial port and UART.

## Beacon settings

Both boards advertise UUID `BEE80189-496A-4F70-8BDA-C68CAAF94EE2`. On first
boot, their default zones are set by the selected PlatformIO environment
(table above). Settings changed from the serial monitor are saved in flash
and survive a reboot; `defaults` restores the defaults for that environment.

Type `help` in the serial monitor to list commands:

| Command | What it does |
|---|---|
| `status` | Show zone, broadcast state, requested and read-back TX power, interval, and measured power |
| `tx -12` | Set transmit power; usable steps are 3 dB apart |
| `int 300` | Set broadcast interval in milliseconds (20 to 10240) |
| `zone 1 2` | Change the major/minor zone ID |
| `mp -62` | Set measured power at 1 m |
| `off` / `on` | Stop or start broadcasting |
| `defaults` | Restore the selected environment's default settings |

The available transmit power depends on the chip: ESP32-S3 supports -24 to
+20 dBm, and ESP32-C6 supports -15 to +20 dBm. Requested values are rounded
to a supported 3 dB step, and values below a chip's minimum use its lowest
step. Use `status` to check the radio's reported power.

## Testing and calibration

- **Check advertisements:** nRF Connect can scan for the beacon. iOS may hide
  iBeacon contents from ordinary Bluetooth scanners; the app's Staff tab is
  the intended detection check. Android scanners may show the iBeacon UUID.
- **Check range and wall bleed:** Compare reception at different `tx` levels
  and in neighboring rooms. Lower transmit power if a phone continues to
  detect a beacon across a zone boundary.
- **Check enter/exit behavior:** With the phone locked, use `off` and `on` to
  simulate leaving and entering a zone, and measure event delays in the app.
- **Check interval:** Compare detection behavior at different `int` settings.
- **Calibrate measured power:** Hold the phone 1 m from the beacon with the
  Staff tab open, wait about 30 seconds, then set `mp` to the average RSSI
  (for example, `mp -63`). This affects the iOS distance estimate, not beacon
  detection.

## Shared contract

| Item | Value |
|---|---|
| UUID | `BEE80189-496A-4F70-8BDA-C68CAAF94EE2` |
| ESP32-S3 default zone | major `1`, minor `1` |
| ESP32-C6 default zone | major `1`, minor `2` |

The PlatformIO project is configured for both board targets. Confirm builds
and radio behavior with the specific board and toolchain being used; firmware
configuration alone does not establish physical-board testing.

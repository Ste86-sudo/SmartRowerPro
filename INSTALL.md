# SmartRowerPro — Installation Guide

## Requirements

- Arduino IDE 2.x (recommended) or Arduino IDE 1.8.x
- ESP32 board (tested on ESP32-S3 / ESP32)

---

## 1. Install the ESP32 Board Package

1. Open **Arduino IDE → File → Preferences**
2. Add the following URL to *Additional Boards Manager URLs*:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
3. Open **Tools → Board → Boards Manager**
4. Search for `esp32` by Espressif and install it

---

## 2. Install Required Libraries

Open **Tools → Manage Libraries** and install the following:

| Library | Author | Tested version |
|---|---|---|
| `AsyncTCP` | dvarrel | 3.3.9 |
| `ESPAsyncWebServer` | lacamera | 3.7.6 |
| `ADS1220_WE` | Wolfgang Ewald | 1.0.6 |

> **Built-in libraries** (already included with the ESP32 core, no manual install needed):
> `WiFi`, `SPI`, `Preferences`, `ArduinoOTA`, `Update`, `BLEDevice` / `BLEServer` / `BLEUtils` / `BLE2902`

### Alternative: arduino-cli

If you use `arduino-cli`, install all dependencies in one command:

```bash
arduino-cli lib install --config-file sketch.yaml
```

---

## 3. Board & Upload Settings

| Setting | Value |
|---|---|
| Board | `ESP32 Dev Module` (or your specific variant) |
| Upload Speed | `921600` |
| CPU Frequency | `240MHz` |
| Flash Size | `4MB (32Mb)` |
| Partition Scheme | `Default 4MB with spiffs` |

---

## 4. Flash the Firmware

1. Connect the ESP32 via USB
2. Select the correct **Port** under **Tools → Port**
3. Click **Upload**

---

## 5. OTA Updates (after first flash)

1. Connect to the Wi-Fi network **`RP_AP`** (password: `password`)
2. Open a browser and go to `http://192.168.4.1/update`
3. Select the `.bin` file exported from **Sketch → Export Compiled Binary**
4. Click **START FLASH**

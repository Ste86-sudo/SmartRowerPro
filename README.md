# SmartRowerPro
🚣‍♂️ Smart Rower Pro (BLE FTMS &amp; Web App)

Transform a standard rowing machine into a competitive-grade smart ergometer. This open-source project uses an ESP32 and a 24-bit ADC to acquire real-time force data from a load cell. It transmits the data via Bluetooth Low Energy (FTMS protocol) to simulators like **EXR** or **Kinomap** while hosting a built-in Web App for standalone training.

Unlike classic magnetic sensors that estimate power based on flywheel speed, this project measures the actual pulling force using a load cell (mechanically replacing the original handle). This results in extremely precise and highly responsive calculations of exerted power (Watts) and overall rowing metrics.

## ✨ Key Features

* **Native Bluetooth FTMS:** Full compatibility with EXR, Kinomap, and standard rowing apps. No lag, maximum responsiveness.
* **1kHz Data Acquisition:** Ultra-fast and precise force reading via the ADS1220 24-bit ADC.
* **Built-in Web App:** No third-party apps to install. The ESP32 creates its own Wi-Fi Access Point: just connect and open your browser.
* **Advanced Training Programs:** Free Style, Fixed Target, Interval Training (HIIT), Advanced Multi-Set HIIT, Custom Tabata, and Custom Pyramid.
* **Ghost Pacer & Curve Analysis:** Real-time animation of the drive/recovery phases and live plotting of the force curve.
* **Dynamic Thresholds:** Web-configurable trigger limits (in Kg) for pull initiation and handle return to perfectly match your load cell setup.
* **OTA Updates:** Flash new firmware updates directly Over-The-Air from the browser, no USB cables needed.

## 🛠️ Hardware Requirements

* **Microcontroller:** ESP32-S3 (or ESP32-C3). Chosen for native BLE 5.0 support and processing power.
* **ADC:** ADS1220 module (High-precision 24-bit Analog-to-Digital Converter).
* **Sensor:** Load cell (S-Type or Ring type), mechanically installed in place of the original pulling handle.
* **Power Supply:** Standard 5V Powerbank or a LiPo battery module connected to the ESP32.

### Basic Wiring (SPI)
The ADS1220 communicates with the ESP32 via the SPI bus.

| ADS1220 Pin | ESP32 Pin (Example) |
| :--- | :--- |
| VCC | 3.3V |
| GND | GND |
| CS | Pin 3 |
| SCK | Pin 2 |
| MOSI | Pin 1 |
| MISO | Pin 0 |
| DRDY | Pin 5 |


## 🚀 Installation and Setup

### 1. Firmware Flashing
1. Open `SmartRowerPro/SmartRowerPro.ino` with the Arduino IDE.
2. Ensure you have the ESP32 core installed (via Boards Manager).
3. Install the required libraries:
   - `ESPAsyncWebServer`
   - `AsyncTCP`
   - `ADS1220_WE`
4. Compile and upload the firmware to your ESP32.

### 2. First Boot and Calibration
1. Power up the ESP32. It will broadcast a Wi-Fi network named **`RP_AP`** (password: **`password`**).
2. Connect to this network using your smartphone or PC and open your browser to `http://192.168.4.1`.
3. Navigate to the **SETUP & PROFILE** tab:
   - Enter your Height and Weight (used for precise Watt/Distance biological calculations).
   - Ensure the handle is at rest with no tension, then click **SET ZERO** (Tare).
   - Apply a known calibration weight to the rope/load cell (e.g., 10 Kg), type the value in the input box, and click **CALIBRATE SENSOR**.


## 📱 Using with EXR or Kinomap

The firmware is designed to give absolute hardware priority to the Bluetooth antenna, ensuring a stutter-free experience on professional simulators while keeping the Web App running.
1. Turn on the Smart Rower Pro.
2. Open EXR (or Kinomap) on your PC, Tablet, or Apple TV.
3. Search for a new Bluetooth rowing machine. The device will show up as **Smart Rower Pro**.
4. Start rowing! Your real-time data (Watts, SPM, Distance) will be broadcasted smoothly.

## 👨‍💻 Author

Developed by **Stefano Farisé**.
Exploring the intersection of industrial automation, signal processing, and DIY fitness hardware.

---
*If you find this project useful, please consider leaving a ⭐️ on the repository! Contributions, pull requests, and bug reports are always welcome.*

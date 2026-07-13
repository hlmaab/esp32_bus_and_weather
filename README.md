# ESP32 Bus & Weather Display

An interactive, ESP32-S3-powered smart display that fetches and displays real-time bus arrival timings and weather information. Built using the **LVGL** graphics library for a smooth, modern User Interface (UI).

## Features
* **Real-time Bus Tracking:** Displays upcoming bus arrival or tunnel transit times.
* **Live Weather Updates:** Pulls current weather data and forecasts.
* **Responsive Touch UI:** Interactive layout tailored for high-resolution touchscreen displays.
* **Custom Chinese Fonts:** Pre-rendered 16px, 26px, and 48px Chinese font support for localized data.

---

## Hardware Requirements

This project is specifically configured for the following hardware:

* **Development Board:** [Waveshare ESP32-S3-Touch-LCD-4.3](https://www.waveshare.com/esp32-s3-touch-lcd-4.3.htm) (4.3-inch 800x480 capacitive touch display).
* **Processor:** ESP32-S3 dual-core Xtensa LX7.

---

## Software & Environment Setup

This project is developed and compiled using the **Arduino IDE**. Follow the steps below to set up your environment:

### 1. Board Manager Setup
1. Open **Arduino IDE**.
2. Go to **File > Preferences**.
3. In **Additional Boards Manager URLs**, add the Espressif official link:
   `https://espressif.github.io/arduino-esp32/package_esp32_index.json`
4. Go to **Tools > Board > Boards Manager...**, search for `esp32`, and install the **esp32** platform (version 2.0.x or 3.0.x as required by your libraries).
5. Select your board under **Tools > Board > ESP32S3 Dev Module** (or your specific Waveshare profile if available).

### 2. Required Libraries
Install the following libraries via the Arduino Library Manager (**Tools > Manage Libraries...**):
* **LVGL** (v8.x.x is used in this project—*do not use v9.x unless updated*)
* **ESP32_Display_Panel** (by Espressif, required for managing the Waveshare LCD driver and interface)
* **ArduinoJson** (for parsing weather/bus API payloads)
* **WiFi** and **HTTPClient** (built into the ESP32 core)

### 3. Configuration Files
* **Custom Board Config:** Ensure `esp_panel_board_custom_conf.h` and `hero_waveshare_43.h` are placed correctly in your sketch folder to map the display pins and initialization sequences for the Waveshare 4.3" screen.
* **LVGL Porting:** The project utilizes `lvgl_v8_port.cpp` and `lvgl_v8_port.h` to handle the display flushing and touch input registering.

---

## How to Deploy

### 1. Clone the Repository
```bash
git clone https://github.com/hlmaab/esp32_bus_and_weather.git
```

### 2. Open the Sketch
* Open `esp32_bus_and_weather.ino` inside the Arduino IDE.

### 3. Configuration & Credentials
#### Wi-Fi & API Setup (`secrets.h`)
To protect your credentials, this repository uses a separate secrets file. Do not hardcode passwords directly into the main sketch.

1. Locate `secrets.example.h` in the project root directory.
2. Duplicate or rename the file to `secrets.h`.
3. Open `secrets.h` and input your network credentials and API keys:
   ```cpp
   const char *ssid     = "YOUR_WIFI_SSID";
   const char *password = "YOUR_WIFI_PASSWORD";
   ```
### 4. Configure Your Bus Routes
Open the main sketch file and update your bus route configurations inside the array:

```cpp
BusConfig buses[3] = {
    {"296A", "往牛頭角站(循環線)", "403881982F9E7209", {-1, -1, -1}, nullptr, nullptr, nullptr},
    {"296C", "往長沙灣(海盈邨)", "5527FF8CC85CF139", {-1, -1, -1}, nullptr, nullptr, nullptr},
    {"296D", "往九龍站", "21E3E95EAEB2048C", {-1, -1, -1}, nullptr, nullptr, nullptr}
};
   ```

### 5. Compile & Upload
- Connect your Waveshare ESP32-S3 board to your computer using a USB-C cable.
- Select the correct COM Port under Tools > Port.
- Click the Upload button (Right-pointing arrow).

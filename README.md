#  IoT Hub Companion Bot

A smart companion desktop bot built on **ESP32** that helps you stay productive, healthy, and connected.  
It combines **Pomodoro focus timers**, **alarms**, **hydration reminders**, **weather updates**, and **IoT home appliance control** into one interactive system with a TFT display and BLE notifications.

---

## ✨ Features

- **Pomodoro Timer & Alarm**
  - Set focus sessions (1h, 2h, 3h) with automatic break cycles.
  - Configurable alarm clock with buzzer + LED alerts.

- **Real-Time Clock & Weather**
  - Syncs time via NTP.
  - Fetches live weather data (temperature & humidity) from OpenWeatherMap.

- **Hydration Reminder**
  - Hourly hydration alerts with pop-up notifications.
  - Tracks unread water alerts.

- **IoT Home Assistance**
  - Control appliances (Fan, Bulb, AC, Heater) via Wi-Fi HTTP commands.
  - Toggle device states directly from the TFT menu.

- **BLE Notifications**
  - Receives and stores up to 5 Bluetooth notifications.
  - Scrollable notification hub on the TFT screen.

- **Interactive TFT UI**
  - Rotary encoder navigation with press/long-press actions.
  - Multiple screens: Home, Focus, Alarm, IoT Control, Notifications, AI Mode.

---

## 🛠️ Hardware Requirements

- ESP32 (primary + optional secondary for AI engine)
- TFT display (SPI, using `TFT_eSPI`)
- Rotary encoder with push button
- Buzzer + LEDs
- Wi-Fi network
- BLE-enabled device (for notifications)

---

## 📦 Libraries Used

- `WiFi.h` – Wi-Fi connectivity
- `HTTPClient.h` – REST API calls
- `ArduinoJson.h` – JSON parsing
- `TFT_eSPI.h` – TFT display driver
- `BLEDevice.h`, `BLEServer.h`, `BLEUtils.h` – Bluetooth Low Energy
- `time.h` – NTP time synchronization

---

## ⚙️ Setup

1. Clone the repository and open in Arduino IDE / PlatformIO.
2. Install required libraries.
3. Configure your **Wi-Fi credentials**:
   ```cpp
   const char* ssid     = "YOUR_SSID";
   const char* password = "YOUR_PASSWORD";


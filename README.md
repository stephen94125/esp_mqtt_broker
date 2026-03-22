# ESP32 MQTT Broker & Dashboard

![Dashboard Screenshot](https://raw.githubusercontent.com/stephen94125/esp32_mqtt_broker/main/dashboard.png)

A lightweight and robust MQTT broker running directly on an ESP32. It features a modern, responsive "Glassmorphism" web dashboard to monitor system status and the latest MQTT topics in real-time.

## Features

- **Embedded MQTT Broker:** Powered by `PicoMQTT`, running on port `1883`.
- **Real-time Web Dashboard:** View active connections, free RAM (KB), uptime, and latest MQTT payloads instantly via a web browser.
- **Auto-Refresh:** The dashboard automatically refreshes every 10 seconds to keep data up to date.
- **Resilient WiFi:** Built-in auto-reconnection mechanism ensures the broker stays online even if the router drops.
- **Memory Optimized:** Designed without dynamic allocation strings (`std::map` or `String()` resizing) for topic caching to prevent heap fragmentation and runtime crashes.

## Prerequisites

- **Hardware:** ESP32 Development Board
- **Software:** [PlatformIO](https://platformio.org/) (CLI or VSCode Extension)

## Configuration & Setup

1. **Clone the repository**
   ```bash
   git clone <your-repo-name>
   cd van_mqtt_broker
   ```

2. **Set up WiFi Credentials**
   To keep your network credentials secure, they are separated from the main code. Copy the example file:
   ```bash
   cp include/secrets.example.h include/secrets.h
   ```
   Open `include/secrets.h` and update it with your actual WiFi settings:
   ```cpp
   #define WIFI_SSID "YOUR_WIFI_SSID"
   #define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
   ```
   *(Note: `include/secrets.h` is excluded from git tracking via `.gitignore`.)*

## Build & Upload

Connect your ESP32 to your computer via a data-capable USB cable and run:

```bash
pio run -t upload -t monitor
```

> **Troubleshooting Tip:** If you see an error like `[Errno 35] Resource temporarily unavailable` or `Could not open port`, make sure you don't have another Serial Monitor (or another terminal) holding the USB port open. Close it and try again.

## Usage

1. Wait for the firmware to finish uploading.
2. In the Serial Monitor output, wait for the ESP32 to connect to your WiFi. It will print its local IP:
   ```text
   [WiFi] Connected!
   [WiFi] IP: 192.168.4.2
   [HTTP] Dashboard Started on port 80
   [MQTT] Broker Started on port 1883
   ```
3. Open a web browser on a device connected to the same network and navigate to `http://192.168.4.2` (replace with your actual IP).
4. Send MQTT messages to the broker from any client (e.g., Node-RED, Home Assistant, Mosquitto CLI), and watch the dashboard update!

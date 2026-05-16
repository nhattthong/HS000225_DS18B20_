# ESP32-C3 RF Spectrum Analyzer

Real-time 2.4 GHz + 5.8 GHz spectrum analyzer with waterfall display served over WiFi.

| Band | Hardware | Channels | Range |
|------|----------|----------|-------|
| 2.4 GHz | nRF24L01+ | 128 | 2400–2527 MHz |
| 5.8 GHz | RX5808 FPV | 40 | 5645–5945 MHz |

---

## Features

- 📡 **Waterfall display** — scrolling heat-map (blue→red colour scale)
- 📊 **Spectrum display** — real-time bar chart per band
- 🌐 **Web UI via WebSocket** — no app needed, open any browser
- ⚙ **WiFi AP + Station modes** — switch & configure from the web page
- 🔒 **Network scanner** — scan, pick, and enter password in the UI
- 💾 **Persistent settings** — AP/STA credentials saved to NVS (Preferences)

---

## Hardware Wiring (ESP32-C3)

### nRF24L01+ (hardware SPI – FSPI/SPI2)

| nRF24 pin | ESP32-C3 GPIO |
|-----------|---------------|
| SCK       | 6             |
| MOSI      | 7             |
| MISO      | 2             |
| CSN       | 10            |
| CE        | 3             |
| VCC       | 3.3 V         |
| GND       | GND           |

### RX5808 5.8 GHz module (bit-bang SPI + ADC)

| RX5808 pin | ESP32-C3 GPIO |
|------------|---------------|
| CH1_DATA   | 4             |
| CH1_CLK    | 5             |
| CH1_CS     | 8             |
| RSSI (AV)  | 1 (ADC1_CH1)  |
| VCC        | 5 V ⚠         |
| GND        | GND           |

> **⚠ Voltage note:** The RX5808 module requires a 5 V supply.  
> The SPI/CS logic lines are 3.3 V tolerant on most modules, but confirm with your datasheet.  
> The RSSI pin outputs 0–3.3 V, which is safe for the ESP32-C3 ADC.

---

## Required Libraries

Install via **Arduino Library Manager**:

| Library | Author |
|---------|--------|
| RF24 | TMRh20 |
| ESPAsyncWebServer | lacamera (fork of me-no-dev) |
| AsyncTCP | dvarrel (fork of me-no-dev) |
| ArduinoJson | Benoit Blanchon (v6.x) |

Board: **ESP32C3 Dev Module** (package: `esp32` by Espressif, ≥ 2.0.11)

---

## RSSI Calibration

The RX5808 RSSI pin is an analog voltage that varies by module.  
In `ESP32_SpectrumAnalyzer.ino`, adjust:

```cpp
#define RX5808_RSSI_MIN  200   // ADC count with NO signal
#define RX5808_RSSI_MAX 2800   // ADC count with STRONG signal
```

To calibrate:
1. Open Serial Monitor at 115200 baud.
2. Temporarily add `Serial.println(rx5808.readRSSIRaw());` inside the scan loop.
3. Note the idle ADC value → set as `RX5808_RSSI_MIN`.
4. Place a video transmitter very close → set peak as `RX5808_RSSI_MAX`.

---

## WiFi Modes

### Access Point (default)
ESP32 creates its own hotspot:
- SSID: `SpectrumAnalyzer`
- Password: `spectrum123`
- Web UI: **http://192.168.4.1**

### Station mode
Connect to your home/field router:
1. Open the web UI → **WiFi Configuration** → **Station** tab
2. Click **Scan Networks**, select your AP, enter password, click **Connect**
3. New IP shown in the header; settings saved to flash for next boot

To make Station mode the default after a successful connection, the firmware saves credentials automatically and retries them on the next boot.

---

## Web UI Screenshot

```
⚡ RF SPECTRUM ANALYZER          Mode: 📡 AP   IP: 192.168.4.1   ● Live
──────────────────────────────────────────────────────────────────────
BAND: [BOTH] [2.4 GHz] [5.8 GHz]   nRF24: 9 fps   RX5808: 1 fps
                                    Weak ░░░▒▒▓▓██ Strong
──────────────────────────────────────────────────────────────────────
▶ 2.4 GHz  (nRF24L01 — 2400-2527 MHz)
  [ SPECTRUM bar chart ]
  [ WATERFALL heat-map – scrolling in real time ]
  2400  2420  2440  2460  2480  2500  2520  2527

▶ 5.8 GHz  (RX5808 — 5645-5945 MHz)
  [ SPECTRUM bar chart ]
  [ WATERFALL heat-map ]
  5645  5700  5750  5800  5850  5900  5945

⚙ WiFi Configuration  ▲
  [📡 Access Point]  [🌐 Station (client)]
  AP Name:  SpectrumAnalyzer
  Password: ••••••••••
  [ Apply & Save ]
```

---

## Architecture

```
ESP32-C3
├── setup()
│   ├── RF24 + nRF24Scanner.begin()
│   ├── RX5808.begin()
│   ├── WiFi (AP or STA)
│   ├── AsyncWebServer  :80  → serves INDEX_HTML (PROGMEM)
│   └── AsyncWebSocket  /ws  → bidirectional JSON
└── loop()
    ├── nRF24Scanner.scan()      ~100 ms  → broadcastScan24()
    └── RX5808 sweep (40 ch)    ~1000 ms → broadcastScan58()

Browser
├── WebSocket client (auto-reconnect)
├── Canvas 2D – spectrum bar chart (per band)
└── Canvas 2D – waterfall (ImageData pixel-push)
```

---

## References

- [sheaivey/rx5808-pro-diversity](https://github.com/sheaivey/rx5808-pro-diversity)
- [jyesmith/fenix-rx5808-pro-diversity](https://github.com/jyesmith/fenix-rx5808-pro-diversity)
- [RF24 library docs](https://nRF24.github.io/RF24/)

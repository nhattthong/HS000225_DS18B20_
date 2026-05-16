# ESP32-C3 RF Spectrum Analyzer

Real-time 2.4 GHz + 5.8 GHz spectrum analyzer với giao diện waterfall, phục vụ qua WiFi — không cần cài app.

| Band | Module | Kênh | Dải tần |
|------|--------|------|---------|
| 2.4 GHz | nRF24L01+ | 128 | 2400–2527 MHz |
| 5.8 GHz | RX5808 FPV | 40 | 5645–5945 MHz |

---

## Tính năng

- 📡 **Waterfall display** — heat-map cuộn thời gian thực (blue→red)
- 📊 **Spectrum display** — biểu đồ thanh theo từng band
- 🌐 **Web UI qua WebSocket** — mở trình duyệt bất kỳ, không cần app
- ⚙ **WiFi AP + Station** — chuyển chế độ và cấu hình ngay trên trang web
- 🔒 **Network scanner** — quét, chọn và nhập mật khẩu trực tiếp trên UI
- 💾 **Lưu cài đặt** — thông tin AP/STA lưu vào flash (NVS / Preferences)

---

## Sơ đồ kết nối (ESP32-C3)

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

### RX5808 5.8 GHz (bit-bang SPI + ADC)

| RX5808 pin | ESP32-C3 GPIO |
|------------|---------------|
| CH1_DATA   | 4             |
| CH1_CLK    | 5             |
| CH1_CS     | 8             |
| RSSI (AV)  | 1 (ADC1_CH1)  |
| VCC        | 5 V ⚠         |
| GND        | GND           |

> **⚠ Lưu ý nguồn:** Module RX5808 yêu cầu nguồn 5 V. Các chân SPI/CS tương thích 3.3 V; chân RSSI xuất 0–3.3 V, an toàn cho ADC của ESP32-C3.

---

## Thư viện cần cài

Cài qua **Arduino Library Manager**:

| Thư viện | Tác giả |
|----------|---------|
| RF24 | TMRh20 |
| ESPAsyncWebServer | lacamera (fork của me-no-dev) |
| AsyncTCP | dvarrel (fork của me-no-dev) |
| ArduinoJson | Benoit Blanchon (v6.x) |

Board: **ESP32C3 Dev Module** (package `esp32` by Espressif, ≥ 2.0.11)

---

## Hiệu chỉnh RSSI (RX5808)

Trong file `firmware/ESP32_SpectrumAnalyzer/ESP32_SpectrumAnalyzer.ino`, chỉnh:

```cpp
#define RX5808_RSSI_MIN  200   // ADC khi không có tín hiệu
#define RX5808_RSSI_MAX 2800   // ADC khi có tín hiệu mạnh
```

Cách hiệu chỉnh:
1. Mở Serial Monitor ở 115200 baud.
2. Thêm tạm `Serial.println(rx5808.readRSSIRaw());` vào vòng quét.
3. Ghi nhận giá trị nền → đặt làm `RX5808_RSSI_MIN`.
4. Đưa transmitter lại gần → đặt giá trị đỉnh làm `RX5808_RSSI_MAX`.

---

## Chế độ WiFi

### Access Point (mặc định)
ESP32 tạo hotspot riêng:
- SSID: `SpectrumAnalyzer`
- Password: `spectrum123`
- Web UI: **http://192.168.4.1**

### Station mode
Kết nối vào router của bạn:
1. Mở Web UI → **WiFi Configuration** → tab **Station**
2. Nhấn **Scan Networks**, chọn AP, nhập mật khẩu, nhấn **Connect**
3. IP mới hiển thị trên header; cài đặt tự lưu vào flash cho lần boot sau

---

## Cấu trúc firmware

```
firmware/
└── ESP32_SpectrumAnalyzer/
    ├── ESP32_SpectrumAnalyzer.ino   # Main sketch
    ├── nrf24_scan.h                 # Driver quét 2.4 GHz (nRF24L01+)
    ├── rx5808.h                     # Driver quét 5.8 GHz (RX5808)
    └── web_html.h                   # Trang Web UI (PROGMEM)
```

### Luồng hoạt động

```
ESP32-C3
├── setup()
│   ├── RF24 + nRF24Scanner.begin()
│   ├── RX5808.begin()
│   ├── WiFi (AP hoặc STA)
│   ├── AsyncWebServer  :80  → phục vụ INDEX_HTML (PROGMEM)
│   └── AsyncWebSocket  /ws  → JSON hai chiều
└── loop()
    ├── nRF24Scanner.scan()   ~100 ms  → broadcastScan24()
    └── RX5808 sweep (40 ch)  ~1000 ms → broadcastScan58()

Browser
├── WebSocket client (tự kết nối lại)
├── Canvas 2D – biểu đồ spectrum
└── Canvas 2D – waterfall (ImageData pixel-push)
```

---

## Tài liệu tham khảo

- [sheaivey/rx5808-pro-diversity](https://github.com/sheaivey/rx5808-pro-diversity)
- [jyesmith/fenix-rx5808-pro-diversity](https://github.com/jyesmith/fenix-rx5808-pro-diversity)
- [RF24 library docs](https://nRF24.github.io/RF24/)
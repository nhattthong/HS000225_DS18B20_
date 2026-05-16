# HS000225 – DS18B20 Temperature Sensor (ESP32)

Kho lưu trữ firmware và ví dụ cho cảm biến nhiệt độ **DS18B20** sử dụng với **ESP32**.

---

## Cấu trúc repo

```
├── firmware/
│   ├── DS18B20_ESP32/          # ← Firmware chính (đọc DS18B20 + Web UI)
│   │   ├── DS18B20_ESP32.ino   # Main sketch
│   │   ├── ds18b20.h           # 1-Wire DS18B20 driver (tích hợp, không cần lib ngoài)
│   │   ├── web_html.h          # Web UI real-time (PROGMEM)
│   │   └── README.md           # Hướng dẫn chi tiết
│   │
│   └── ESP32_SpectrumAnalyzer/ # RF Spectrum Analyzer (2.4+5.8GHz)
│
├── examples/
│   ├── DS18B20_Basic/          # Ví dụ đơn giản, in Serial
│   └── DS18B20_MultiSensor/    # Ví dụ nhiều cảm biến + thống kê
│
└── libraries/                  # Thư viện bổ sung (nếu có)
```

---

## DS18B20_ESP32 – Firmware chính

**Tính năng:**
- Tối đa **10 cảm biến DS18B20** trên 1 dây (1-Wire bus)
- **Web UI real-time** qua WebSocket – mở browser là dùng ngay
- Biểu đồ nhiệt độ, Min/Max/Avg, **cảnh báo Alarm**
- WiFi AP mode (mặc định) hoặc Station mode (kết nối router)
- Lưu cài đặt vào NVS flash

**Kết nối nhanh:**
```
DS18B20 DQ  → GPIO 4  (4.7 kΩ pull-up lên 3.3V)
DS18B20 VDD → 3.3V
DS18B20 GND → GND
```

**Truy cập web:** `http://192.168.4.1` (WiFi: `DS18B20_Monitor` / `monitor123`)

→ Xem [firmware/DS18B20_ESP32/README.md](firmware/DS18B20_ESP32/README.md) để biết thêm chi tiết.

---

## Ví dụ

| Ví dụ | Mô tả |
|-------|-------|
| [DS18B20_Basic](examples/DS18B20_Basic/) | Đọc 1 cảm biến, in ra Serial Monitor |
| [DS18B20_MultiSensor](examples/DS18B20_MultiSensor/) | Nhiều cảm biến, Min/Max/Avg, cảnh báo |

---

## Thư viện cần cài (Library Manager)

| Thư viện | Dùng cho |
|----------|---------|
| ESPAsyncWebServer | Web server async |
| AsyncTCP | TCP async (dependency) |
| ArduinoJson v6 | JSON WebSocket protocol |

> Driver DS18B20 (`ds18b20.h`) đã tích hợp sẵn, **không cần** cài OneWire hay DallasTemperature.
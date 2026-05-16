# DS18B20_ESP32 – Temperature Monitor Firmware

ESP32 firmware đọc cảm biến nhiệt độ **DS18B20** (1-Wire), hiển thị real-time qua web browser với WebSocket.

---

## Tính năng

| Tính năng | Mô tả |
|-----------|-------|
| **Đa cảm biến** | Tối đa 10 cảm biến DS18B20 trên 1 dây dữ liệu |
| **Độ chính xác** | 9/10/11/12-bit (mặc định 12-bit = 0.0625 °C) |
| **Web UI** | Giao diện web real-time, không cần app |
| **WebSocket** | Cập nhật nhiệt độ mỗi N giây (configurable) |
| **Thống kê** | Min / Max / Average theo thời gian |
| **Cảnh báo** | Alarm Lo/Hi cho từng cảm biến, thông báo browser |
| **WiFi** | AP mode (mặc định) hoặc Station mode (kết nối router) |
| **NVS** | Lưu cài đặt vào flash, khởi động lại vẫn giữ |
| **Parasite power** | Hỗ trợ chế độ 2 dây (VDD nối GND) |

---

## Phần cứng

### Linh kiện cần thiết
- **ESP32** (any variant) hoặc ESP32-C3, ESP32-S3
- **DS18B20** × 1 đến 10 (TO-92 hoặc module có chống thấm)
- Điện trở **4.7 kΩ** × 1 (pull-up)

### Sơ đồ kết nối

```
          ┌─────────────────────┐
          │        ESP32        │
          │                     │
3.3V ─────┼─ 3.3V     GPIO 4  ─┼──────────┬──── DS18B20 DQ
GND  ─────┼─ GND              │          │
          └─────────────────────┘       4.7kΩ
                                          │
                                         3.3V
```

**DS18B20 chân:**
| Chân | Màu thường | Kết nối |
|------|-----------|---------|
| GND  | Đen       | GND ESP32 |
| DQ   | Vàng/Xanh | GPIO 4 + 4.7kΩ lên 3.3V |
| VDD  | Đỏ        | 3.3V (hoặc GND nếu dùng parasite) |

> **Parasite power (2 dây):** Nối VDD + GND của DS18B20 xuống GND, firmware tự phát hiện.

---

## Cài đặt thư viện

Mở **Arduino IDE → Library Manager** và cài:

| Thư viện | Tác giả |
|----------|---------|
| ESPAsyncWebServer | lacamera / me-no-dev |
| AsyncTCP | dvarrel / me-no-dev |
| ArduinoJson | Benoit Blanchon (v6.x) |

> **Không cần** cài OneWire hay DallasTemperature – driver DS18B20 được tích hợp trong `ds18b20.h`.

---

## Biên dịch & nạp

1. Chọn board: **ESP32 Dev Module** (hoặc board phù hợp)
2. Partition: **Default 4MB with spiffs** (hoặc bất kỳ)
3. Nạp và mở **Serial Monitor** 115200 baud

---

## Sử dụng

### Lần đầu khởi động
1. ESP32 tạo WiFi AP: **`DS18B20_Monitor`** / password: **`monitor123`**
2. Kết nối điện thoại/máy tính vào AP này
3. Mở trình duyệt: **http://192.168.4.1**

### Giao diện web
- **Cards nhiệt độ**: mỗi cảm biến 1 card, màu theo nhiệt độ
  - 🔵 Lạnh (dưới ngưỡng Lo)
  - 🟢 Bình thường
  - 🔴 Nóng (trên ngưỡng Hi)
- **Biểu đồ**: lịch sử 60 điểm đo gần nhất
- **Min/Max/Avg**: thống kê tự động
- **Alarm**: cài ngưỡng Lo/Hi cho từng cảm biến

### Kết nối WiFi router (STA mode)
1. Mở panel **⚙ WiFi Configuration** ở cuối trang
2. Tab **🌐 Station** → Quét mạng → Chọn → Nhập password → Connect
3. ESP32 sẽ nhớ và tự kết nối lại sau khi reset

---

## Cấu hình code

```cpp
// DS18B20_ESP32.ino
#define PIN_ONE_WIRE    4        // GPIO kết nối DQ
#define DS18B20_RESOLUTION DS18B20_RES_12  // 9/10/11/12
#define DEFAULT_INTERVAL_S  5   // chu kỳ đọc (giây)
#define STATS_WINDOW        60  // số mẫu tính avg

#define AP_SSID_DEFAULT   "DS18B20_Monitor"
#define AP_PASS_DEFAULT   "monitor123"
```

---

## WebSocket API

**ESP32 → Browser:**
```json
{"t":"data","sensors":[{"id":0,"name":"Sensor 1","rom":"28FF...","tempC":25.50,"tempF":77.90,"min":22.00,"max":30.00,"avg":25.12,"lo":-10,"hi":85,"alarm":false}],"power":"ext","res":12,"interval":5}
{"t":"alarm","id":0,"name":"Sensor 1","tempC":90.0,"lo":-10,"hi":85}
{"t":"st","m":"ap","ip":"192.168.4.1","ss":"DS18B20_Monitor"}
```

**Browser → ESP32:**
```json
{"c":"status"}
{"c":"setAlarm","id":0,"lo":-10,"hi":85}
{"c":"setInterval","v":5}
{"c":"scan"}
{"c":"ap","s":"MyAP","p":"password"}
{"c":"sta","s":"MyRouter","p":"password"}
```

---

## Cấu trúc file

```
DS18B20_ESP32/
├── DS18B20_ESP32.ino   # Main sketch
├── ds18b20.h           # 1-Wire + DS18B20 driver (no external lib)
├── web_html.h          # Web UI (PROGMEM)
└── README.md           # This file
```

// ============================================================
// DS18B20_Basic.ino
//
// Ví dụ cơ bản: đọc 1 cảm biến DS18B20, in ra Serial.
//
// Kết nối:
//   DS18B20 DQ  → GPIO 4  (4.7 kΩ pull-up lên 3.3V)
//   DS18B20 VDD → 3.3V
//   DS18B20 GND → GND
//
// Thư viện cần cài (Library Manager):
//   (Không cần thêm – driver ds18b20.h tích hợp sẵn)
//
// Chép thư mục này vào Arduino sketchbook, giữ nguyên cấu trúc
// cùng với ds18b20.h (copy từ firmware/DS18B20_ESP32/ds18b20.h).
// ============================================================

#include <Arduino.h>
#include "ds18b20.h"     // copy from firmware/DS18B20_ESP32/ds18b20.h

#define PIN_DQ  4        // DS18B20 data pin

DS18B20Bus bus(PIN_DQ, DS18B20_RES_12);  // 12-bit resolution

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println("\n=== DS18B20 Basic Example ===");

    if (bus.begin()) {
        Serial.print("Found "); Serial.print(bus.count); Serial.println(" sensor(s)");
        Serial.print("Power: "); Serial.println(bus.isParasitePower() ? "Parasite (2-wire)" : "External (3-wire)");
        Serial.print("Resolution: "); Serial.print(DS18B20_RES_12); Serial.println(" bit");
        Serial.print("Conversion time: "); Serial.print(bus.conversionMs()); Serial.println(" ms");
        Serial.println();
        // Print ROM addresses
        for (uint8_t i = 0; i < bus.count; i++) {
            Serial.print("Sensor "); Serial.print(i); Serial.print(" ROM: ");
            Serial.println(bus.romHex(i));
        }
    } else {
        Serial.println("ERROR: No DS18B20 sensor found!");
        Serial.println("Check wiring:");
        Serial.println("  DQ → GPIO4 with 4.7kΩ pull-up to 3.3V");
    }
    Serial.println();
}

void loop() {
    if (bus.count == 0) {
        Serial.println("No sensor – waiting 5s…");
        delay(5000);
        return;
    }

    // Blocking read: request conversion, wait, then read
    bus.readAllBlocking();

    // Print result
    for (uint8_t i = 0; i < bus.count; i++) {
        Serial.print("Sensor "); Serial.print(i); Serial.print(": ");

        if (bus.sensors[i].valid) {
            float tC = bus.getTempC(i);
            float tF = bus.getTempF(i);
            Serial.print(tC, 2); Serial.print(" °C  /  ");
            Serial.print(tF, 2); Serial.println(" °F");
        } else {
            Serial.println("CRC ERROR – check wiring!");
        }
    }
    Serial.println();

    delay(2000);  // read every 2 seconds
}

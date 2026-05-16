// ============================================================
// DS18B20_MultiSensor.ino
//
// Ví dụ nâng cao: đọc nhiều cảm biến DS18B20 cùng lúc,
// thống kê Min/Max/Avg, cảnh báo ngưỡng nhiệt độ.
//
// Kết nối:
//   Tất cả DS18B20 DQ nối CHUNG 1 dây → GPIO 4 (4.7 kΩ pull-up)
//   DS18B20 VDD → 3.3V  (hoặc GND nếu dùng parasite power)
//   DS18B20 GND → GND
//
// Output: Serial Monitor 115200 baud
// ============================================================

#include <Arduino.h>
#include "ds18b20.h"   // copy from firmware/DS18B20_ESP32/ds18b20.h

#define PIN_DQ    4    // Shared 1-Wire data pin for all sensors
#define ALARM_LO  10   // °C – cảnh báo lạnh
#define ALARM_HI  40   // °C – cảnh báo nóng
#define READ_INTERVAL_MS  3000

DS18B20Bus bus(PIN_DQ, DS18B20_RES_12);

// Rolling statistics per sensor
struct Stats {
    float minC =  1e9f;
    float maxC = -1e9f;
    float sumC = 0;
    uint32_t n = 0;
};
Stats stats[DS18B20_MAX_SENSORS];

void printSeparator() {
    Serial.println("─────────────────────────────────────────────");
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    printSeparator();
    Serial.println("  DS18B20 Multi-Sensor Example");
    printSeparator();

    if (!bus.begin()) {
        Serial.println("ERROR: No DS18B20 found on GPIO" + String(PIN_DQ));
        Serial.println("Please check wiring and pull-up resistor (4.7kΩ).");
        return;
    }

    Serial.print("Sensors found : "); Serial.println(bus.count);
    Serial.print("Power mode    : "); Serial.println(bus.isParasitePower() ? "Parasite (2-wire)" : "External (3-wire)");
    Serial.print("Resolution    : "); Serial.print(DS18B20_RES_12); Serial.println("-bit (0.0625 °C)");
    Serial.print("Conv. time    : "); Serial.print(bus.conversionMs()); Serial.println(" ms");
    Serial.println();

    // Print ROM IDs
    Serial.println("ROM addresses:");
    for (uint8_t i = 0; i < bus.count; i++) {
        Serial.print("  ["); Serial.print(i); Serial.print("] ");
        Serial.println(bus.romHex(i));
    }

    // Set alarm thresholds on device EEPROM
    for (uint8_t i = 0; i < bus.count; i++) {
        bus.setAlarm(i, ALARM_LO, ALARM_HI);
    }
    Serial.print("Alarm thresholds: Lo="); Serial.print(ALARM_LO);
    Serial.print("°C  Hi="); Serial.print(ALARM_HI); Serial.println("°C");
    printSeparator();
    Serial.println();
}

void loop() {
    if (bus.count == 0) {
        delay(5000);
        return;
    }

    // Non-blocking: request conversion, yield while waiting
    bus.requestAll();
    uint32_t t0 = millis();
    while (millis() - t0 < bus.conversionMs()) {
        yield();   // keep WiFi/BT stack alive if used
    }
    bus.readAll();

    // Print header
    Serial.printf("%-12s %-10s %-10s %-8s %-8s %-8s  %s\n",
                  "Sensor", "Temp (°C)", "Temp (°F)", "Min(°C)", "Max(°C)", "Avg(°C)", "Status");
    printSeparator();

    for (uint8_t i = 0; i < bus.count; i++) {
        const DS18B20Sensor &s = bus.sensors[i];

        if (!s.valid) {
            Serial.printf("[%d] %-10s  *** CRC ERROR ***\n", i, s.name);
            continue;
        }

        float tC = bus.getTempC(i);
        float tF = bus.getTempF(i);
        Stats &st = stats[i];
        if (tC < st.minC) st.minC = tC;
        if (tC > st.maxC) st.maxC = tC;
        st.sumC += tC; st.n++;

        const char *status = "";
        if (tC >= ALARM_HI)       status = "⚠ HOT";
        else if (tC <= ALARM_LO)  status = "⚠ COLD";
        else                      status = "OK";

        Serial.printf("[%d] %-10s  %+8.2f   %+8.2f   %+7.2f  %+7.2f  %+7.2f  %s\n",
                      i, s.name,
                      tC, tF,
                      st.minC, st.maxC,
                      st.n ? st.sumC / st.n : 0.0f,
                      status);
    }
    Serial.println();

    delay(READ_INTERVAL_MS - bus.conversionMs());
}

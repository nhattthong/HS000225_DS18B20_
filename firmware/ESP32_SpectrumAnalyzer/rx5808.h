#pragma once
// ============================================================
// rx5808.h  –  RX5808 5.8 GHz module driver (bit-bang SPI)
//
// Wiring (ESP32-C3):
//   DATA  → GPIO4   (MOSI, bit-bang)
//   CLK   → GPIO5   (SCK,  bit-bang)
//   CS    → GPIO8   (active-LOW)
//   RSSI  → GPIO1   (ADC1_CH1, analog out of RX5808)
//   GND / 5 V as required by module
//
// Protocol:  25-bit LSB-first write
//   [3:0]  register address  (4 bits)
//   [4]    R/nW flag         (0 = write)
//   [24:5] data              (20 bits)
//
// Channel table: 40 FPV channels across 5 bands
//   Band A  (A1-A8)  – 5865 … 5725 MHz
//   Band B  (B1-B8)  – 5733 … 5866 MHz
//   Band E  (E1-E8)  – 5705 … 5945 MHz
//   Band F  (F1-F8)  – 5740 … 5880 MHz
//   Raceband(R1-R8)  – 5658 … 5917 MHz
// ============================================================
#include <Arduino.h>

#define RX5808_NUM_CHANNELS 40

// Raw synth-register values sent to RX5808 register 0x01
// (source: sheaivey/rx5808-pro-diversity)
static const uint16_t RX5808_REG[RX5808_NUM_CHANNELS] PROGMEM = {
    // Band A
    0x2A05, 0x299B, 0x2991, 0x2987, 0x291D, 0x2913, 0x2909, 0x289F,
    // Band B
    0x2903, 0x290C, 0x2915, 0x291E, 0x2927, 0x2930, 0x2939, 0x2942,
    // Band E
    0x2895, 0x288B, 0x2881, 0x2877, 0x2A15, 0x2A1F, 0x2A29, 0x2A33,
    // Band F
    0x2906, 0x2910, 0x291A, 0x2924, 0x292E, 0x2938, 0x2942, 0x294C,
    // Raceband
    0x2872, 0x288F, 0x2AC8, 0x2ACD, 0x2AD4, 0x2ADB, 0x2AE2, 0x2B67
};

// Matching human-readable frequencies (MHz)
static const uint16_t RX5808_FREQ[RX5808_NUM_CHANNELS] PROGMEM = {
    // Band A
    5865, 5845, 5825, 5805, 5785, 5765, 5745, 5725,
    // Band B
    5733, 5752, 5771, 5790, 5809, 5828, 5847, 5866,
    // Band E
    5705, 5685, 5665, 5645, 5885, 5905, 5925, 5945,
    // Band F
    5740, 5760, 5780, 5800, 5820, 5840, 5860, 5880,
    // Raceband
    5658, 5695, 5732, 5769, 5806, 5843, 5880, 5917
};

// Pre-computed sort order (index into above arrays) by ascending frequency.
// Generated offline; avoids dynamic sort at runtime.
static const uint8_t RX5808_SORT_IDX[RX5808_NUM_CHANNELS] PROGMEM = {
    19, 32, 18, 17, 33, 16,  7, 34,  8, 24,
     6,  9, 25,  5, 35, 10, 26,  4, 11, 27,
     3, 36, 12, 28,  2, 13, 29, 37,  1, 14,
    30,  0, 15, 31, 38, 20, 21, 39, 22, 23
};

// ─────────────────────────────────────────────────────────────
class RX5808 {
public:
    RX5808(uint8_t dataPin, uint8_t clkPin, uint8_t csPin,
           uint8_t rssiPin, uint16_t rssiMin, uint16_t rssiMax)
        : _data(dataPin), _clk(clkPin), _cs(csPin),
          _rssi(rssiPin), _rMin(rssiMin), _rMax(rssiMax) {}

    // Call once in setup()
    void begin() {
        pinMode(_data, OUTPUT);
        pinMode(_clk,  OUTPUT);
        pinMode(_cs,   OUTPUT);
        digitalWrite(_cs,  HIGH);
        digitalWrite(_clk, LOW);
        // Power up the RX5808 synthesiser
        _writeSynth(0);          // clear synth
        delay(10);
        setChannel(0);
    }

    // Set active RX channel (0-39, band-ordered)
    void setChannel(uint8_t ch) {
        if (ch >= RX5808_NUM_CHANNELS) return;
        _writeSynth(pgm_read_word(&RX5808_REG[ch]));
    }

    // Read raw ADC value from RSSI pin
    uint16_t readRSSIRaw(uint8_t samples = 8) {
        uint32_t sum = 0;
        for (uint8_t i = 0; i < samples; i++) {
            sum += analogRead(_rssi);
            delayMicroseconds(200);
        }
        return (uint16_t)(sum / samples);
    }

    // Map raw ADC → 0-100 %
    uint8_t readRSSIPercent(uint8_t samples = 8) {
        uint16_t raw = readRSSIRaw(samples);
        int32_t pct = ((int32_t)raw - _rMin) * 100 / ((int32_t)_rMax - _rMin);
        return (uint8_t)constrain(pct, 0, 100);
    }

private:
    uint8_t  _data, _clk, _cs, _rssi;
    uint16_t _rMin, _rMax;

    inline void _clkPulse() {
        digitalWrite(_clk, HIGH);
        delayMicroseconds(1);
        digitalWrite(_clk, LOW);
        delayMicroseconds(1);
    }

    // Write 20-bit value to Synth-B register (address 0x01)
    void _writeSynth(uint32_t synthVal) {
        // Compose 25-bit payload: addr(4) | W(1) | data(20)
        // Address 0x01 → bits[3:0] = 0001  (sent LSB-first)
        // W=0 (write)   → bit[4]   = 0
        // synthVal       → bits[24:5]
        uint32_t payload = 0x01u | ((synthVal & 0xFFFFFu) << 5);

        digitalWrite(_cs, LOW);
        delayMicroseconds(1);

        for (int i = 0; i < 25; i++) {
            digitalWrite(_data, (payload >> i) & 1u ? HIGH : LOW);
            _clkPulse();
        }

        digitalWrite(_data, LOW);
        delayMicroseconds(1);
        digitalWrite(_cs, HIGH);
        delayMicroseconds(1);
    }
};

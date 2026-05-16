#pragma once
// ============================================================
// ds18b20.h  –  DS18B20 1-Wire temperature sensor driver
//
// Supports:
//   • Multiple sensors on a single 1-Wire bus (up to 10)
//   • Parasite power mode (2-wire connection)
//   • 9 / 10 / 11 / 12-bit resolution (0.5 / 0.25 / 0.125 / 0.0625 °C)
//   • Non-blocking (start conversion, poll DONE, read later)
//   • CRC-8 Maxim validation on scratchpad
//
// Wiring:
//   DS18B20 DQ  → GPIO (configurable, default GPIO4)
//   4.7 kΩ pull-up between DQ and 3.3 V
//   (Parasite: VDD tied to GND; external power: VDD to 3.3V)
//
// Usage:
//   DS18B20Bus bus(4);          // DQ pin
//   bus.begin();
//   bus.requestAll();           // start async conversion
//   delay(750);                 // or poll bus.conversionDone()
//   for (uint8_t i=0; i<bus.count(); i++)
//       float t = bus.getTempC(i);
// ============================================================
#include <Arduino.h>

#define DS18B20_MAX_SENSORS   10
#define DS18B20_ROM_BYTES      8   // 64-bit ROM = 8 bytes
#define DS18B20_SCRATCH_BYTES  9

// Resolution → conversion time (ms)
#define DS18B20_RES_9   9
#define DS18B20_RES_10 10
#define DS18B20_RES_11 11
#define DS18B20_RES_12 12

static const uint16_t DS18B20_CONV_MS[] = { 94, 188, 375, 750 };  // 9-12 bit

// 1-Wire Commands
#define OW_CMD_SEARCH_ROM   0xF0
#define OW_CMD_READ_ROM     0x33
#define OW_CMD_MATCH_ROM    0x55
#define OW_CMD_SKIP_ROM     0xCC
#define OW_CMD_ALARM_SEARCH 0xEC
#define OW_CMD_CONVERT_T    0x44
#define OW_CMD_WRITE_SCRCH  0x4E
#define OW_CMD_READ_SCRCH   0xBE
#define OW_CMD_COPY_SCRCH   0x48
#define OW_CMD_RECALL_EE    0xB8
#define OW_CMD_READ_POWER   0xB4
#define DS18B20_FAMILY_CODE 0x28

// ── CRC-8 (Maxim) ────────────────────────────────────────────
static uint8_t _crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        uint8_t byte = data[i];
        for (uint8_t b = 0; b < 8; b++) {
            uint8_t mix = (crc ^ byte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            byte >>= 1;
        }
    }
    return crc;
}

// ── Low-level 1-Wire bit-bang ─────────────────────────────────
class OneWireBitBang {
public:
    OneWireBitBang(uint8_t pin) : _pin(pin) {}

    void begin() { pinMode(_pin, INPUT); }

    // Reset + presence detect. Returns true if device responds.
    bool reset() {
        noInterrupts();
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
        delayMicroseconds(480);
        pinMode(_pin, INPUT);
        delayMicroseconds(70);
        bool present = (digitalRead(_pin) == LOW);
        interrupts();
        delayMicroseconds(410);
        return present;
    }

    void writeBit(uint8_t bit) {
        noInterrupts();
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
        if (bit) {
            delayMicroseconds(10);
            pinMode(_pin, INPUT);
            delayMicroseconds(55);
        } else {
            delayMicroseconds(65);
            pinMode(_pin, INPUT);
            delayMicroseconds(5);
        }
        interrupts();
    }

    uint8_t readBit() {
        noInterrupts();
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, LOW);
        delayMicroseconds(3);
        pinMode(_pin, INPUT);
        delayMicroseconds(10);
        uint8_t bit = digitalRead(_pin);
        interrupts();
        delayMicroseconds(53);
        return bit;
    }

    void writeByte(uint8_t byte) {
        for (uint8_t i = 0; i < 8; i++) {
            writeBit(byte & 1);
            byte >>= 1;
        }
    }

    uint8_t readByte() {
        uint8_t r = 0;
        for (uint8_t i = 0; i < 8; i++) {
            if (readBit()) r |= (1 << i);
        }
        return r;
    }

    // Strong pull-up (parasite power during conversion)
    void pullHigh() {
        noInterrupts();
        pinMode(_pin, OUTPUT);
        digitalWrite(_pin, HIGH);
        interrupts();
    }

    void release() { pinMode(_pin, INPUT); }

private:
    uint8_t _pin;
};

// ── DS18B20 Sensor info ───────────────────────────────────────
struct DS18B20Sensor {
    uint8_t  rom[DS18B20_ROM_BYTES];   // 64-bit ROM address
    float    tempC;                    // last read temperature (°C)
    bool     valid;                    // last CRC OK
    char     name[16];                 // user label (e.g. "Sensor 1")
};

// ── Main Bus class ────────────────────────────────────────────
class DS18B20Bus {
public:
    DS18B20Sensor sensors[DS18B20_MAX_SENSORS];
    uint8_t       count  = 0;

    DS18B20Bus(uint8_t pin, uint8_t resolution = DS18B20_RES_12)
        : _ow(pin), _res(resolution), _parasitePwr(false) {}

    // Call once in setup()
    bool begin() {
        _ow.begin();
        if (!_ow.reset()) return false;
        _scanBus();
        _setAllResolution(_res);
        // Detect parasite power
        _ow.reset();
        _ow.writeByte(OW_CMD_SKIP_ROM);
        _ow.writeByte(OW_CMD_READ_POWER);
        _parasitePwr = (_ow.readBit() == 0);
        return (count > 0);
    }

    // Start temperature conversion on ALL sensors (non-blocking).
    // Wait at least conversionMs() ms, then call readAll().
    void requestAll() {
        _ow.reset();
        _ow.writeByte(OW_CMD_SKIP_ROM);
        _ow.writeByte(OW_CMD_CONVERT_T);
        if (_parasitePwr) _ow.pullHigh();
    }

    // Poll: true when conversion is done (line goes HIGH).
    // Only meaningful in external-power mode.
    bool conversionDone() {
        if (_parasitePwr) return false;   // can't poll
        return (_ow.readBit() == 1);
    }

    // Conversion time in ms for current resolution
    uint16_t conversionMs() const {
        return DS18B20_CONV_MS[_res - 9];
    }

    // Read scratchpad from all sensors (call after conversion)
    void readAll() {
        for (uint8_t i = 0; i < count; i++) {
            _readScratchpad(i);
        }
        if (_parasitePwr) _ow.release();
    }

    // Blocking one-shot: request + wait + read
    void readAllBlocking() {
        requestAll();
        uint32_t t0 = millis();
        while (millis() - t0 < conversionMs()) { yield(); }
        readAll();
    }

    // Get temperature in °C for sensor index i
    float getTempC(uint8_t i) const {
        if (i >= count) return -999.0f;
        return sensors[i].tempC;
    }

    // Get temperature in °F
    float getTempF(uint8_t i) const {
        if (i >= count) return -999.0f;
        return sensors[i].tempC * 1.8f + 32.0f;
    }

    // Set alarm thresholds (stored in EEPROM on sensor)
    void setAlarm(uint8_t i, int8_t lo, int8_t hi) {
        if (i >= count) return;
        _selectROM(i);
        _ow.writeByte(OW_CMD_WRITE_SCRCH);
        _ow.writeByte((uint8_t)hi);   // TH register
        _ow.writeByte((uint8_t)lo);   // TL register
        uint8_t cfg = 0x1F | ((_res - 9) << 5);
        _ow.writeByte(cfg);
    }

    // ROM address as hex string (for display)
    String romHex(uint8_t i) const {
        if (i >= count) return "";
        char buf[17];
        const uint8_t *r = sensors[i].rom;
        snprintf(buf, sizeof(buf),
                 "%02X%02X%02X%02X%02X%02X%02X%02X",
                 r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7]);
        return String(buf);
    }

    bool isParasitePower() const { return _parasitePwr; }

private:
    OneWireBitBang _ow;
    uint8_t        _res;
    bool           _parasitePwr;

    // ── ROM search (standard 1-Wire algorithm) ────────────────
    void _scanBus() {
        count = 0;
        uint8_t romBuf[8] = {};
        uint8_t lastDisc = 0, lastFamilyDisc = 0;
        bool    lastDev  = false;

        while (!lastDev && count < DS18B20_MAX_SENSORS) {
            if (!_ow.reset()) break;
            _ow.writeByte(OW_CMD_SEARCH_ROM);

            uint8_t idBit, cmpBit, dir;
            uint8_t lastZero = 0;

            for (uint8_t i = 1; i <= 64; i++) {
                idBit  = _ow.readBit();
                cmpBit = _ow.readBit();

                if (idBit && cmpBit) break;   // no devices

                if (idBit != cmpBit) {
                    dir = idBit;
                } else {
                    if (i < lastDisc)       dir = (romBuf[(i-1)/8] >> ((i-1)%8)) & 1;
                    else if (i == lastDisc) dir = 1;
                    else                    dir = 0;
                    if (!dir) lastZero = i;
                    if (lastZero < 9) lastFamilyDisc = lastZero;
                }

                if (dir) romBuf[(i-1)/8] |=  (1 << ((i-1)%8));
                else     romBuf[(i-1)/8] &= ~(1 << ((i-1)%8));
                _ow.writeBit(dir);
            }

            lastDisc = lastZero;
            if (!lastDisc) lastDev = true;

            // Only add DS18B20 family (0x28)
            if (romBuf[0] == DS18B20_FAMILY_CODE &&
                _crc8(romBuf, 7) == romBuf[7]) {
                memcpy(sensors[count].rom, romBuf, 8);
                sensors[count].tempC = -999.0f;
                sensors[count].valid = false;
                snprintf(sensors[count].name, 16, "Sensor %d", count+1);
                count++;
            }
        }
        (void)lastFamilyDisc;
    }

    void _selectROM(uint8_t i) {
        _ow.reset();
        _ow.writeByte(OW_CMD_MATCH_ROM);
        for (uint8_t b = 0; b < 8; b++) _ow.writeByte(sensors[i].rom[b]);
    }

    void _setAllResolution(uint8_t res) {
        uint8_t cfg = 0x1F | ((res - 9) << 5);
        for (uint8_t i = 0; i < count; i++) {
            _selectROM(i);
            _ow.writeByte(OW_CMD_WRITE_SCRCH);
            _ow.writeByte(0x4B);   // TH = 75°C
            _ow.writeByte(0x46);   // TL = 70°C
            _ow.writeByte(cfg);
        }
    }

    void _readScratchpad(uint8_t i) {
        _selectROM(i);
        _ow.writeByte(OW_CMD_READ_SCRCH);
        uint8_t sp[DS18B20_SCRATCH_BYTES];
        for (uint8_t b = 0; b < DS18B20_SCRATCH_BYTES; b++) sp[b] = _ow.readByte();

        sensors[i].valid = (_crc8(sp, 8) == sp[8]);
        if (!sensors[i].valid) return;

        int16_t raw = (int16_t)((sp[1] << 8) | sp[0]);
        // Mask unused LSBs based on resolution
        uint8_t ures = ((sp[4] >> 5) & 0x03);
        static const uint8_t mask[] = { 0xF8, 0xFC, 0xFE, 0xFF };
        raw &= mask[ures];
        sensors[i].tempC = raw / 16.0f;
    }
};

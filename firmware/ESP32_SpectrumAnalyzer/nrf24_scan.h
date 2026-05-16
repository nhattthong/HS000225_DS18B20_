#pragma once
// ============================================================
// nrf24_scan.h  –  nRF24L01+ scanner for 2.4 GHz band
//
// Wiring (ESP32-C3  FSPI / SPI2):
//   SCK   → GPIO6
//   MOSI  → GPIO7
//   MISO  → GPIO2
//   CS    → GPIO10
//   CE    → GPIO3
//
// Method:
//   Channels 0-127  →  2400-2527 MHz (1 MHz steps)
//   Each channel is tuned, a short dwell allows the LNA to
//   respond, then testRPD() (Received Power Detector) returns
//   1 if power > -64 dBm.
//
//   For a smoother display the scanner accumulates binary RPD
//   hits over SCAN_PASSES sweeps and presents a 0-100 % value.
//   An exponential moving average then smooths across scans.
//
// Required library:  RF24 by TMRh20 (≥1.4.6)
// ============================================================
#include <Arduino.h>
#include <SPI.h>
#include <RF24.h>

#define NRF24_NUM_CHANNELS 128
#define NRF24_SCAN_PASSES   4      // sweeps to accumulate per report
#define NRF24_DWELL_US    200      // µs dwell per channel per pass
#define NRF24_SMOOTH_K      6      // EMA weight: new = (old*K + raw) / (K+1)

class NRF24Scanner {
public:
    uint8_t rssi[NRF24_NUM_CHANNELS] = {};   // 0-100, smoothed

    NRF24Scanner(RF24 &radio) : _radio(radio) {}

    // Call once in setup() after radio.begin()
    void begin() {
        _radio.setAutoAck(false);
        _radio.disableCRC();
        _radio.setPayloadSize(2);
        _radio.setDataRate(RF24_2MBPS);   // wider detection window
        _radio.setPALevel(RF24_PA_MIN);
        _radio.startListening();
        _radio.stopListening();
    }

    // Run one full scan (blocking ~NRF24_SCAN_PASSES * 128 * DWELL_US ≈ 100 ms)
    // Calls yield() every 16 channels to keep WiFi stack alive.
    void scan() {
        uint8_t hits[NRF24_NUM_CHANNELS] = {};

        for (uint8_t pass = 0; pass < NRF24_SCAN_PASSES; pass++) {
            for (uint8_t ch = 0; ch < NRF24_NUM_CHANNELS; ch++) {
                _radio.setChannel(ch);
                _radio.startListening();
                delayMicroseconds(NRF24_DWELL_US);
                if (_radio.testRPD()) hits[ch]++;
                _radio.stopListening();

                if ((ch & 0x0F) == 0x0F) yield();  // every 16 ch
            }
        }

        // Convert hit count → 0-100 then apply EMA smoothing
        for (uint8_t ch = 0; ch < NRF24_NUM_CHANNELS; ch++) {
            uint8_t raw = (uint8_t)((uint16_t)hits[ch] * 100 / NRF24_SCAN_PASSES);
            rssi[ch] = (uint8_t)(((uint16_t)rssi[ch] * NRF24_SMOOTH_K + raw)
                                  / (NRF24_SMOOTH_K + 1));
        }
    }

    bool ready() { return _radio.isChipConnected(); }

private:
    RF24 &_radio;
};

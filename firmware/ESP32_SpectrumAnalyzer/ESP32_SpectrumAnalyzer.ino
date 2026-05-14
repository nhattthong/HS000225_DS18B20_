// ============================================================
// ESP32_SpectrumAnalyzer.ino
//
// RF Spectrum Analyzer for ESP32-C3
//   • 2.4 GHz band via nRF24L01+ (128 channels, 2400-2527 MHz)
//   • 5.8 GHz band via RX5808 FPV module (40 FPV channels)
//   • Real-time waterfall + spectrum display in browser
//   • WiFi: Access-Point mode  OR  Station mode (configurable)
//   • WebSocket data streaming + JSON command/response
//
// ── Required libraries (install via Library Manager) ────────
//   RF24           by TMRh20          (≥1.4.6)
//   ESPAsyncWebServer by lacamera / me-no-dev
//   AsyncTCP       by dvarrel / me-no-dev
//   ArduinoJson    by Benoit Blanchon  (v6.x)
//
// ── Pin map (ESP32-C3) ───────────────────────────────────────
//   nRF24L01+   (hardware FSPI / SPI2)
//     SCK   → GPIO 6
//     MOSI  → GPIO 7
//     MISO  → GPIO 2
//     CS    → GPIO 10
//     CE    → GPIO 3
//
//   RX5808      (bit-bang SPI + ADC)
//     DATA  → GPIO 4
//     CLK   → GPIO 5
//     CS    → GPIO 8
//     RSSI  → GPIO 1  (ADC1_CH1)
//     VCC   → 5 V    (module requires 5 V)
//     GND   → GND
//
//   Note: GPIO 18/19 are USB on ESP32-C3-DevKitM-1; avoid them.
// ============================================================
#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <SPI.h>
#include <RF24.h>

#include "rx5808.h"
#include "nrf24_scan.h"
#include "web_html.h"

// ── Pin definitions ──────────────────────────────────────────
#define PIN_SPI_SCK      6
#define PIN_SPI_MOSI     7
#define PIN_SPI_MISO     2
#define PIN_NRF24_CE     3
#define PIN_NRF24_CS    10

#define PIN_RX5808_DATA  4
#define PIN_RX5808_CLK   5
#define PIN_RX5808_CS    8
#define PIN_RX5808_RSSI  1   // ADC1_CH1

// ── Tuning constants ─────────────────────────────────────────
// Calibrate these by measuring your RX5808's RSSI pin with a
// known signal source and no signal.
#define RX5808_RSSI_MIN  200   // ADC count  ≈  no signal
#define RX5808_RSSI_MAX 2800   // ADC count  ≈  strong signal
#define RX5808_SETTLE_MS  25   // PLL settle time per channel (ms)

// ── WiFi defaults ────────────────────────────────────────────
#define AP_SSID_DEFAULT  "SpectrumAnalyzer"
#define AP_PASS_DEFAULT  "spectrum123"   // ≥8 chars; set "" for open

// ── Global objects ───────────────────────────────────────────
AsyncWebServer httpServer(80);
AsyncWebSocket wsServer("/ws");
Preferences    prefs;

SPIClass    spi2(FSPI);
RF24        radio(PIN_NRF24_CE, PIN_NRF24_CS);

RX5808      rx5808(PIN_RX5808_DATA, PIN_RX5808_CLK,
                   PIN_RX5808_CS,   PIN_RX5808_RSSI,
                   RX5808_RSSI_MIN, RX5808_RSSI_MAX);

NRF24Scanner nrfScanner(radio);

// ── Runtime state ─────────────────────────────────────────────
bool     isAPMode    = true;
String   apSSID      = AP_SSID_DEFAULT;
String   apPass      = AP_PASS_DEFAULT;
String   staSSID, staPass;
String   currentIP   = "192.168.4.1";

uint8_t  activeBand  = 0;    // 0=both  1=2.4GHz only  2=5.8GHz only

bool     nrf24OK     = false;
bool     rx5808OK    = true;  // RX5808 has no digital "ready" signal

// Scan result buffers (per-channel RSSI 0-100)
uint8_t  nrf24Rssi[NRF24_NUM_CHANNELS]   = {};
uint8_t  rx5808Rssi[RX5808_NUM_CHANNELS] = {};

// ── Helpers ───────────────────────────────────────────────────
void broadcastScan24() {
    if (wsServer.count() == 0) return;
    // Build compact JSON inline to avoid heap fragmentation
    String out;
    out.reserve(NRF24_NUM_CHANNELS * 4 + 20);
    out = F("{\"t\":\"s\",\"b\":0,\"d\":[");
    for (int i = 0; i < NRF24_NUM_CHANNELS; i++) {
        out += nrfScanner.rssi[i];
        if (i < NRF24_NUM_CHANNELS - 1) out += ',';
    }
    out += F("]}");
    wsServer.textAll(out);
}

void broadcastScan58() {
    if (wsServer.count() == 0) return;
    // Include frequencies so JS can map them correctly
    String out;
    out.reserve(RX5808_NUM_CHANNELS * 10 + 30);
    out = F("{\"t\":\"s\",\"b\":1,\"f\":[");
    for (int i = 0; i < RX5808_NUM_CHANNELS; i++) {
        // Send frequencies in SORTED order (by ascending freq)
        uint8_t idx = pgm_read_byte(&RX5808_SORT_IDX[i]);
        out += pgm_read_word(&RX5808_FREQ[idx]);
        if (i < RX5808_NUM_CHANNELS - 1) out += ',';
    }
    out += F("],\"d\":[");
    for (int i = 0; i < RX5808_NUM_CHANNELS; i++) {
        uint8_t idx = pgm_read_byte(&RX5808_SORT_IDX[i]);
        out += rx5808Rssi[idx];
        if (i < RX5808_NUM_CHANNELS - 1) out += ',';
    }
    out += F("]}");
    wsServer.textAll(out);
}

void sendStatus(AsyncWebSocketClient *client) {
    StaticJsonDocument<200> doc;
    doc[F("t")]  = F("st");
    doc[F("m")]  = isAPMode ? F("ap") : F("sta");
    doc[F("ip")] = currentIP;
    doc[F("ss")] = isAPMode ? apSSID : staSSID;
    String out;
    serializeJson(doc, out);
    if (client) client->text(out);
    else        wsServer.textAll(out);
}

void sendMsg(AsyncWebSocketClient *client, const char *msg, bool ok) {
    StaticJsonDocument<100> doc;
    doc[F("t")]  = F("msg");
    doc[F("m")]  = msg;
    doc[F("ok")] = ok;
    String out;
    serializeJson(doc, out);
    if (client) client->text(out);
    else        wsServer.textAll(out);
}

// ── WiFi management ───────────────────────────────────────────
void setupAP() {
    WiFi.disconnect(true);
    WiFi.mode(WIFI_AP);
    bool ok;
    if (apPass.length() >= 8)
        ok = WiFi.softAP(apSSID.c_str(), apPass.c_str());
    else
        ok = WiFi.softAP(apSSID.c_str());   // open AP

    isAPMode  = true;
    currentIP = WiFi.softAPIP().toString();
    Serial.printf("[WiFi] AP  SSID=%s  IP=%s  ok=%d\n",
                  apSSID.c_str(), currentIP.c_str(), ok);
    sendStatus(nullptr);
}

void connectSTA() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(staSSID.c_str(), staPass.c_str());

    Serial.printf("[WiFi] Connecting to %s …\n", staSSID.c_str());

    // Wait up to 10 s (non-blocking: use yield so WebSocket stays alive)
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
        yield();
        delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
        isAPMode  = false;
        currentIP = WiFi.localIP().toString();
        Serial.printf("[WiFi] Connected  IP=%s\n", currentIP.c_str());
        sendStatus(nullptr);
        sendMsg(nullptr, "Connected!", true);
    } else {
        Serial.println("[WiFi] STA connect failed – reverting to AP");
        sendMsg(nullptr, "Connection failed – reverting to AP", false);
        setupAP();
    }
}

// ── WebSocket event handler ───────────────────────────────────
void onWSEvent(AsyncWebSocket *server,
               AsyncWebSocketClient *client,
               AwsEventType type, void *arg,
               uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT) {
        Serial.printf("[WS] Client #%u connected\n", client->id());
        sendStatus(client);

    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] Client #%u disconnected\n", client->id());

    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        // Only handle complete, single-frame text messages
        if (info->opcode != WS_TEXT || !info->final || info->index != 0) return;

        // Null-terminate and parse
        data[len] = '\0';
        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, (char *)data) != DeserializationError::Ok) return;

        const char *cmd = doc[F("c")] | "";

        if (strcmp(cmd, "status") == 0) {
            sendStatus(client);

        } else if (strcmp(cmd, "band") == 0) {
            activeBand = (uint8_t)(doc[F("b")] | 0);

        } else if (strcmp(cmd, "scan") == 0) {
            // WiFi network scan (runs on calling task)
            int n = WiFi.scanNetworks(false, false);
            DynamicJsonDocument res(2048);
            res[F("t")] = F("w");
            JsonArray arr = res.createNestedArray(F("n"));
            for (int i = 0; i < n && i < 20; i++) {
                JsonObject net = arr.createNestedObject();
                net[F("s")] = WiFi.SSID(i);
                net[F("r")] = WiFi.RSSI(i);
                net[F("e")] = (WiFi.encryptionType(i) != WIFI_AUTH_OPEN);
            }
            WiFi.scanDelete();
            String out;
            serializeJson(res, out);
            client->text(out);

        } else if (strcmp(cmd, "ap") == 0) {
            apSSID = doc[F("s")] | AP_SSID_DEFAULT;
            apPass = doc[F("p")] | "";
            prefs.putString("apSSID", apSSID);
            prefs.putString("apPass", apPass);
            sendMsg(client, "AP settings saved – switching…", true);
            // Small delay so the message reaches the client first
            delay(400);
            setupAP();

        } else if (strcmp(cmd, "sta") == 0) {
            const char *s = doc[F("s")] | "";
            const char *p = doc[F("p")] | "";
            if (!s[0]) { sendMsg(client, "SSID required", false); return; }
            staSSID = s;
            staPass = p;
            prefs.putString("staSSID", staSSID);
            prefs.putString("staPass", staPass);
            sendMsg(client, "Connecting…", true);
            delay(200);
            connectSTA();
        }
    }

    // Cleanup disconnected clients to free memory
    server->cleanupClients();
}

// ── Main scan loop ─────────────────────────────────────────────
void runScan() {
    // 2.4 GHz scan (nRF24)
    if ((activeBand == 0 || activeBand == 1) && nrf24OK) {
        nrfScanner.scan();       // ~100 ms, yields internally
        broadcastScan24();
    }

    // 5.8 GHz scan (RX5808)
    if (activeBand == 0 || activeBand == 2) {
        for (uint8_t ch = 0; ch < RX5808_NUM_CHANNELS; ch++) {
            rx5808.setChannel(ch);

            // Wait for PLL to settle while yielding to WiFi stack
            uint32_t t0 = millis();
            while (millis() - t0 < RX5808_SETTLE_MS) { yield(); }

            rx5808Rssi[ch] = rx5808.readRSSIPercent(8);
            yield();
        }
        broadcastScan58();
    }
}

// ── Setup ──────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println(F("\n[Boot] RF Spectrum Analyzer"));

    // Load saved WiFi credentials
    prefs.begin("spectrum", false);
    apSSID  = prefs.getString("apSSID",  AP_SSID_DEFAULT);
    apPass  = prefs.getString("apPass",  AP_PASS_DEFAULT);
    staSSID = prefs.getString("staSSID", "");
    staPass = prefs.getString("staPass", "");
    bool preferSTA = prefs.getBool("preferSTA", false);

    // ── nRF24L01+ init ─────────────────────────────────────────
    spi2.begin(PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI, PIN_NRF24_CS);
    if (radio.begin(&spi2)) {
        nrfScanner.begin();
        nrf24OK = true;
        Serial.println(F("[nRF24] OK"));
    } else {
        Serial.println(F("[nRF24] NOT detected – 2.4GHz scan disabled"));
    }

    // ── RX5808 init ────────────────────────────────────────────
    analogReadResolution(12);   // ESP32-C3: 12-bit ADC
    analogSetAttenuation(ADC_11db);   // full-scale ~3.3 V
    rx5808.begin();
    Serial.println(F("[RX5808] OK"));

    // ── WiFi ───────────────────────────────────────────────────
    if (preferSTA && staSSID.length() > 0) {
        connectSTA();   // try saved STA creds
    } else {
        setupAP();
    }

    // ── HTTP server ────────────────────────────────────────────
    httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send_P(200, "text/html", INDEX_HTML);
    });

    // ── WebSocket ──────────────────────────────────────────────
    wsServer.onEvent(onWSEvent);
    httpServer.addHandler(&wsServer);
    httpServer.begin();
    Serial.println(F("[HTTP] Server started"));
}

// ── Loop ───────────────────────────────────────────────────────
void loop() {
    wsServer.cleanupClients();   // free stale connections
    runScan();                   // one full sweep (2.4+5.8)
}

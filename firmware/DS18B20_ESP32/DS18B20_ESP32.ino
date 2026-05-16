// ============================================================
// DS18B20_ESP32.ino
//
// DS18B20 1-Wire Temperature Monitor for ESP32
//   • Up to 10 DS18B20 sensors on a single GPIO
//   • Real-time web UI via WebSocket (JSON)
//   • Configurable read interval, resolution, alarm thresholds
//   • WiFi: Access-Point mode OR Station mode (NVS persistent)
//   • Per-sensor min / max / rolling-average statistics
//
// ── Required libraries (install via Arduino Library Manager) ─
//   ESPAsyncWebServer  by lacamera / me-no-dev
//   AsyncTCP           by dvarrel  / me-no-dev
//   ArduinoJson        by Benoit Blanchon (v6.x)
//   (DS18B20 driver is built-in: ds18b20.h — no extra lib needed)
//
// ── Pin map ──────────────────────────────────────────────────
//   DS18B20 DQ  → GPIO 4   (4.7 kΩ pull-up to 3.3 V)
//   Optional LED → GPIO 2  (blinks on read)
//
// ── WiFi defaults ────────────────────────────────────────────
//   AP SSID : DS18B20_Monitor
//   AP Pass : monitor123
//   URL     : http://192.168.4.1
// ============================================================

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <Preferences.h>

#include "ds18b20.h"
#include "web_html.h"

// ── Pin definitions ──────────────────────────────────────────
#define PIN_ONE_WIRE    4     // DS18B20 data line (DQ)
#define PIN_LED         2     // On-board LED (active HIGH on most boards)

// ── Sensor config ─────────────────────────────────────────────
#define DS18B20_RESOLUTION  DS18B20_RES_12    // 12-bit = 0.0625 °C
#define DEFAULT_INTERVAL_S  5                 // read period (seconds)
#define STATS_WINDOW        60                // samples for rolling avg

// ── WiFi defaults ─────────────────────────────────────────────
#define AP_SSID_DEFAULT   "DS18B20_Monitor"
#define AP_PASS_DEFAULT   "monitor123"        // ≥8 chars; "" for open

// ── Global objects ────────────────────────────────────────────
AsyncWebServer httpServer(80);
AsyncWebSocket wsServer("/ws");
Preferences    prefs;
DS18B20Bus     bus(PIN_ONE_WIRE, DS18B20_RESOLUTION);

// ── Runtime state ─────────────────────────────────────────────
bool     isAPMode   = true;
String   apSSID     = AP_SSID_DEFAULT;
String   apPass     = AP_PASS_DEFAULT;
String   staSSID, staPass;
String   currentIP  = "192.168.4.1";

uint32_t intervalMs = DEFAULT_INTERVAL_S * 1000UL;
uint32_t lastRead   = 0;

// Per-sensor statistics (rolling window)
struct SensorStats {
    float sumC    = 0;
    float minC    = 1e9f;
    float maxC    = -1e9f;
    uint32_t cnt  = 0;
    // Alarm thresholds (°C, integer – DS18B20 TH/TL are 8-bit signed)
    int8_t   alarmLo = -10;
    int8_t   alarmHi = 85;
    bool     inAlarm = false;
};
SensorStats stats[DS18B20_MAX_SENSORS];

// ── JSON broadcast helpers ────────────────────────────────────
void broadcastData() {
    if (wsServer.count() == 0) return;

    // Estimate buffer: 200 bytes per sensor + 60 overhead
    const size_t CAP = 200 * DS18B20_MAX_SENSORS + 128;
    DynamicJsonDocument doc(CAP);

    doc[F("t")] = F("data");
    doc[F("power")]    = bus.isParasitePower() ? F("par") : F("ext");
    doc[F("res")]      = DS18B20_RESOLUTION;
    doc[F("interval")] = intervalMs / 1000;

    JsonArray arr = doc.createNestedArray(F("sensors"));
    for (uint8_t i = 0; i < bus.count; i++) {
        const DS18B20Sensor &s  = bus.sensors[i];
        SensorStats         &st = stats[i];

        float t = s.tempC;
        bool  alarm = (t >= st.alarmHi || t <= st.alarmLo);
        st.inAlarm = alarm;

        JsonObject o = arr.createNestedObject();
        o[F("id")]   = i;
        o[F("name")] = s.name;
        o[F("rom")]  = bus.romHex(i);
        o[F("tempC")] = serialized(String(t, 2));
        o[F("tempF")] = serialized(String(bus.getTempF(i), 2));
        o[F("min")]  = serialized(st.cnt ? String(st.minC, 2) : String("null"));
        o[F("max")]  = serialized(st.cnt ? String(st.maxC, 2) : String("null"));
        o[F("avg")]  = serialized(st.cnt ? String(st.sumC / st.cnt, 2) : String("null"));
        o[F("lo")]   = st.alarmLo;
        o[F("hi")]   = st.alarmHi;
        o[F("alarm")] = alarm;
    }

    String out;
    serializeJson(doc, out);
    wsServer.textAll(out);
}

void broadcastAlarm(uint8_t i) {
    if (wsServer.count() == 0) return;
    StaticJsonDocument<200> doc;
    doc[F("t")]    = F("alarm");
    doc[F("id")]   = i;
    doc[F("name")] = bus.sensors[i].name;
    doc[F("tempC")] = bus.sensors[i].tempC;
    doc[F("lo")]   = stats[i].alarmLo;
    doc[F("hi")]   = stats[i].alarmHi;
    String out;
    serializeJson(doc, out);
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
    StaticJsonDocument<128> doc;
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
    bool ok = apPass.length() >= 8
        ? WiFi.softAP(apSSID.c_str(), apPass.c_str())
        : WiFi.softAP(apSSID.c_str());
    isAPMode  = true;
    currentIP = WiFi.softAPIP().toString();
    Serial.printf("[WiFi] AP SSID=%s IP=%s ok=%d\n",
                  apSSID.c_str(), currentIP.c_str(), ok);
    sendStatus(nullptr);
}

void connectSTA() {
    WiFi.mode(WIFI_STA);
    WiFi.begin(staSSID.c_str(), staPass.c_str());
    Serial.printf("[WiFi] Connecting to %s …\n", staSSID.c_str());

    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
        yield(); delay(200);
    }

    if (WiFi.status() == WL_CONNECTED) {
        isAPMode  = false;
        currentIP = WiFi.localIP().toString();
        prefs.putBool("preferSTA", true);
        Serial.printf("[WiFi] Connected IP=%s\n", currentIP.c_str());
        sendStatus(nullptr);
        sendMsg(nullptr, "Connected!", true);
    } else {
        prefs.putBool("preferSTA", false);
        Serial.println("[WiFi] Connect failed – reverting to AP");
        sendMsg(nullptr, "Connection failed – reverting to AP", false);
        setupAP();
    }
}

// ── Sensor read + stats update ────────────────────────────────
void doSensorRead() {
    digitalWrite(PIN_LED, HIGH);
    bus.readAllBlocking();
    digitalWrite(PIN_LED, LOW);

    for (uint8_t i = 0; i < bus.count; i++) {
        if (!bus.sensors[i].valid) continue;
        float t = bus.sensors[i].tempC;
        SensorStats &st = stats[i];
        if (t < st.minC) st.minC = t;
        if (t > st.maxC) st.maxC = t;
        st.sumC += t;
        st.cnt++;
        // Cap to avoid overflow (keep last STATS_WINDOW samples' sum)
        if (st.cnt > STATS_WINDOW) {
            // approximate: drop oldest contribution
            st.sumC -= st.sumC / st.cnt;
            st.cnt = STATS_WINDOW;
        }
        // Alarm check
        if (t >= st.alarmHi || t <= st.alarmLo) {
            broadcastAlarm(i);
            Serial.printf("[ALARM] Sensor %d '%s' = %.2f°C (Lo=%d Hi=%d)\n",
                          i, bus.sensors[i].name, t, st.alarmLo, st.alarmHi);
        }
    }

    broadcastData();

    Serial.printf("[READ]");
    for (uint8_t i = 0; i < bus.count; i++) {
        Serial.printf(" %s=%.2f°C", bus.sensors[i].name, bus.sensors[i].tempC);
    }
    Serial.println();
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
        broadcastData();   // send current readings immediately

    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("[WS] Client #%u disconnected\n", client->id());

    } else if (type == WS_EVT_DATA) {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;
        if (info->opcode != WS_TEXT || !info->final || info->index != 0) return;

        // Copy to null-terminated stack buffer
        char buf[257];
        size_t cl = (len < sizeof(buf) - 1) ? len : sizeof(buf) - 1;
        memcpy(buf, data, cl);
        buf[cl] = '\0';

        StaticJsonDocument<256> doc;
        if (deserializeJson(doc, buf) != DeserializationError::Ok) return;

        const char *cmd = doc[F("c")] | "";

        if (strcmp(cmd, "status") == 0) {
            sendStatus(client);
            broadcastData();

        } else if (strcmp(cmd, "setAlarm") == 0) {
            uint8_t id = (uint8_t)(doc[F("id")] | 0);
            if (id < bus.count) {
                stats[id].alarmLo = (int8_t)(doc[F("lo")] | -10);
                stats[id].alarmHi = (int8_t)(doc[F("hi")] | 85);
                bus.setAlarm(id, stats[id].alarmLo, stats[id].alarmHi);
                // Persist per-sensor alarm
                char key[12];
                snprintf(key, sizeof(key), "alo%d", id);
                prefs.putChar(key, stats[id].alarmLo);
                snprintf(key, sizeof(key), "ahi%d", id);
                prefs.putChar(key, stats[id].alarmHi);
                sendMsg(client, "Alarm saved", true);
            }

        } else if (strcmp(cmd, "setInterval") == 0) {
            uint32_t v = (uint32_t)(doc[F("v")] | DEFAULT_INTERVAL_S);
            intervalMs = constrain(v, 1UL, 3600UL) * 1000UL;
            prefs.putUInt("interval", intervalMs / 1000);
            sendMsg(client, "Interval updated", true);

        } else if (strcmp(cmd, "scan") == 0) {
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
            String out; serializeJson(res, out);
            client->text(out);

        } else if (strcmp(cmd, "ap") == 0) {
            apSSID = doc[F("s")] | AP_SSID_DEFAULT;
            apPass = doc[F("p")] | "";
            prefs.putString("apSSID", apSSID);
            prefs.putString("apPass", apPass);
            sendMsg(client, "AP settings saved – switching…", true);
            delay(400);
            setupAP();

        } else if (strcmp(cmd, "sta") == 0) {
            const char *s = doc[F("s")] | "";
            const char *p = doc[F("p")] | "";
            if (!s[0]) { sendMsg(client, "SSID required", false); return; }
            staSSID = s; staPass = p;
            prefs.putString("staSSID", staSSID);
            prefs.putString("staPass", staPass);
            sendMsg(client, "Connecting…", true);
            delay(200);
            connectSTA();
        }
    }

    server->cleanupClients();
}

// ── Setup ─────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println(F("\n[Boot] DS18B20 Temperature Monitor"));

    pinMode(PIN_LED, OUTPUT);
    digitalWrite(PIN_LED, LOW);

    // Load NVS settings
    prefs.begin("ds18b20", false);
    apSSID  = prefs.getString("apSSID",  AP_SSID_DEFAULT);
    apPass  = prefs.getString("apPass",  AP_PASS_DEFAULT);
    staSSID = prefs.getString("staSSID", "");
    staPass = prefs.getString("staPass", "");
    bool preferSTA = prefs.getBool("preferSTA", false);
    intervalMs = prefs.getUInt("interval", DEFAULT_INTERVAL_S) * 1000UL;

    // ── DS18B20 init ───────────────────────────────────────────
    Serial.printf("[DS18B20] Scanning 1-Wire bus (GPIO%d) …\n", PIN_ONE_WIRE);
    if (bus.begin()) {
        Serial.printf("[DS18B20] Found %d sensor(s)  parasite=%d  res=%dbit  conv=%dms\n",
                      bus.count, bus.isParasitePower(), DS18B20_RESOLUTION, bus.conversionMs());
        // Restore saved alarm thresholds
        for (uint8_t i = 0; i < bus.count; i++) {
            char key[12];
            snprintf(key, sizeof(key), "alo%d", i);
            stats[i].alarmLo = prefs.getChar(key, -10);
            snprintf(key, sizeof(key), "ahi%d", i);
            stats[i].alarmHi = prefs.getChar(key, 85);
        }
    } else {
        Serial.println(F("[DS18B20] No sensors detected – check wiring!"));
    }

    // ── WiFi ───────────────────────────────────────────────────
    if (preferSTA && staSSID.length() > 0) {
        connectSTA();
    } else {
        setupAP();
    }

    // ── HTTP ────────────────────────────────────────────────────
    httpServer.on("/", HTTP_GET, [](AsyncWebServerRequest *req) {
        req->send_P(200, "text/html", INDEX_HTML);
    });

    // ── WebSocket ───────────────────────────────────────────────
    wsServer.onEvent(onWSEvent);
    httpServer.addHandler(&wsServer);
    httpServer.begin();
    Serial.println(F("[HTTP] Server started"));

    // First read immediately
    if (bus.count > 0) {
        doSensorRead();
        lastRead = millis();
    }
}

// ── Loop ──────────────────────────────────────────────────────
void loop() {
    wsServer.cleanupClients();

    if (bus.count > 0 && millis() - lastRead >= intervalMs) {
        doSensorRead();
        lastRead = millis();
    }
}

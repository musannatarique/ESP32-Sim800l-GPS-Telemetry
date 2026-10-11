// GPS tracker: TTGO T-Call V1.3 (ESP32 + SIM800) + Neo-6M GPS -> HiveMQ Cloud over MQTT/TLS.
// Link priority: WiFi first, GPRS fallback.
// Setup: copy include/secrets.example.h to include/secrets.h and fill it in.
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <TinyGsmClient.h>   // TINY_GSM_MODEM_SIM800 comes from build_flags in platformio.ini
#include <TinyGPS++.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_task_wdt.h>
#include "secrets.h"

/* ---------------- Hardware pins (TTGO T-Call V1.3 / AM-036) ---------------- */
#define MODEM_RST        5
#define MODEM_PWKEY      4
#define MODEM_POWER_ON   23
#define MODEM_TX         27
#define MODEM_RX         26
#define GPS_RX_PIN       2
#define GPS_TX_PIN       25

/* ---------------- Tunables ---------------- */
constexpr uint32_t PUBLISH_MS      = 5000;
constexpr uint32_t GPS_FRESH_MS    = 5000;    // a fix older than this is treated as lost
constexpr uint32_t GPRS_RETRY_MS   = 20000;
constexpr uint32_t WDT_SECONDS     = 120;     // must exceed the longest blocking call (GPRS attach)

HardwareSerial SerialAT(1);
HardwareSerial SerialGPS(2);
TinyGsm modem(SerialAT);
TinyGsmClientSecure gsmClient(modem);   // TLS over GPRS (SIM800 TLS support is limited, see README)
WiFiClientSecure wifiClient;
PubSubClient mqtt;
TinyGPSPlus gps;

enum class Link { NONE, WIFI, GPRS };
Link activeLink = Link::NONE;
bool modemReady = false;

const String topicLocation = String("gps/devices/") + DEVICE_ID + "/location";
const String topicStatus   = String("gps/devices/") + DEVICE_ID + "/status";

/* ---------------- Helpers ---------------- */
void pumpGps() {
    while (SerialGPS.available() > 0) gps.encode(SerialGPS.read());
}

// Delay that keeps feeding the GPS parser and the watchdog
void waitMs(uint32_t ms) {
    uint32_t start = millis();
    while (millis() - start < ms) {
        pumpGps();
        esp_task_wdt_reset();
        delay(5);
    }
}

/* ---------------- Modem / GPRS ---------------- */
void powerOnModem() {
    pinMode(MODEM_RST, OUTPUT);
    digitalWrite(MODEM_RST, HIGH);
    pinMode(MODEM_POWER_ON, OUTPUT);
    digitalWrite(MODEM_POWER_ON, HIGH);
    pinMode(MODEM_PWKEY, OUTPUT);
    digitalWrite(MODEM_PWKEY, HIGH); delay(100);
    digitalWrite(MODEM_PWKEY, LOW);  delay(1000);
    digitalWrite(MODEM_PWKEY, HIGH);

    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    waitMs(3000);
    modemReady = modem.restart();
    Serial.printf("Modem: %s\n", modemReady ? "ready" : "NOT responding");
}

bool connectGprs() {
    if (!modemReady) { modemReady = modem.restart(); if (!modemReady) return false; }
    if (!modem.isNetworkConnected() && !modem.waitForNetwork(30000L)) {
        Serial.println("GPRS: no cellular network");
        return false;
    }
    bool ok = modem.gprsConnect(GPRS_APN, GPRS_USER, GPRS_PASS);
    Serial.printf("GPRS: %s\n", ok ? "connected" : "failed");
    return ok;
}

/* ---------------- Link selection ---------------- */
Link pickLink() {
    if (WiFi.status() == WL_CONNECTED) return Link::WIFI;
    if (modemReady && modem.isGprsConnected()) return Link::GPRS;
    return Link::NONE;
}

void maintainLink() {
    static uint32_t lastGprsTry = 0;

    // WiFi reconnects on its own (setAutoReconnect). GPRS is only brought up while WiFi is down.
    if (WiFi.status() != WL_CONNECTED && !(modemReady && modem.isGprsConnected())
        && millis() - lastGprsTry > GPRS_RETRY_MS) {
        lastGprsTry = millis();
        connectGprs();
    }

    Link next = pickLink();
    if (next != activeLink) {
        Serial.printf("Link: %d -> %d\n", (int)activeLink, (int)next);
        mqtt.disconnect();
        if (next == Link::WIFI)      mqtt.setClient(wifiClient);
        else if (next == Link::GPRS) mqtt.setClient(gsmClient);
        activeLink = next;
    }
}

/* ---------------- MQTT ---------------- */
void maintainMqtt() {
    static uint32_t lastTry = 0;
    static uint32_t backoff = 5000;

    if (activeLink == Link::NONE) return;
    if (mqtt.connected()) { mqtt.loop(); return; }
    if (millis() - lastTry < backoff) return;
    lastTry = millis();

    // Last Will: the broker publishes "offline" if the tracker drops without saying goodbye
    bool ok = mqtt.connect(DEVICE_ID, MQTT_USER, MQTT_PASS,
                           topicStatus.c_str(), 1, true, "{\"online\":false}");
    if (ok) {
        Serial.println("MQTT connected");
        mqtt.publish(topicStatus.c_str(), "{\"online\":true}", true);
        backoff = 5000;
    } else {
        Serial.printf("MQTT failed, rc=%d\n", mqtt.state());
        backoff = min<uint32_t>(backoff * 2, 60000);   // 5s, 10s, 20s, 40s, 60s
    }
}

void publishTelemetry() {
    if (!mqtt.connected()) return;

    StaticJsonDocument<256> doc;
    char latBuf[16], lonBuf[16];

    // Only send coordinates when the fix is valid AND recent; the dashboard keeps the last
    // known position when lat/lon are absent, instead of jumping to 0,0
    bool fix = gps.location.isValid() && gps.location.age() < GPS_FRESH_MS;
    doc["fix"] = fix;
    if (fix) {
        dtostrf(gps.location.lat(), 1, 6, latBuf);
        dtostrf(gps.location.lng(), 1, 6, lonBuf);
        doc["lat"] = serialized(latBuf);   // full 6-decimal precision
        doc["lon"] = serialized(lonBuf);
        if (gps.speed.isValid()) doc["speed_kmph"] = serialized(String(gps.speed.kmph(), 1));
    }
    if (gps.satellites.isValid()) doc["sats"] = gps.satellites.value();
    if (gps.hdop.isValid())       doc["hdop"] = serialized(String(gps.hdop.hdop(), 1));

    doc["net"]      = (activeLink == Link::WIFI) ? "WiFi" : "GPRS";
    doc["uptime_s"] = millis() / 1000;

    char payload[256];
    size_t n = serializeJson(doc, payload, sizeof(payload));
    if (!mqtt.publish(topicLocation.c_str(), (const uint8_t*)payload, n, false)) {
        Serial.println("Publish failed");
    }
}

/* ---------------- Arduino entry points ---------------- */
void setup() {
    Serial.begin(115200);
    SerialGPS.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

    powerOnModem();

    // WiFiManager is bounded so a tracker with no known WiFi still reaches GPRS
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFiManager wm;
    wm.setConnectTimeout(15);
    wm.setConfigPortalTimeout(120);
    if (!wm.autoConnect("TRACKER-SETUP-AP")) Serial.println("WiFi not configured, using GPRS");

#ifdef MQTT_ROOT_CA
    wifiClient.setCACert(MQTT_ROOT_CA);   // verified TLS
#else
    wifiClient.setInsecure();             // encrypted but the broker is not authenticated
#endif
    wifiClient.setTimeout(8);

    mqtt.setServer(MQTT_HOST, MQTT_PORT);
    mqtt.setBufferSize(512);
    mqtt.setKeepAlive(30);
    mqtt.setSocketTimeout(8);

    // Armed last, so the config portal above cannot trip it
    esp_task_wdt_init(WDT_SECONDS, true);
    esp_task_wdt_add(NULL);
}

void loop() {
    esp_task_wdt_reset();
    pumpGps();
    maintainLink();
    maintainMqtt();

    static uint32_t lastPub = 0;
    if (millis() - lastPub >= PUBLISH_MS) {
        lastPub = millis();
        publishTelemetry();
    }
}

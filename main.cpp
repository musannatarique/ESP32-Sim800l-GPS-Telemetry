#define TINY_GSM_MODEM_SIM800
#include <Arduino.h>
#include <TinyGsmClient.h>
#include <TinyGPS++.h>
#include <WiFiManager.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

/* --- HARDWARE PINS (TTGO T-Call V1.3 / AM-036 Configuration) --- */
#define MODEM_RST            5
#define MODEM_PWKEY          4
#define MODEM_POWER_ON       23
#define MODEM_TX             27
#define MODEM_RX             26
#define GPS_RX_PIN           2
#define GPS_TX_PIN           25
#define BAT_ADC_PIN          34 

/* --- MQTT & CLOUD CONFIGURATION (SANITIZED FOR PUBLIC REPO) --- */
const char apn[]        = "your_carrier_apn"; // Change
const char* mqtt_server = "YOUR_HIVEMQ_CLUSTER_ID.s1.eu.hivemq.cloud"; // Change
const int mqtt_port     = 8883; //
const char* mqtt_user   = "YOUR_MQTT_USERNAME"; // Change
const char* mqtt_pass   = "YOUR_MQTT_PASSWORD";   // Change 

HardwareSerial SerialAT(1);
HardwareSerial SerialGPS(2);
TinyGsm modem(SerialAT);
TinyGPSPlus gps;
WiFiClientSecure secureClient;
PubSubClient client(secureClient);

// Device Tracking Topics
String deviceId = "YOUR_DEVICE_ID"; // Change
String topic    = "gps/devices/" + deviceId + "/location";

/* --- POWER & BATTERY LOGIC --- */
float getBatteryVoltage() {
    return (analogRead(BAT_ADC_PIN) / 4095.0) * 3.3 * 2.1; 
}

int getBatteryPercentage(float voltage) {
    if (voltage >= 4.15) return 100;
    if (voltage >= 4.0)  return map(voltage * 100, 400, 415, 80, 100);
    if (voltage >= 3.8)  return map(voltage * 100, 380, 400, 50, 80);
    if (voltage >= 3.5)  return map(voltage * 100, 350, 380, 10, 50);
    if (voltage >= 3.3)  return map(voltage * 100, 330, 350, 0, 10);
    return 0;
}

void setupModem() {
    pinMode(MODEM_POWER_ON, OUTPUT);
    digitalWrite(MODEM_POWER_ON, HIGH);
    pinMode(MODEM_PWKEY, OUTPUT);
    digitalWrite(MODEM_PWKEY, HIGH); delay(100);
    digitalWrite(MODEM_PWKEY, LOW);  delay(1000);
    digitalWrite(MODEM_PWKEY, HIGH);

    SerialAT.begin(115200, SERIAL_8N1, MODEM_RX, MODEM_TX);
    SerialGPS.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    
    delay(3000);
    modem.restart();
    modem.gprsConnect(apn, "", "");
}

void setup() {
    Serial.begin(115200);
    setupModem();
    WiFiManager wm;
    wm.autoConnect("TRACKER-SETUP-AP"); 
    secureClient.setInsecure(); 
    client.setServer(mqtt_server, mqtt_port);
}

void loop() {
    while (SerialGPS.available() > 0) gps.encode(SerialGPS.read());

    if (!client.connected()) {
        if (client.connect(deviceId.c_str(), mqtt_user, mqtt_pass)) {
            Serial.println("MQTT Connection Live");
        }
    }
    client.loop();

    static unsigned long lastPub = 0;
    if (millis() - lastPub >= 5000) { 
        lastPub = millis();

        float vBat = getBatteryVoltage();
        int bPct = 0;
        String pwrStat;
        bool hasBat = true;

        if (vBat < 2.5) { 
            hasBat = false;
            pwrStat = "Connected via USB"; 
        } else {
            bPct = getBatteryPercentage(vBat);
            pwrStat = (vBat > 4.05) ? "Charging" : "On Battery";
        }

        String netType = (WiFi.status() == WL_CONNECTED) ? "WiFi" : "GPRS";

        String payload = "{";
        payload += "\"lat\":" + String(gps.location.isValid() ? gps.location.lat() : 0, 6) + ",";
        payload += "\"lon\":" + String(gps.location.isValid() ? gps.location.lng() : 0, 6) + ",";
        payload += "\"net\":\"" + netType + "\",";
        payload += "\"volt\":" + String(vBat, 2) + ",";
        payload += "\"batt_pct\":" + String(bPct) + ",";
        payload += "\"bat_stat\":\"" + pwrStat + "\",";
        payload += "\"has_bat\":" + String(hasBat ? "true" : "false");
        payload += "}";

        client.publish(topic.c_str(), payload.c_str());
    }
}

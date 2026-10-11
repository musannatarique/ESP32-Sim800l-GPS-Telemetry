// Copy this file to include/secrets.h and fill it in.
// Add "include/secrets.h" to .gitignore so real credentials never reach the repo.
#pragma once

#define DEVICE_ID   "YOUR_DEVICE_ID"

#define GPRS_APN    "your_carrier_apn"
#define GPRS_USER   ""
#define GPRS_PASS   ""

#define MQTT_HOST   "YOUR_HIVEMQ_CLUSTER_ID.s1.eu.hivemq.cloud"
#define MQTT_PORT   8883
#define MQTT_USER   "YOUR_DEVICE_MQTT_USERNAME"   // publish-only credential for this device
#define MQTT_PASS   "YOUR_DEVICE_MQTT_PASSWORD"

// Optional but recommended: paste the ISRG Root X1 PEM (HiveMQ Cloud uses Let's Encrypt)
// from https://letsencrypt.org/certs/isrgrootx1.pem to verify the broker over WiFi.
// If left undefined, the WiFi link falls back to unverified TLS.
// #define MQTT_ROOT_CA \
// "-----BEGIN CERTIFICATE-----\n" \
// "...\n" \
// "-----END CERTIFICATE-----\n"

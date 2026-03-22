#include <Arduino.h>
#include <WiFi.h>
#include <PicoMQTT.h>

PicoMQTT::Server mqtt;

const char* ssid = "YOUR_VAN_ROUTER_SSID";
const char* password = "YOUR_VAN_ROUTER_PASSWORD";

void setup() {
    Serial.begin(115200);
    Serial.println("\n--- MQTT Broker ---");

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] Broker IP: ");
    Serial.println(WiFi.localIP());

    mqtt.begin();
    Serial.println("[MQTT] Broker Started on port 1883. Ready for any payload.");
}

void loop() {
    mqtt.loop();
}

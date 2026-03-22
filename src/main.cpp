#include <Arduino.h>
#include <WiFi.h>
#include <PicoMQTT.h>
#include <WebServer.h>
#include "secrets.h"

#define MAX_TOPIC_CACHE 20

// Fixed-size structure to avoid dynamic allocation
struct MqttTopicData {
    char topic[64];
    char payload[128];
    uint32_t last_updated;
    bool active;
};

class CustomBroker : public PicoMQTT::Server {
public:
    uint32_t active_connections = 0;
    uint32_t total_connections = 0;
    uint32_t total_messages = 0;
    MqttTopicData topic_cache[MAX_TOPIC_CACHE];

    CustomBroker() {
        for (int i = 0; i < MAX_TOPIC_CACHE; i++) {
            topic_cache[i].active = false;
        }
    }

protected:
    void on_connected(const char * client_id) override {
        active_connections++;
        total_connections++;
    }

    void on_disconnected(const char * client_id) override {
        if (active_connections > 0) active_connections--;
    }

    void on_message(const char * topic, PicoMQTT::IncomingPacket & packet) override {
        total_messages++;

        // Read payload into a static buffer to avoid String allocation where possible
        size_t len = packet.get_remaining_size();
        char payload_buf[128] = {0};
        size_t read_len = (len < sizeof(payload_buf) - 1) ? len : sizeof(payload_buf) - 1;
        if (read_len > 0) {
            packet.read((uint8_t*)payload_buf, read_len);
        }
        
        // Cache management: find existing topic, empty slot, or oldest slot
        int slot = -1;
        int oldest_slot = 0;
        uint32_t oldest_time = 0xFFFFFFFF;

        for (int i = 0; i < MAX_TOPIC_CACHE; i++) {
            if (topic_cache[i].active && strcmp(topic_cache[i].topic, topic) == 0) {
                slot = i;
                break;
            }
            if (!topic_cache[i].active && slot == -1) {
                slot = i; // First empty slot found
            }
            // More robust oldest check
            if (topic_cache[i].active && topic_cache[i].last_updated < oldest_time) {
                oldest_time = topic_cache[i].last_updated;
                oldest_slot = i;
            }
        }

        if (slot == -1) {
            slot = oldest_slot; // Evict oldest
        }

        topic_cache[slot].active = true;
        strncpy(topic_cache[slot].topic, topic, sizeof(topic_cache[slot].topic) - 1);
        strncpy(topic_cache[slot].payload, payload_buf, sizeof(topic_cache[slot].payload) - 1);
        topic_cache[slot].last_updated = millis();

        // Pass to base Server so subscribers can receive it!
        PicoMQTT::Server::on_message(topic, packet);
    }
};

CustomBroker mqtt;
WebServer server(80);

const char* ssid = WIFI_SSID;
const char* password = WIFI_PASSWORD;

// HTML layout chunked to sendContent
const char* html_head = R"rawliteral(
<!DOCTYPE html>
<html style="background-color: #1a1a2e; min-height: 100vh;">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <meta http-equiv="refresh" content="10">
    <title>MQTT Broker</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, 'Helvetica Neue', Arial, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            color: #e0e0e0;
            padding: 1rem;
            min-height: 100vh;
            line-height: 1.6;
        }
        #container {
            max-width: 1200px;
            margin: 0 auto;
            padding: 1.5rem;
            background: rgba(30, 30, 46, 0.9);
            border-radius: 16px;
            box-shadow: 0 8px 32px rgba(0, 0, 0, 0.4);
            backdrop-filter: blur(10px);
        }
        h1 { font-size: 1.75rem; font-weight: 600; color: #00d9ff; margin-bottom: 1.5rem; text-align: center; text-shadow: 0 0 20px rgba(0, 217, 255, 0.3); }
        h2 { font-size: 1.25rem; font-weight: 500; color: #00d9ff; margin-bottom: 1rem; text-align: center; border-bottom: 1px solid rgba(0, 217, 255, 0.2); padding-bottom: 0.5rem; }
        .layout-grid { display: grid; grid-template-columns: 400px 1fr; gap: 1.5rem; align-items: start; }
        .status-table {
            background: rgba(22, 33, 62, 0.6);
            border-radius: 12px;
            padding: 1rem;
            border: 1px solid rgba(0, 217, 255, 0.1);
            overflow-x: auto;
        }
        .status-table table { width: 100%; border-collapse: collapse; }
        .status-table th { color: #888; text-align: left; padding: 0.75rem 0.5rem; font-size: 0.9rem; border-bottom: 1px solid rgba(255, 255, 255, 0.1); }
        .status-table td { padding: 0.75rem 0.5rem; font-size: 0.95rem; border-bottom: 1px solid rgba(255, 255, 255, 0.05); }
        .status-table tr:last-child td { border-bottom: none; }
        
        .sys-table td:first-child { color: #888; text-align: right; padding-right: 1rem; width: 45%; font-size: 0.9rem; }
        .sys-table td:last-child { color: #e0e0e0; font-weight: 500; }
        
        .topic-cell { display: inline-block; background: rgba(0,217,255,0.1); color: #00d9ff; padding: 0.1rem 0.4rem; border-radius: 4px; font-family: monospace; font-size: 0.85rem;}
        
        @media (max-width: 900px) {
            .layout-grid { grid-template-columns: 1fr; }
        }
        @media (max-width: 600px) {
            body { padding: 0; }
            #container { border-radius: 0; margin-top: 0; }
            h1 { font-size: 1.5rem; margin-top: 1rem; margin-bottom: 1rem; }
            .sys-table td:first-child { width: 40%; font-size: 0.8rem; }
            .sys-table td, .status-table td { padding: 0.5rem 0.25rem; font-size: 0.85rem; }
        }
    </style>
</head>
<body>
    <div id="container">
        <h1>🚐 T5 Cyber Broker</h1>
        <div class="layout-grid">
            <div class="left-col">
                <h2>System Status</h2>
                <div class="status-table sys-table">
                    <table>
)rawliteral";

const char* html_stats = R"rawliteral(
                        <tr><td>Broker Status</td><td style="color:#00d9ff; font-weight: 600; text-shadow: 0 0 10px rgba(0, 217, 255, 0.3);">ONLINE</td></tr>
                        <tr><td>Broker IP Address</td><td>%s</td></tr>
                        <tr><td>MQTT Port</td><td>1883</td></tr>
                        <tr><td>WiFi Signal</td><td>%d dBm</td></tr>
                        <tr><td>System Uptime</td><td>%lu Seconds</td></tr>
                        <tr><td>Free RAM</td><td>%u / %u KB</td></tr>
                        <tr><td>Active Connections</td><td>%u</td></tr>
                        <tr><td>Total Messages Processed</td><td>%u</td></tr>
                    </table>
                </div>
            </div>
            <div class="right-col">
                <h2>Latest MQTT Items</h2>
                <div class="status-table">
                    <table>
                        <tr><th>Topic</th><th>Payload</th><th>Last Updated</th></tr>
)rawliteral";

const char* html_tail = R"rawliteral(
                    </table>
                </div>
            </div>
        </div>
    </div>
</body>
</html>
)rawliteral";

void handleRoot() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", html_head);

    char buf[1024];
    snprintf(buf, sizeof(buf), html_stats,
             WiFi.localIP().toString().c_str(),
             WiFi.RSSI(),
             (unsigned long)(millis() / 1000),
             ESP.getFreeHeap() / 1024,
             ESP.getHeapSize() / 1024,
             mqtt.active_connections,
             mqtt.total_messages
    );
    server.sendContent(buf);

    uint32_t now = millis();
    bool has_topics = false;
    for (int i = 0; i < MAX_TOPIC_CACHE; i++) {
        if (mqtt.topic_cache[i].active) {
            has_topics = true;
            uint32_t diff_sec = (now - mqtt.topic_cache[i].last_updated) / 1000;
            snprintf(buf, sizeof(buf), 
                     "<tr><td><span class=\"topic-cell\">%s</span></td><td>%s</td><td>%lu sec ago</td></tr>", 
                     mqtt.topic_cache[i].topic, 
                     mqtt.topic_cache[i].payload, 
                     diff_sec);
            server.sendContent(buf);
        }
    }

    if (!has_topics) {
        server.sendContent("<tr><td colspan='3' style='text-align:center;color:#888;'>No messages received yet.</td></tr>");
    }

    server.sendContent(html_tail);
    server.sendContent(""); // End of chunk
}

void setup() {
    Serial.begin(115200);
    Serial.println("\n--- MQTT Broker ---");

    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\n[WiFi] Connected!");
    Serial.print("[WiFi] IP: ");
    Serial.println(WiFi.localIP());

    server.on("/", handleRoot);
    server.begin();
    Serial.println("[HTTP] Dashboard Started on port 80");

    mqtt.begin();
    Serial.println("[MQTT] Broker Started on port 1883");
}

void loop() {
    mqtt.loop();
    server.handleClient();
}

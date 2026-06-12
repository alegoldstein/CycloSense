#include "ws_telemetry.h"
 
#include <Arduino.h>
#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
 
#include "hall.h"
 
extern "C" {
    extern volatile float g_squeak_confidence;
    extern volatile float g_normal_confidence;
    extern volatile int   g_squeak_detected;
}
 
// GPS data shared with main.cpp (extern the same struct + mux)
#include "gps.h"
extern GPS_data       s_gps;
extern portMUX_TYPE   s_gps_mux;
 
// Runtime-overridable circumference (starts at compile-time value)
static float s_circumference_m = HALL_WHEEL_CIRCUMFERENCE_M;
 
#define TELEMETRY_INTERVAL_MS   250   // send 4 packets/sec
#define WS_PATH                 "/ws/device"
#define RECONNECT_INTERVAL_MS   5000
 
static WebSocketsClient  s_ws;
static bool               s_ws_connected = false;
static const char        *s_host         = nullptr;
static uint16_t           s_port         = 8000;
 
// ── WebSocket event handler ───────────────────────────────────
static void on_ws_event(WStype_t type, uint8_t *payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            s_ws_connected = true;
            Serial.printf("[WS] Connected to ws://%s:%u%s\n", s_host, s_port, WS_PATH);
            // Fetch config on connect
            s_ws.sendTXT("{\"type\":\"hello\",\"client\":\"esp32\"}");
            break;
 
        case WStype_DISCONNECTED:
            s_ws_connected = false;
            Serial.println("[WS] Disconnected — will reconnect");
            break;
 
        case WStype_TEXT: {
            // Parse incoming config messages
            JsonDocument doc;
            if (deserializeJson(doc, payload, length) != DeserializationError::Ok) break;
            const char *msg_type = doc["type"] | "";
            if (strcmp(msg_type, "config") == 0) {
                float circ = doc["wheel_circumference_m"] | s_circumference_m;
                s_circumference_m = circ;
                Serial.printf("[WS] Config update: wheel_circumference_m=%.4f m\n", circ);
                // NOTE: hall.cpp reads HALL_WHEEL_CIRCUMFERENCE_M as a macro.
                // To make runtime updates take effect without a reboot, change
                // hall.cpp to use a global `float g_circumference_m` initialised
                // from the macro, and replace all uses of the macro with that
                // global. Then set: extern float g_circumference_m; g_circumference_m = circ;
                g_circumference_m = circ;
            }
            break;
        }
 
        default:
            break;
    }
}
 
// ── WiFi + WebSocket task ─────────────────────────────────────
static void telemetry_task(void *pv) {
    const char *ssid     = ((const char **)pv)[0];
    const char *password = ((const char **)pv)[1];
 
    // Connect WiFi
    Serial.printf("[WS] Connecting to WiFi: %s\n", ssid);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        vTaskDelay(pdMS_TO_TICKS(500));
        Serial.print(".");
    }
    bool ok = Serial.printf("\n[WS] WiFi connected, IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WS] hello send=%d\n", ok);
    
    vTaskDelay(pdMS_TO_TICKS(2000));  // let network stack settle

    // Connect WebSocket
    s_ws.begin(s_host, s_port, WS_PATH);
    s_ws.onEvent(on_ws_event);
    s_ws.setReconnectInterval(RECONNECT_INTERVAL_MS);
 
    // Fetch initial config via HTTP before WebSocket loop
    // (optional — WebSocket config push covers it too)
 
    uint32_t last_send_ms = 0;
    while (1) {
        s_ws.loop();  // must be called as fast as possible
 
        uint32_t now = millis();
        if (s_ws_connected && (now - last_send_ms) >= TELEMETRY_INTERVAL_MS) {
            last_send_ms = now;
 
            // Read sensors
            float speed_mph = hall_get_speed_kmh();  // function name kept for compat
            uint32_t delta_us = hall_get_last_delta_us();
 
            portENTER_CRITICAL(&s_gps_mux);
            GPS_data gps = s_gps;
            portEXIT_CRITICAL(&s_gps_mux);
 
            // Build JSON
            JsonDocument doc;
            doc["speed_mph"]         = serialized(String(speed_mph, 2));
            doc["delta_us"]          = delta_us;
            doc["squeak_detected"]   = (bool)g_squeak_detected;
            doc["squeak_confidence"] = serialized(String((float)g_squeak_confidence, 3));
            doc["normal_confidence"] = serialized(String((float)g_normal_confidence, 3));
            doc["gps_valid"]         = gps.valid;
            if (gps.valid) {
                doc["gps_hours"]      = gps.hours;
                doc["gps_minutes"]    = gps.minutes;
                doc["gps_seconds"]    = gps.seconds;
                doc["gps_latitude"]   = serialized(String(gps.latitude,  6));
                doc["gps_longitude"]  = serialized(String(gps.longitude, 6));
                doc["gps_satellites"] = gps.satellites;
            }
            doc["wheel_circumference_m"] = s_circumference_m;
 
            String out;
            serializeJson(doc, out);
            bool ok = s_ws.sendTXT(out);
            Serial.printf("[WS] send result=%d len=%u wheel circumference=%.4f\n", ok, out.length(), s_circumference_m);
        }
 
        vTaskDelay(pdMS_TO_TICKS(1));  // yield; loop() needs ~1ms cadence
    }
}
 
// ── Public entry point ────────────────────────────────────────
static const char *s_creds[2];
 
void ws_telemetry_start(const char *ssid, const char *password,
                        const char *host, uint16_t port) {
    s_host    = host;
    s_port    = port;
    s_creds[0] = ssid;
    s_creds[1] = password;
    xTaskCreate(telemetry_task, "ws_telem", 8192,
                (void *)s_creds, 5, NULL);
}
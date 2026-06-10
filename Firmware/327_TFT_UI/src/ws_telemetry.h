#pragma once

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif
 
/**
 * Start the background telemetry task.
 * Call once in setup() after WiFi.begin() + waitForConnection.
 *
 * @param ssid      WiFi network name
 * @param password  WiFi password
 * @param host      Backend host IP or hostname (no http://)
 * @param port      Backend port (default 8000)
 */
void ws_telemetry_start(const char *ssid, const char *password,
                        const char *host, uint16_t port);
 
#ifdef __cplusplus
}
#endif
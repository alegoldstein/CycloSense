#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Bitmask flags for set_mode()
#define GPS_GLL  (1 << 0)
#define GPS_RMC  (1 << 1)
#define GPS_VTG  (1 << 2)
#define GPS_GGA  (1 << 3)
#define GPS_GSA  (1 << 4)
#define GPS_GSV  (1 << 5)

typedef struct {
    // Position
    double latitude;   // decimal degrees, + = N
    double longitude;  // decimal degrees, + = E

    // Time (UTC, from RMC/GGA)
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;

    // Movement (from RMC/VTG)
    float speed_knots;
    float heading_deg;   // true course

    // Fix quality (from GGA)
    uint8_t fix_quality; // 0=none, 1=GPS, 2=DGPS
    uint8_t satellites;

    bool valid;          // RMC 'A' = active
} GPS_data;

// Init UART for GPS
void gps_init(void);

// Configure which sentences the module outputs
// mod: bitmask of GPS_GLL | GPS_RMC | ... 
// freq: 0=off, 1=every fix, 5=every 5th fix, etc.
void gps_set_mode(uint8_t mod, uint8_t freq);

// Parse a single NMEA sentence, returns true if a known sentence was decoded
bool gps_parse(const char *sentence, GPS_data *out);

// Blocking read: fills buf with one '\n'-terminated NMEA line
// Returns number of bytes read, -1 on timeout
int gps_read_line(char *buf, size_t max_len, uint32_t timeout_ms);

//update gps rate
void gps_set_update_rate(uint16_t ms); // 1000 = 1Hz, 500 = 2Hz, 200 = 5Hz
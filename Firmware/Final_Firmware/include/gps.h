#ifndef GPS_H
#define GPS_H

#include <stdint.h>
#include <stdbool.h>

// Bitmasks for modes
#define GPS_GLL  (1 << 0)
#define GPS_RMC  (1 << 1)
#define GPS_VTG  (1 << 2)
#define GPS_GGA  (1 << 3)
#define GPS_GSA  (1 << 4)
#define GPS_GSV  (1 << 5)

typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    bool valid;
    double latitude;
    double longitude;
    double speed_knots;
    double heading_deg;
    uint8_t fix_quality;
    uint8_t satellites;
} GPS_data;

void gps_init(void);
void gps_set_mode(uint8_t mod, uint8_t freq);
void gps_set_update_rate(uint16_t ms);
bool gps_parse(const char *sentence, GPS_data *out);

// Crucial for multi-task reading
bool gps_get_latest_data(GPS_data *dest); 

#endif
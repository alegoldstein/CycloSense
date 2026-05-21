#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define GPS_GLL  (1 << 0)
#define GPS_RMC  (1 << 1)
#define GPS_VTG  (1 << 2)
#define GPS_GGA  (1 << 3)
#define GPS_GSA  (1 << 4)
#define GPS_GSV  (1 << 5)

typedef struct {
    double  latitude;
    double  longitude;
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    float   speed_knots;
    float   heading_deg;
    uint8_t fix_quality;
    uint8_t satellites;
    bool    valid;
} GPS_data;

void gps_init(void);
void gps_set_mode(uint8_t mod, uint8_t freq);
bool gps_parse(const char *sentence, GPS_data *out);
int  gps_read_line(char *buf, size_t max_len, uint32_t timeout_ms);
void gps_set_update_rate(uint16_t ms);
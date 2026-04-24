#include "gps.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static const char *TAG = "GPS";

// ── UART wiring ──────────────────────────────────────────────
#ifndef GPS_UART_NUM
#define GPS_UART_NUM  UART_NUM_1
#endif
#ifndef GPS_TX_PIN
#define GPS_TX_PIN    17
#endif
#ifndef GPS_RX_PIN
#define GPS_RX_PIN    18
#endif
#ifndef GPS_BAUD
#define GPS_BAUD      9600
#endif

#define BUF_SIZE 512

// ── Internal helpers ─────────────────────────────────────────

// Compute NMEA checksum: XOR of all bytes between '$' and '*'
static uint8_t nmea_checksum(const char *sentence) {
    uint8_t cs = 0;
    // skip leading '$'
    const char *p = (*sentence == '$') ? sentence + 1 : sentence;
    while (*p && *p != '*') {
        cs ^= (uint8_t)*p++;
    }
    return cs;
}

// Append checksum and \r\n to cmd, then transmit
static void uart_write_nmea(const char *cmd) {
    uint8_t cs = nmea_checksum(cmd);
    char full[160];
    snprintf(full, sizeof(full), "%s*%02X\r\n", cmd, cs);
    uart_write_bytes(GPS_UART_NUM, full, strlen(full));
    ESP_LOGD(TAG, "TX: %s", full);
}

// Convert NMEA lat/lon ddmm.mmmm -> decimal degrees
static double nmea_to_decimal(const char *val, char hemi) {
    if (!val || val[0] == '\0') return 0.0;
    double raw = atof(val);
    int deg = (int)(raw / 100);
    double min = raw - (deg * 100.0);
    double dd = deg + min / 60.0;
    if (hemi == 'S' || hemi == 'W') dd = -dd;
    return dd;
}

// ── Public API ───────────────────────────────────────────────

void gps_init(void) {
    uart_config_t cfg = {
        .baud_rate  = GPS_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
    };
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART_NUM,
        GPS_TX_PIN, GPS_RX_PIN,
        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(GPS_UART_NUM,
        BUF_SIZE * 2, 0, 0, NULL, 0));

    ESP_LOGI(TAG, "GPS UART init on UART%d TX=%d RX=%d @ %d baud",
        GPS_UART_NUM, GPS_TX_PIN, GPS_RX_PIN, GPS_BAUD);
}

void gps_set_mode(uint8_t mod, uint8_t freq) {
    //set mode variable and their frequency, 0 = off, 5 = every 5th fix
    uint8_t GLL = (mod & GPS_GLL) ? freq : 0;
    uint8_t RMC = (mod & GPS_RMC) ? freq : 0;
    uint8_t VTG = (mod & GPS_VTG) ? freq : 0;
    uint8_t GGA = (mod & GPS_GGA) ? freq : 0;
    uint8_t GSA = (mod & GPS_GSA) ? freq : 0;
    uint8_t GSV = (mod & GPS_GSV) ? freq : 0;

    char cmd[128];
    snprintf(cmd, sizeof(cmd),
        "$PMTK314,%d,%d,%d,%d,%d,%d,0,0,0,0,0,0,0,0,0,0,0,0,0,0",
        GLL, RMC, VTG, GGA, GSA, GSV);

    uart_write_nmea(cmd);
}

int gps_read_line(char *buf, size_t max_len, uint32_t timeout_ms) {
    size_t pos = 0;
    TickType_t deadline = xTaskGetTickCount() +
                          pdMS_TO_TICKS(timeout_ms);
    while (pos < max_len - 1) {
        uint8_t byte;
        int n = uart_read_bytes(GPS_UART_NUM, &byte, 1,
                                deadline - xTaskGetTickCount());
        if (n <= 0) return -1; // timeout
        buf[pos++] = (char)byte;
        if (byte == '\n') break;
    }
    buf[pos] = '\0';
    return (int)pos;
}

// ── NMEA parsers ─────────────────────────────────────────────

// Tokenise in-place, returns pointer array into sentence copy
static int split_csv(char *line, char **fields, int max_fields) {
    int count = 0;
    char *p = line;
    while (count < max_fields) {
        fields[count++] = p;
        p = strchr(p, ',');
        if (!p) break;
        *p++ = '\0';
    }
    return count;
}

// $GPRMC,hhmmss.ss,A,lat,N,lon,E,spd,cog,date,...*cs
static bool parse_RMC(char **f, int n, GPS_data *out) {
    if (n < 9) return false;
    // time
    double t = atof(f[1]);
    out->hours   = (uint8_t)(t / 10000);
    out->minutes = (uint8_t)((int)(t / 100) % 100);
    out->seconds = (uint8_t)((int)t % 100);
    // validity
    out->valid = (f[2][0] == 'A');
    // position
    out->latitude  = nmea_to_decimal(f[3], f[4][0]);
    out->longitude = nmea_to_decimal(f[5], f[6][0]);
    // movement
    out->speed_knots = atof(f[7]);
    out->heading_deg = atof(f[8]);
    return true;
}

// $GPGGA,hhmmss.ss,lat,N,lon,E,fix,sats,hdop,alt,...*cs
static bool parse_GGA(char **f, int n, GPS_data *out) {
    if (n < 8) return false;
    out->fix_quality = (uint8_t)atoi(f[6]);
    out->satellites  = (uint8_t)atoi(f[7]);
    // also update position/time if not already from RMC
    double t = atof(f[1]);
    out->hours   = (uint8_t)(t / 10000);
    out->minutes = (uint8_t)((int)(t / 100) % 100);
    out->seconds = (uint8_t)((int)t % 100);
    out->latitude  = nmea_to_decimal(f[2], f[3][0]);
    out->longitude = nmea_to_decimal(f[4], f[5][0]);
    return true;
}

// $GPVTG,cog,T,,,spd_kn,N,spd_kph,K,...*cs
static bool parse_VTG(char **f, int n, GPS_data *out) {
    if (n < 8) return false;
    out->heading_deg = atof(f[1]);
    out->speed_knots = atof(f[5]);
    return true;
}

bool gps_parse(const char *sentence, GPS_data *out) {
    // Validate checksum
    const char *star = strrchr(sentence, '*');
    if (star) {
        uint8_t expected = (uint8_t)strtol(star + 1, NULL, 16);
        // checksum over chars between $ and *
        char tmp[128];
        strncpy(tmp, sentence, sizeof(tmp) - 1);
        tmp[sizeof(tmp)-1] = '\0';
        char *end = strrchr(tmp, '*');
        if (end) *end = '\0';
        if (nmea_checksum(tmp) != expected) {
            ESP_LOGW(TAG, "Checksum mismatch: %s", sentence);
            return false;
        }
    }

    // Copy and strip *XX\r\n for tokenising
    char buf[128];
    strncpy(buf, sentence, sizeof(buf) - 1);
    buf[sizeof(buf)-1] = '\0';
    char *end = strpbrk(buf, "*\r\n");
    if (end) *end = '\0';

    char *fields[20];
    int n = split_csv(buf, fields, 20);
    if (n < 1) return false;

    // Match talker+sentence type (handles GP, GN, GL prefixes)
    const char *type = fields[0] + 3; // skip $GP / $GN / $GL
    if      (strcmp(type, "RMC") == 0) return parse_RMC(fields, n, out);
    else if (strcmp(type, "GGA") == 0) return parse_GGA(fields, n, out);
    else if (strcmp(type, "VTG") == 0) return parse_VTG(fields, n, out);

    return false; // GSA/GSV/GLL not parsed yet
}
//gps.c for rtos

#include "gps.h"
#include "driver/uart.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"

static const char *TAG = "GPS";

// ── UART wiring configurations ───────────────────────────────────
#ifndef GPS_UART_NUM
#define GPS_UART_NUM  UART_NUM_1
#endif
#ifndef GPS_TX_PIN
#define GPS_TX_PIN    17
#endif
#ifndef GPS_RX_PIN
#define GPS_RX_PIN    16
#endif
#ifndef GPS_BAUD
#define GPS_BAUD      9600
#endif

#define BUF_SIZE 512

// FreeRTOS Handles for background processing
static QueueHandle_t gps_uart_queue = NULL;
static SemaphoreHandle_t gps_mutex = NULL;

// This holds the live, absolute source-of-truth GPS values
static GPS_data shared_gps_data; 

// ── Your Original Internal Helpers & Parsers ─────────────────────

static uint8_t nmea_checksum(const char *sentence) {
    uint8_t cs = 0;
    const char *p = (*sentence == '$') ? sentence + 1 : sentence;
    while (*p && *p != '*') {
        cs ^= (uint8_t)*p++;
    }
    return cs;
}

static void uart_write_nmea(const char *cmd) {
    uint8_t cs = nmea_checksum(cmd);
    char full[160];
    snprintf(full, sizeof(full), "%s*%02X\r\n", cmd, cs);
    uart_write_bytes(GPS_UART_NUM, full, strlen(full));
    ESP_LOGD(TAG, "TX: %s", full);
}

static double nmea_to_decimal(const char *val, char hemi) {
    if (!val || val[0] == '\0') return 0.0;
    double raw = atof(val);
    int deg = (int)(raw / 100);
    double min = raw - (deg * 100.0);
    double dd = deg + min / 60.0;
    if (hemi == 'S' || hemi == 'W') dd = -dd;
    return dd;
}

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

static bool parse_RMC(char **f, int n, GPS_data *out) {
    if (n < 9) return false;
    double t = atof(f[1]);
    out->hours   = (uint8_t)(t / 10000);
    out->minutes = (uint8_t)((int)(t / 100) % 100);
    out->seconds = (uint8_t)((int)t % 100);
    out->valid = (f[2][0] == 'A');
    out->latitude  = nmea_to_decimal(f[3], f[4][0]);
    out->longitude = nmea_to_decimal(f[5], f[6][0]);
    out->speed_knots = atof(f[7]);
    out->heading_deg = atof(f[8]);
    return true;
}

static bool parse_GGA(char **f, int n, GPS_data *out) {
    if (n < 8) return false;
    out->fix_quality = (uint8_t)atoi(f[6]);
    out->satellites  = (uint8_t)atoi(f[7]);
    double t = atof(f[1]);
    out->hours   = (uint8_t)(t / 10000);
    out->minutes = (uint8_t)((int)(t / 100) % 100);
    out->seconds = (uint8_t)((int)t % 100);
    out->latitude  = nmea_to_decimal(f[2], f[3][0]);
    out->longitude = nmea_to_decimal(f[4], f[5][0]);
    return true;
}

static bool parse_VTG(char **f, int n, GPS_data *out) {
    if (n < 8) return false;
    out->heading_deg = atof(f[1]);
    out->speed_knots = atof(f[5]);
    return true;
}

bool gps_parse(const char *sentence, GPS_data *out) {
    const char *star = strrchr(sentence, '*');
    if (star) {
        uint8_t expected = (uint8_t)strtol(star + 1, NULL, 16);
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

    char buf[128];
    strncpy(buf, sentence, sizeof(buf) - 1);
    buf[sizeof(buf)-1] = '\0';
    char *end = strpbrk(buf, "*\r\n");
    if (end) *end = '\0';

    char *fields[20];
    int n = split_csv(buf, fields, 20);
    if (n < 1) return false;
    if (strlen(fields[0]) < 6) return false;

    const char *type = fields[0] + 3; // skip $GP / $GN / $GL
    if      (strcmp(type, "RMC") == 0) return parse_RMC(fields, n, out);
    else if (strcmp(type, "GGA") == 0) return parse_GGA(fields, n, out);
    else if (strcmp(type, "VTG") == 0) return parse_VTG(fields, n, out);

    return false;
}

// ── The FreeRTOS Background Parser Task ──────────────────────────

static void gps_parser_task(void *pvParameters) {
    uart_event_t event;
    uint8_t* sentence_buf = (uint8_t*) malloc(BUF_SIZE);
    
    while (1) {
        // Sleep here using 0% CPU until a hardware event arrives
        if (xQueueReceive(gps_uart_queue, (void *)&event, portMAX_DELAY)) {
            memset(sentence_buf, 0, BUF_SIZE);

            switch (event.type) {
                case UART_PATTERN_DET: {
                    int pos = uart_pattern_pop_pos(GPS_UART_NUM);
                    if (pos != -1) {
                        // Extract the complete line from the buffer
                        int bytes_read = uart_read_bytes(GPS_UART_NUM, sentence_buf, pos + 1, portMAX_DELAY);
                        if (bytes_read > 0) {
                            sentence_buf[bytes_read] = '\0';

                            // Lock the Mutex before modifying the global structure
                            if (xSemaphoreTake(gps_mutex, portMAX_DELAY) == pdTRUE) {
                                
                                // Call your parsing logic directly here!
                                gps_parse((char*)sentence_buf, &shared_gps_data);
                                
                                // Unlock immediately when done
                                xSemaphoreGive(gps_mutex);
                            }
                        }
                    }
                    break;
                }

                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "Ring buffer full. Flushing input...");
                    uart_flush_input(GPS_UART_NUM);
                    xQueueReset(gps_uart_queue);
                    break;

                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "Hardware FIFO Overflow!");
                    uart_flush_input(GPS_UART_NUM);
                    xQueueReset(gps_uart_queue);
                    break;

                default:
                    break;
            }
        }
    }
    free(sentence_buf);
    vTaskDelete(NULL);
}

// ── New Public Thread-Safe API ───────────────────────────────────

void gps_init(void) {
    // Instantiate the Mutex barrier
    gps_mutex = xSemaphoreCreateMutex();
    memset(&shared_gps_data, 0, sizeof(GPS_data));

    uart_config_t cfg = {
        .baud_rate  = GPS_BAUD,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(GPS_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(GPS_UART_NUM, GPS_TX_PIN, GPS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    
    // Allocate the background ring buffers
    ESP_ERROR_CHECK(uart_driver_install(GPS_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 20, &gps_uart_queue, 0));

    // Turn on newline pattern detection interrupts
    ESP_ERROR_CHECK(uart_enable_pattern_det_baud_intr(GPS_UART_NUM, '\n', 1, 9, 0, 0));
    ESP_ERROR_CHECK(uart_pattern_queue_reset(GPS_UART_NUM, 20));

    // Spin up the background event task on Core 1
    xTaskCreatePinnedToCore(gps_parser_task, "gps_parse_task", 3584, NULL, 12, NULL, 1);

    ESP_LOGI(TAG, "GPS Engine & FreeRTOS task initialized.");
}

/**
 * @brief Thread-safe public function to grab a copy of the latest location state
 * @param dest Pointer to a local GPS_data variable where values will be copied
 * @return true if copy was successful, false if mutex timed out
 */
bool gps_get_latest_data(GPS_data *dest) {
    if (gps_mutex == NULL || dest == NULL) return false;
    
    // Try to safely lock the data for up to 10 milliseconds
    if (xSemaphoreTake(gps_mutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        memcpy(dest, &shared_gps_data, sizeof(GPS_data));
        xSemaphoreGive(gps_mutex);
        return true;
    }
    return false;
}

void gps_set_mode(uint8_t mod, uint8_t freq) {
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

void gps_set_update_rate(uint16_t ms) {
    char cmd[32];
    snprintf(cmd, sizeof(cmd), "$PMTK220,%d", ms);
    uart_write_nmea(cmd);
}
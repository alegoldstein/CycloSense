#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gps.h"          // ← this was missing

static const char *TAG = "MAIN";

void app_main(void) {
    gps_init();
    vTaskDelay(pdMS_TO_TICKS(100));
    gps_set_mode(GPS_RMC | GPS_GGA, 1);   // was "ps_set_mode" — typo
    gps_set_update_rate(1000);

    GPS_data data = {0};
    char line[128];

    while (1) {
        if (gps_read_line(line, sizeof(line), 1000) > 0) {
            ESP_LOGI(TAG, "RAW: %s", line);   
            if (gps_parse(line, &data) && data.valid) {
                ESP_LOGI(TAG, "Fix: %.6f, %.6f  hdg=%.1f  spd=%.1fkn  sats=%d",
                    data.latitude, data.longitude,
                    data.heading_deg, data.speed_knots,
                    data.satellites);
            }
        }
    }
}
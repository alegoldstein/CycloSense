#include "gps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MAIN";

void app_main(void) {
    gps_init();

    // Only RMC + GGA, output every fix
    gps_set_mode(GPS_RMC | GPS_GGA | GPS_VTG, 1);

    GPS_data data = {0};
    char line[128];

    while (1) {
        if (gps_read_line(line, sizeof(line), 1000) > 0) {
            if (gps_parse(line, &data) && data.valid) {
                ESP_LOGI(TAG, "Fix: %.6f, %.6f  hdg=%.1f°  spd=%.1fkn  sats=%d",
                    data.latitude, data.longitude,
                    data.heading_deg, data.speed_knots,
                    data.satellites);
            }
        }
    }
}
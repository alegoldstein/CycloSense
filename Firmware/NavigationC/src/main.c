#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "esp_timer.h"
#include "astar.h"
#include "display.h"
#include "graphing.h"

static const char *TAG = "bike";

#define ROUTE_START_NODE  7751u
#define ROUTE_END_NODE    2377u
#define MAX_PATH          2048

Graph     g_graph;
uint32_t *g_path     = NULL;
int       g_path_len = 0;

static int init_spiffs(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path              = "/spiffs",
        .partition_label        = NULL,
        .max_files              = 4,
        .format_if_mount_failed = false,
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPIFFS mount failed: %s", esp_err_to_name(ret));
        return -1;
    }
    size_t total = 0, used = 0;
    esp_spiffs_info(NULL, &total, &used);
    ESP_LOGI(TAG, "SPIFFS: %d KB used / %d KB total",
             (int)(used / 1024), (int)(total / 1024));
    return 0;
}

void app_main(void)
{
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_ERROR_CHECK(display_init());

    if (init_spiffs() != 0) return;

    if (astar_load_graph(&g_graph, "/spiffs/graph.bin") != 0) {
        ESP_LOGE(TAG, "graph load failed");
        return;
    }
    ESP_LOGI(TAG, "graph: %lu nodes", (unsigned long)g_graph.node_count);

    g_path = (uint32_t *)malloc(MAX_PATH * sizeof(uint32_t));
    if (!g_path) { ESP_LOGE(TAG, "OOM path"); return; }

    int64_t t0 = esp_timer_get_time();
    g_path_len = astar_find(&g_graph, ROUTE_START_NODE, ROUTE_END_NODE,
                            g_path, MAX_PATH);
    int64_t t1 = esp_timer_get_time();

    if (g_path_len < 0) {
        ESP_LOGE(TAG, "astar_find failed: %d", g_path_len);
        return;
    }
    ESP_LOGI(TAG, "route: %d nodes in %lld ms",
             g_path_len, (t1 - t0) / 1000);

    draw_background(&g_graph);
    draw_route(&g_graph, g_path, g_path_len);
    display_fill_circle(120, 120, 5, COLOR_RED);
    draw_metrics(0, 0, 0);

    ESP_LOGI(TAG, "display ready");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
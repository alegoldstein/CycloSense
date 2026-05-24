// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h" // Required for tasks



// ///////////////////////////////////////////////////////////////
// //update display based on gathered information
// ///////////////////////////////////////////////////////////////
// void update_display(){

// }


// ///////////////////////////////////////////////////////////////
// // get hall sensor reading and calculate speed
// ///////////////////////////////////////////////////////////////
// void get_speed(){
// }

// //check if user wants to input new coords

// ///////////////////////////////////////////////////////////////
// //parse GPS data when uart event is triggered
// ///////////////////////////////////////////////////////////////
// static void gps_parser_task(void *pvParameters) {
//     uart_event_t event;
//     // Allocate a buffer to hold the incoming sentence
//     uint8_t* sentence_buf = (uint8_t*) malloc(UART_BUF_SIZE);
    
//     while (1) {
//         // Block indefinitely until the internal UART ISR sends an event
//         if (xQueueReceive(gps_uart_queue, (void *)&event, portMAX_DELAY)) {
//             // Clear the buffer safely before reading
//             memset(sentence_buf, 0, UART_BUF_SIZE);
            
//             switch (event.type) {
                
//                 // fire when \n is detected
//                 case UART_PATTERN_DET: {
//                     // Get the memory position where the newline character is located
//                     int pos = uart_pattern_pop_pos(GPS_UART_NUM);
                    
//                     if (pos != -1) {
//                         // Read exactly up to the newline character (+1 to include the '\n' itself)
//                         int bytes_read = uart_read_bytes(GPS_UART_NUM, sentence_buf, pos + 1, portMAX_DELAY);
                        
//                         if (bytes_read > 0) {
//                             // Null-terminate the string safely to prevent overflows during string parsing
//                             sentence_buf[bytes_read] = '\0';
                            
//                             // Strip trailing carriage returns (\r) if present to clean up the string
//                             if (bytes_read > 1 && sentence_buf[bytes_read - 2] == '\r') {
//                                 sentence_buf[bytes_read - 2] = '\0';
//                             }

//                             // Output the clean sentence
//                             ESP_LOGI(TAG, "Full Sentence Received: %s", (char*)sentence_buf);
                            
//                             gps_parse((char*)sentence_buf, &shared_gps_data);
//                         }
//                     }
//                     break;
//                 }
                
//                 // Optional handling: If the buffer fills up before seeing a newline
//                 case UART_BUFFER_FULL:
//                     ESP_LOGW(TAG, "UART Internal Ring Buffer Full! Flushing input...");
//                     uart_flush_input(GPS_UART_NUM);
//                     xQueueReset(gps_uart_queue);
//                     break;
                
//                 // Optional handling: Hardware data overflow or parity errors
//                 case UART_FIFO_OVF:
//                     ESP_LOGW(TAG, "UART HW FIFO Overflow!");
//                     uart_flush_input(GPS_UART_NUM);
//                     xQueueReset(gps_uart_queue);
//                     break;

//                 default:
//                     break;
//             }
//         }
//     }
    
//     // Clean up if the task ever exits (unlikely in an embedded main loop)
//     free(sentence_buf);
//     vTaskDelete(NULL);
// }

// /**
//  * @brief Configures UART parameters, allocates ring buffers, and sets up pattern detection.
//  */
// void init_gps_uart(void) {
//     uart_config_t uart_config = {
//         .baud_rate = 9600,                  // Default baud rate for most GPS modules
//         .data_bits = UART_DATA_8_BITS,
//         .parity = UART_PARITY_DISABLE,
//         .stop_bits = UART_STOP_BITS_1,
//         .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
//         .source_clk = UART_SCLK_DEFAULT,
//     };
    
//     // 1. Apply configurations to the peripheral
//     ESP_ERROR_CHECK(uart_param_config(GPS_UART_NUM, &uart_config));

//     // 2. Map the physical RX and TX pins
//     ESP_ERROR_CHECK(uart_set_pin(GPS_UART_NUM, GPS_TX_PIN, GPS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

//     // 3. Install the UART driver. This hooks up the low-level ISR and instantiates 'gps_uart_queue'
//     ESP_ERROR_CHECK(uart_driver_install(GPS_UART_NUM, UART_BUF_SIZE * 2, UART_BUF_SIZE * 2, 20, &gps_uart_queue, 0));

//     // 4. Configure the Pattern Detect Hardware Interrupt
//     // - Look for the '\n' character
//     // - Trigger on 1 consecutive instance of it
//     // - Set timeout thresholds (9 bits duration is typical for standard baud rates)
//     ESP_ERROR_CHECK(uart_enable_pattern_det_baud_intr(GPS_UART_NUM, '\n', 1, 9, 0, 0));
    
//     // Set the maximum size for tracking pattern positions in the buffer
//     ESP_ERROR_CHECK(uart_pattern_queue_reset(GPS_UART_NUM, 20));

//     // 5. Spin up the FreeRTOS processing task
//     xTaskCreatePinnedToCore(
//         gps_parser_task,    // Task function
//         "gps_parser_task",  // Name
//         4096,               // Stack size in bytes
//         NULL,               // Parameters
//         12,                 // High priority (to ensure we process data before the next sentence)
//         NULL,               // Task Handle
//         1                   // Pin to Core 1 (keeps Core 0 free for RF/Wi-Fi systems if needed)
//     );
    
//     ESP_LOGI(TAG, "GPS UART system initialized.");
// }

// void app_main() {

//     xInterrupt

//     xTaskCreate(
//         update_display,   // Function that implements the task.
//         "update_display", // Text name for the task.
//         2048,            // Stack size in words, not bytes.
//         NULL,            // Parameter passed into the task.
//         1,               // Priority at which the task is created.
//         NULL             // Used to pass out the created task's handle.
//     );

//     xTaskCreate(
//         get_gps_cords,   
//         "gps_parser_task", 
//         2048,            
//         NULL,            
//         1,              
//         NULL             
//     );

//     xTaskCreate(
//         get_speed,       
//         "get_speed",     
//         2048,            
//         NULL,            
//         1,              
//         NULL             
//     );

//     xTaskCreatePinnedToCore(
//         gps_tracking_loop_task,  
//         "get_gps_cords",       
//         3072,                    
//         NULL,                    
//         5,                       
//         NULL,                    
//         1                        
//     );



// }
/*
 * main.c — A* bringup test for ESP32
 *
 * Loads the synthetic test graph from flash (no SD card needed),
 * runs astar_find(), and prints a clear PASS / FAIL verdict over UART.
 *
 * No FreeRTOS tasks, no display, no GPS — just the algorithm.
 * Replace astar_load_graph_from_mem() with astar_load_graph("/sdcard/graph.bin")
 * once storage is wired up.
 */

#include <stdio.h>
#include <stdint.h>
#include "esp_log.h"
#include "astar.h"
#include "astar_mem.h"
#include "graph_test.h"

static const char *TAG = "astar_test";

/* Longest path we ever expect in the test graph */
#define MAX_PATH 64

void app_main(void)
{
    ESP_LOGI(TAG, "=== A* bringup test ===");
    ESP_LOGI(TAG, "Graph: %u nodes, %u edges (%u bytes in flash)",
             TEST_GRAPH_NODE_COUNT, TEST_GRAPH_EDGE_COUNT, TEST_GRAPH_BYTE_SIZE);

    /* ------------------------------------------------------------------ */
    /* 1. Load graph from the embedded byte array                          */
    /* ------------------------------------------------------------------ */
    Graph g;
    if (astar_load_graph_from_mem(&g, TEST_GRAPH_BIN, TEST_GRAPH_BYTE_SIZE) != 0) {
        ESP_LOGE(TAG, "FAIL — graph load error");
        return;
    }
    ESP_LOGI(TAG, "Graph loaded OK");

    /* ------------------------------------------------------------------ */
    /* 2. Run A*                                                           */
    /* ------------------------------------------------------------------ */
    uint32_t path[MAX_PATH];
    int result = astar_find(&g,
                            TEST_GRAPH_START_NODE,
                            TEST_GRAPH_END_NODE,
                            path,
                            MAX_PATH);

    /* ------------------------------------------------------------------ */
    /* 3. Print result                                                     */
    /* ------------------------------------------------------------------ */
    if (result < 0) {
        const char *reason = "unknown";
        if      (result == ASTAR_ERR_OOM)          reason = "out of memory";
        else if (result == ASTAR_ERR_NO_PATH)       reason = "no path exists";
        else if (result == ASTAR_ERR_INVALID_NODE)  reason = "invalid node ID";
        else if (result == ASTAR_ERR_PATH_TOO_LONG) reason = "path buffer too small";
        ESP_LOGE(TAG, "FAIL — astar_find returned %d (%s)", result, reason);
        astar_free_graph(&g);
        return;
    }

    int path_len = result;
    ESP_LOGI(TAG, "Path found: %d node(s)", path_len);

    /* Print the node sequence */
    for (int i = 0; i < path_len; i++) {
        ESP_LOGI(TAG, "  step %d -> node %lu", i, (unsigned long)path[i]);
    }

    /* Compute actual cost by walking the CSR adjacency */
    uint32_t actual_cost = 0;
    for (int i = 0; i + 1 < path_len; i++) {
        uint32_t u = path[i];
        uint32_t v = path[i + 1];
        int found = 0;
        for (uint32_t ei = g.row_ptr[u]; ei < g.row_ptr[u + 1]; ei++) {
            if (g.adj_node[ei] == v) {
                actual_cost += g.adj_cost[ei];
                found = 1;
                break;
            }
        }
        if (!found) {
            ESP_LOGE(TAG, "FAIL — edge %lu->%lu not found in adjacency",
                     (unsigned long)u, (unsigned long)v);
            astar_free_graph(&g);
            return;
        }
    }
    ESP_LOGI(TAG, "Total path cost: %lu dm  (expected %u dm)",
             (unsigned long)actual_cost, TEST_EXPECTED_COST_DM);

    /* ------------------------------------------------------------------ */
    /* 4. Validate against known-good answers                              */
    /* ------------------------------------------------------------------ */
    int pass = 1;

    if (path_len != TEST_EXPECTED_PATH_LEN) {
        ESP_LOGE(TAG, "FAIL — expected path length %d, got %d",
                 TEST_EXPECTED_PATH_LEN, path_len);
        pass = 0;
    }
    if (path[0] != TEST_GRAPH_START_NODE) {
        ESP_LOGE(TAG, "FAIL — path does not start at node %u", TEST_GRAPH_START_NODE);
        pass = 0;
    }
    if (path[path_len - 1] != TEST_GRAPH_END_NODE) {
        ESP_LOGE(TAG, "FAIL — path does not end at node %u", TEST_GRAPH_END_NODE);
        pass = 0;
    }
    if (actual_cost != TEST_EXPECTED_COST_DM) {
        ESP_LOGE(TAG, "FAIL — cost mismatch: expected %u dm, got %lu dm",
                 TEST_EXPECTED_COST_DM, (unsigned long)actual_cost);
        pass = 0;
    }

    if (pass) {
        ESP_LOGI(TAG, "=============================");
        ESP_LOGI(TAG, "  *** PASS — A* WORKS ***    ");
        ESP_LOGI(TAG, "=============================");
    } else {
        ESP_LOGE(TAG, "=============================");
        ESP_LOGE(TAG, "  *** FAIL — see above ***   ");
        ESP_LOGE(TAG, "=============================");
    }

    astar_free_graph(&g);
}
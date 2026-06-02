#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"

#include "gps.h"
#include "Astar.h"
#include "tmag5273.h"

#define I2C_PORT_NUM            0
#define I2C_SDA                 21
#define I2C_SCL                 22
#define I2C_GLITCH_IGNORE       7

#define STACK_SIZE              2048
#define WHEEL_CIRCUMFERENCE     10
 
// speed sensor acquisition + calculation 1
void speed_calc(uint32_t delta_us) {
    if (delta_us == 0) {
        return;
    }

    float speed = WHEEL_CIRCUMFERENCE / (delta_us / 1000000.0f);
}

//gps data acquisition + parsing 2

//inference output 4

//update display 3

//battery life 5

//receive from website 2

//update website 3



void app_main() {
    //initialization
    // start web server

    //i2c to pmic and magnetometer
    i2c_master_bus_handle_t i2c_bus;
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_PORT_NUM,
        .sda_io_num = I2C_SDA,
        .scl_io_num = I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = I2C_GLITCH_IGNORE,
        .flags.enable_internal_pullup = 1
    };

    esp_err_t esp_ret = i2c_new_master_bus(&bus_config, &i2c_bus);
    if (esp_ret != ESP_OK) {
        printf("ERROR: Failed to initialize I2C bus\r\n");
        abort();
    }

    //spi to display

    //i2s to microphone

    //uart to gps

    //UI defaults

    //calculate route

    //

    //create tasks
    esp_ret = tmag5273_set_spike_task(speed_calc, 1);
    if (esp_ret != ESP_OK) {
        printf("ERROR: Failed to create speed task\r\n");
        abort();
    }

    esp_ret = tmag5273_init(&i2c_bus);
    if (esp_ret != ESP_OK) {
        printf("ERROR: Failed to initialize magnetometer\r\n");
        abort();
    }

    // xTaskCreate(
    //             GPS,      
    //             "GPS",          
    //             STACK_SIZE,     
    //             ( void * ) 1,    
    //             3,
    //             &GPS );      

    // xTaskCreate(
    //             PredictiveInference,      
    //             "PredictiveInference",          
    //             STACK_SIZE,     
    //             ( void * ) 1,    
    //             1,
    //             &Inference );   
                
    // xTaskCreate(
    //             UpdateDisplay,      
    //             "UpdateDisplay",          
    //             STACK_SIZE,     
    //             ( void * ) 1,    
    //             2,
    //             &Display );   

    // xTaskCreate(
    //             BatteryLife,      
    //             "BatteryLife",          
    //             STACK_SIZE,     
    //             ( void * ) 1,    
    //             0,
    //             &Battery ); 
                
    // xTaskCreate(
    //             WebsiteOUT,      
    //             "WebsiteOUT",          
    //             STACK_SIZE,     
    //             ( void * ) 1,    
    //             2,
    //             &WebOUT );   

    // xTaskCreate(
    //             WebsiteIN,      
    //             "WebsiteIN",          
    //             STACK_SIZE,     
    //             ( void * ) 1,    
    //             3,
    //             &WebIN );   
}


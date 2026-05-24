#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "driver/gpio.h"
#include "gps.h"
#include "Astar.h"

 
// speed sensor acquisition + calculation 1

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
//spi to display
//i2s to microphone
//uart to gps

//UI defaults

//calculate route

//

//create tasks
xTaskCreate(
            SpeedSensor,       /* Function that implements the task. */
            "SpeedSensor",          /* Text name for the task. */
            STACK_SIZE,      /* Stack size in words, not bytes. */
            ( void * ) 1,    /* Parameter passed into the task. */
            4,/* Priority at which the task is created. */
            &Speed );      /* Used to pass out the created task's handle. */

xTaskCreate(
            GPS,      
            "GPS",          
            STACK_SIZE,     
            ( void * ) 1,    
            3,
            &GPS );      

xTaskCreate(
            PredictiveInference,      
            "PredictiveInference",          
            STACK_SIZE,     
            ( void * ) 1,    
            1,
            &Inference );   
            
xTaskCreate(
            UpdateDisplay,      
            "UpdateDisplay",          
            STACK_SIZE,     
            ( void * ) 1,    
            2,
            &Display );   

xTaskCreate(
            BatteryLife,      
            "BatteryLife",          
            STACK_SIZE,     
            ( void * ) 1,    
            0,
            &Battery ); 
            
xTaskCreate(
            WebsiteOUT,      
            "WebsiteOUT",          
            STACK_SIZE,     
            ( void * ) 1,    
            2,
            &WebOUT );   

xTaskCreate(
            WebsiteIN,      
            "WebsiteIN",          
            STACK_SIZE,     
            ( void * ) 1,    
            3,
            &WebIN );   


}


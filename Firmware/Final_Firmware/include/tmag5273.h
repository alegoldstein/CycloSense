#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"

#define TMAG5273_I2C_ADDRESS    0x35
#define TMAG5273_I2C_SCL_SPEED  100000

typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t mag;
} tmag5273_measurement_t;

typedef enum {
    DEVICE_CONFIG_1,
    DEVICE_CONFIG_2,
    SENSOR_CONFIG_1,
    SENSOR_CONFIG_2,
    X_THR_CONFIG,
    Y_THR_CONFIG,
    Z_THR_CONFIG,
    T_CONFIG,
    INT_CONFIG_1,
    MAG_GAIN_CONFIG,
    MAG_OFFSET_CONFIG_1,
    MAG_OFFSET_CONFIG_2,
    I2C_ADDRESS,
    DEVICE_ID,
    MANUFACTURER_ID_LSB,
    MANUFACTURER_ID_MSB,
    T_MSB_RESULT,
    T_LSB_RESULT,
    X_MSB_RESULT,
    X_LSB_RESULT,
    Y_MSB_RESULT,
    Y_LSB_RESULT,
    Z_MSB_RESULT,
    Z_LSB_RESULT,
    CONV_STATUS,
    ANGLE_RESULT_MSB,
    ANGLE_RESULT_LSB,
    MAGNITUDE_RESULT,
    DEVICE_STATUS
} tmag5273_reg_t;

esp_err_t tmag5273_init(i2c_master_bus_handle_t *i2c_bus);

esp_err_t tmag5273_check_device_id(void);

esp_err_t tmag5273_check_manufacturer_id(void);

esp_err_t tmag5273_read(tmag5273_measurement_t *measurement);
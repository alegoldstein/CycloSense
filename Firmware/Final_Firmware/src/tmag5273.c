#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"

#include "tmag5273.h"

i2c_master_dev_handle_t tmag5273;
tmag5273_reg_t reg;

static TaskHandle_t        s_spike_task_handle = NULL;
static tmag5273_spike_cb_t s_spike_cb          = NULL;
static int64_t             s_last_isr_time_us  = 0;
 
static void IRAM_ATTR tmag5273_isr_handler(void *arg) {
    int64_t now = esp_timer_get_time();
    uint32_t delta_us = (s_last_isr_time_us == 0) ? 0 : (uint32_t)(now - s_last_isr_time_us);
    s_last_isr_time_us = now;
 
    BaseType_t higher_priority_task_woken = pdFALSE;
    xTaskNotifyFromISR(s_spike_task_handle, delta_us, eSetValueWithOverwrite, &higher_priority_task_woken);
    portYIELD_FROM_ISR(higher_priority_task_woken);
}
 
static void tmag5273_spike_task(void *arg) {
    uint32_t delta_us;
    while (1) {
        xTaskNotifyWait(0, 0, &delta_us, portMAX_DELAY);
        if (s_spike_cb != NULL) {
            s_spike_cb(delta_us);
        }
    }
}
 
esp_err_t tmag5273_set_spike_task(tmag5273_spike_cb_t cb, UBaseType_t priority) {
    s_spike_cb = cb;
    BaseType_t ret = xTaskCreate(
        tmag5273_spike_task,
        "tmag5273_spike",
        configMINIMAL_STACK_SIZE + 1024,
        NULL,
        priority,
        &s_spike_task_handle
    );
    return (ret == pdPASS) ? ESP_OK : ESP_ERR_NO_MEM;
}

static esp_err_t tmag5273_write_reg(tmag5273_reg_t target_reg, uint8_t value) {
    uint8_t buf[2] = { (uint8_t)target_reg, value };
    esp_err_t esp_ret = i2c_master_transmit(tmag5273, buf, 2, -1);
    if (esp_ret != ESP_OK) {
        printf("ERROR: Failed to write register %d\r\n", target_reg);
    }
    return esp_ret;
}

esp_err_t tmag5273_check_device_id(void) {
    esp_err_t esp_ret;
    uint8_t id;

    reg = DEVICE_ID;
    esp_ret = i2c_master_transmit_receive(tmag5273, (uint8_t*)&reg, 1, &id, 1, -1);
    if (esp_ret != ESP_OK) {
        printf("ERROR: Failed to read from magnetometer\r\n");
        return esp_ret;
    }

    if (id != 0x02) {
        printf("ERROR: Wrong device ID---received %#X, expected 0x02\r\n", id);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t tmag5273_check_manufacturer_id(void) {
    esp_err_t esp_ret;
    uint8_t id_msb;
    uint8_t id_lsb;
    uint16_t id;

    reg = MANUFACTURER_ID_MSB;
    esp_ret = i2c_master_transmit_receive(tmag5273, (uint8_t*)&reg, 1, &id_msb, 1, -1);
    if (esp_ret != ESP_OK) {
        printf("ERROR: Failed to read from magnetometer\r\n");
        return esp_ret;
    }

    reg = MANUFACTURER_ID_LSB;
    esp_ret = i2c_master_transmit_receive(tmag5273, (uint8_t*)&reg, 1, &id_lsb, 1, -1);
    if (esp_ret != ESP_OK) {
        printf("ERROR: Failed to read from magnetometer\r\n");
        return esp_ret;
    }

    id = (id_msb << 8) | id_lsb;

    if (id != 0x5459) {
        printf("ERROR: Wrong manufacturer ID---got %#X, expected 0x5459\r\n", id);
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t tmag5273_init(i2c_master_bus_handle_t *i2c_bus) {
    esp_err_t esp_ret;

    i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TMAG5273_I2C_ADDRESS,
        .scl_speed_hz = TMAG5273_I2C_SCL_SPEED
    };

    esp_ret = i2c_master_bus_add_device(*i2c_bus, &dev_config, &tmag5273);
    if (esp_ret != ESP_OK) {
        printf("ERROR: Failed to add magnetometer to I2C bus\r\n");
        return esp_ret;
    }

    esp_ret = tmag5273_check_device_id();
    if (esp_ret != ESP_OK) {
        return esp_ret;
    }
    
    esp_ret = tmag5273_check_manufacturer_id();
    if (esp_ret != ESP_OK) {
        return esp_ret;
    }

    esp_ret = tmag5273_write_reg(SENSOR_CONFIG_1, 0x07);
    if (esp_ret != ESP_OK) {
        return esp_ret;
    }

    esp_ret = tmag5273_write_reg(DEVICE_CONFIG_2, 0x08);
    if (esp_ret != ESP_OK) {
        return esp_ret;
    }

    esp_ret = tmag5273_write_reg(Z_THR_CONFIG, (TMAG5273_Z_THRESHOLD << 1) | 0x01);
    if (esp_ret != ESP_OK) {
        return esp_ret;
    }

    esp_ret = tmag5273_write_reg(INT_CONFIG_1, 0x24);
    if (esp_ret != ESP_OK) {
        return esp_ret;
    }
 
    gpio_config_t int_gpio_cfg = {
        .pin_bit_mask = (1ULL << TMAG5273_INT_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    esp_ret = gpio_config(&int_gpio_cfg);
    if (esp_ret != ESP_OK) {
        printf("ERROR: Failed to configure INT GPIO\r\n");
        return esp_ret;
    }
 
    esp_ret = gpio_install_isr_service(0);
    if (esp_ret != ESP_OK && esp_ret != ESP_ERR_INVALID_STATE) {
        printf("ERROR: Failed to install GPIO ISR service\r\n");
        return esp_ret;
    }
 
    esp_ret = gpio_isr_handler_add(TMAG5273_INT_GPIO, tmag5273_isr_handler, NULL);
    if (esp_ret != ESP_OK) {
        printf("ERROR: Failed to add INT GPIO ISR handler\r\n");
        return esp_ret;
    }

    return ESP_OK;
}

static esp_err_t tmag5273_read_reg(tmag5273_reg_t target_reg, uint8_t *out) {
    uint8_t r = (uint8_t)target_reg;
    esp_err_t esp_ret = i2c_master_transmit_receive(tmag5273, &r, 1, out, 1, -1);
    if (esp_ret != ESP_OK) {
        printf("ERROR: Failed to read register %d\r\n", target_reg);
    }
    return esp_ret;
}

esp_err_t tmag5273_read(tmag5273_measurement_t *measurement) {
    esp_err_t esp_ret;
    uint8_t msb, lsb;
 
    esp_ret = tmag5273_read_reg(X_MSB_RESULT, &msb);
    if (esp_ret != ESP_OK) return esp_ret;
    esp_ret = tmag5273_read_reg(X_LSB_RESULT, &lsb);
    if (esp_ret != ESP_OK) return esp_ret;
    measurement->x = (int16_t)((msb << 8) | lsb);
 
    esp_ret = tmag5273_read_reg(Y_MSB_RESULT, &msb);
    if (esp_ret != ESP_OK) return esp_ret;
    esp_ret = tmag5273_read_reg(Y_LSB_RESULT, &lsb);
    if (esp_ret != ESP_OK) return esp_ret;
    measurement->y = (int16_t)((msb << 8) | lsb);
 
    esp_ret = tmag5273_read_reg(Z_MSB_RESULT, &msb);
    if (esp_ret != ESP_OK) return esp_ret;
    esp_ret = tmag5273_read_reg(Z_LSB_RESULT, &lsb);
    if (esp_ret != ESP_OK) return esp_ret;
    measurement->z = (int16_t)((msb << 8) | lsb);
 
    uint8_t mag;
    esp_ret = tmag5273_read_reg(MAGNITUDE_RESULT, &mag);
    if (esp_ret != ESP_OK) return esp_ret;
    measurement->mag = (int16_t)mag;
 
    return ESP_OK;
}
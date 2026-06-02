#include "hall.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include "esp_log.h"

#include <Wire.h>
#include <stdatomic.h>

static const char *TAG = "hall";

// ── FreeRTOS spike task ───────────────────────────────────────
static TaskHandle_t s_spike_task_handle = NULL;
static int64_t      s_last_isr_time_us  = 0;

// ── Shared speed state (atomic — safe to read from Arduino loop) ─
static volatile atomic_uint_least32_t s_last_delta_us = 0;
static volatile atomic_uint_least32_t s_last_pulse_ms = 0;

// ─────────────────────────────────────────────────────────────
// ISR — fires on falling edge of HALL_INT_GPIO
// ─────────────────────────────────────────────────────────────
static void IRAM_ATTR hall_isr_handler(void *arg) {
    int64_t  now      = esp_timer_get_time();
    uint32_t delta_us = (s_last_isr_time_us == 0)
                        ? 0
                        : (uint32_t)(now - s_last_isr_time_us);
    s_last_isr_time_us = now;

    BaseType_t woken = pdFALSE;
    xTaskNotifyFromISR(s_spike_task_handle, delta_us,
                       eSetValueWithOverwrite, &woken);
    portYIELD_FROM_ISR(woken);
}

// ─────────────────────────────────────────────────────────────
// Spike task — woken by ISR, runs in normal task context
// ─────────────────────────────────────────────────────────────
static void hall_spike_task(void *arg) {
    uint32_t delta_us;
    while (1) {
        xTaskNotifyWait(0, 0, &delta_us, portMAX_DELAY);

        uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
        atomic_store(&s_last_pulse_ms, now_ms);

        if (delta_us == 0) {
            // First ever pulse — no period to compute yet
            ESP_LOGD(TAG, "First pulse — awaiting second for speed");
            continue;
        }

        atomic_store(&s_last_delta_us, delta_us);

        float period_s  = (float)delta_us * 1e-6f;
        float speed_kmh = (HALL_WHEEL_CIRCUMFERENCE_M / period_s) * 3.6f;
        ESP_LOGI(TAG, "Pulse: delta=%lu us  speed=%.2f km/h",
                 (unsigned long)delta_us, speed_kmh);
    }
}

// ─────────────────────────────────────────────────────────────
// Wire helpers
// ─────────────────────────────────────────────────────────────
static esp_err_t hall_write_reg(hall_reg_t reg, uint8_t value) {
    Wire.beginTransmission(HALL_I2C_ADDRESS);
    Wire.write((uint8_t)reg);
    Wire.write(value);
    uint8_t err = Wire.endTransmission();
    if (err != 0) {
        ESP_LOGE(TAG, "Wire write reg 0x%02X failed (%d)", reg, err);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t hall_read_reg(hall_reg_t reg, uint8_t *out) {
    Wire.beginTransmission(HALL_I2C_ADDRESS);
    Wire.write((uint8_t)reg);
    uint8_t err = Wire.endTransmission(false);   // repeated start
    if (err != 0) {
        ESP_LOGE(TAG, "Wire read reg 0x%02X failed (%d)", reg, err);
        return ESP_FAIL;
    }
    Wire.requestFrom((uint8_t)HALL_I2C_ADDRESS, (uint8_t)1);
    if (!Wire.available()) {
        ESP_LOGE(TAG, "Wire no data for reg 0x%02X", reg);
        return ESP_FAIL;
    }
    *out = Wire.read();
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────
// Device ID checks
// ─────────────────────────────────────────────────────────────
static esp_err_t hall_check_device_id(void) {
    uint8_t id;
    esp_err_t ret = hall_read_reg(DEVICE_ID, &id);
    if (ret != ESP_OK) return ret;
    if (id != 0x02) {
        ESP_LOGE(TAG, "Wrong device ID: got 0x%02X, expected 0x02", id);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t hall_check_manufacturer_id(void) {
    uint8_t msb, lsb;
    esp_err_t ret;
    ret = hall_read_reg(MANUFACTURER_ID_MSB, &msb); if (ret != ESP_OK) return ret;
    ret = hall_read_reg(MANUFACTURER_ID_LSB, &lsb); if (ret != ESP_OK) return ret;
    uint16_t id = ((uint16_t)msb << 8) | lsb;
    if (id != 0x5459) {
        ESP_LOGE(TAG, "Wrong manufacturer ID: got 0x%04X, expected 0x5459", id);
        return ESP_FAIL;
    }
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────
// hall_init
// ─────────────────────────────────────────────────────────────
esp_err_t hall_init(UBaseType_t task_priority) {
    esp_err_t ret;

    // ── 1. Create spike task before enabling the interrupt ────
    BaseType_t task_ret = xTaskCreate(
        hall_spike_task, "hall_spike",
        configMINIMAL_STACK_SIZE + 1024,
        NULL, task_priority,
        &s_spike_task_handle
    );
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create spike task");
        return ESP_ERR_NO_MEM;
    }

    // ── 2. Verify the device is present and correct ───────────
    ret = hall_check_device_id();      if (ret != ESP_OK) return ret;
    ret = hall_check_manufacturer_id(); if (ret != ESP_OK) return ret;

    // ── 3. Configure TMAG5273 registers ──────────────────────
    // SENSOR_CONFIG_1 = 0x07 : enable X, Y, Z channels
    ret = hall_write_reg(SENSOR_CONFIG_1, 0x07); if (ret != ESP_OK) return ret;

    // DEVICE_CONFIG_2 = 0x08 : continuous measurement mode
    ret = hall_write_reg(DEVICE_CONFIG_2, 0x08); if (ret != ESP_OK) return ret;

    // Z_THR_CONFIG : threshold value + enable bit
    ret = hall_write_reg(Z_THR_CONFIG, (HALL_Z_THRESHOLD << 1) | 0x01);
    if (ret != ESP_OK) return ret;

    // INT_CONFIG_1 = 0x24 : interrupt on threshold, active-low, push-pull
    ret = hall_write_reg(INT_CONFIG_1, 0x24); if (ret != ESP_OK) return ret;

    // ── 4. Configure INT GPIO and install ISR ─────────────────
    gpio_config_t io_cfg = {
        .pin_bit_mask = (1ULL << HALL_INT_GPIO),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_NEGEDGE,
    };
    ret = gpio_config(&io_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GPIO config failed");
        return ret;
    }

    // gpio_install_isr_service may already be called by Arduino core — that's fine
    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "ISR service install failed");
        return ret;
    }

    ret = gpio_isr_handler_add(HALL_INT_GPIO, hall_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ISR handler add failed");
        return ret;
    }

    ESP_LOGI(TAG, "TMAG5273 ready on GPIO %d", HALL_INT_GPIO);
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────
// Public getters
// ─────────────────────────────────────────────────────────────
float hall_get_speed_kmh(void) {
    uint32_t last_ms = atomic_load(&s_last_pulse_ms);
    if (last_ms == 0) return 0.0f;

    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if ((now_ms - last_ms) > HALL_SPEED_TIMEOUT_MS) return 0.0f;

    uint32_t delta_us = atomic_load(&s_last_delta_us);
    if (delta_us == 0) return 0.0f;

    float period_s = (float)delta_us * 1e-6f;
    return (HALL_WHEEL_CIRCUMFERENCE_M / period_s) * 3.6f;
}

uint32_t hall_get_last_delta_us(void) {
    return atomic_load(&s_last_delta_us);
}

esp_err_t hall_read(hall_measurement_t *out) {
    esp_err_t ret;
    uint8_t msb, lsb;

    ret = hall_read_reg(X_MSB_RESULT, &msb); if (ret != ESP_OK) return ret;
    ret = hall_read_reg(X_LSB_RESULT, &lsb); if (ret != ESP_OK) return ret;
    out->x = (int16_t)((msb << 8) | lsb);

    ret = hall_read_reg(Y_MSB_RESULT, &msb); if (ret != ESP_OK) return ret;
    ret = hall_read_reg(Y_LSB_RESULT, &lsb); if (ret != ESP_OK) return ret;
    out->y = (int16_t)((msb << 8) | lsb);

    ret = hall_read_reg(Z_MSB_RESULT, &msb); if (ret != ESP_OK) return ret;
    ret = hall_read_reg(Z_LSB_RESULT, &lsb); if (ret != ESP_OK) return ret;
    out->z = (int16_t)((msb << 8) | lsb);

    uint8_t mag;
    ret = hall_read_reg(MAGNITUDE_RESULT, &mag); if (ret != ESP_OK) return ret;
    out->mag = (int16_t)mag;

    return ESP_OK;
}
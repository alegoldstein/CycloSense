#include "hall.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include <Wire.h>

// ── Shared speed state ────────────────────────────────────────
static portMUX_TYPE      s_mux           = portMUX_INITIALIZER_UNLOCKED;
static volatile uint32_t s_last_delta_us = 0;
static volatile uint32_t s_last_pulse_ms = 0;
float g_circumference_m = HALL_WHEEL_CIRCUMFERENCE_M;

static inline void store_u32(volatile uint32_t *dst, uint32_t val) {
    portENTER_CRITICAL(&s_mux); *dst = val; portEXIT_CRITICAL(&s_mux);
}
static inline uint32_t load_u32(volatile uint32_t *src) {
    portENTER_CRITICAL(&s_mux); uint32_t v = *src; portEXIT_CRITICAL(&s_mux); return v;
}

// ─────────────────────────────────────────────────────────────
// Wire helpers
// ─────────────────────────────────────────────────────────────
static esp_err_t hall_write_reg(hall_reg_t reg, uint8_t value) {
    Wire.beginTransmission(HALL_I2C_ADDRESS);
    Wire.write((uint8_t)reg);
    Wire.write(value);
    if (Wire.endTransmission() != 0) {
        Serial.printf("[HALL] ERROR: write reg 0x%02X\n", reg);
        return ESP_FAIL;
    }
    return ESP_OK;
}

static esp_err_t hall_read_reg(hall_reg_t reg, uint8_t *out) {
    Wire.beginTransmission(HALL_I2C_ADDRESS);
    Wire.write((uint8_t)reg);
    if (Wire.endTransmission(false) != 0) return ESP_FAIL;
    Wire.requestFrom((uint8_t)HALL_I2C_ADDRESS, (uint8_t)1);
    if (!Wire.available()) return ESP_FAIL;
    *out = Wire.read();
    return ESP_OK;
}

static int16_t read_z(void) {
    uint8_t msb = 0, lsb = 0;
    hall_read_reg(Z_MSB_RESULT, &msb);
    hall_read_reg(Z_LSB_RESULT, &lsb);
    return (int16_t)((msb << 8) | lsb);
}

// ─────────────────────────────────────────────────────────────
// Software threshold polling task
//
// TMAG5273A2: 250 LSB/mT at ±133mT range
// 20mT threshold = 20 * 250 = 5000 LSB
//
// Debounce: once a spike is detected, ignore further crossings
// for DEBOUNCE_MS. At typical cycling speeds one spoke pass takes
// ~10-50ms so 200ms gives plenty of margin to reject double-fires
// while still allowing the next spoke to register correctly.
// ─────────────────────────────────────────────────────────────
static void hall_poll_task(void*) {
    const int16_t  THRESHOLD_LSB = 500;   
    const uint32_t DEBOUNCE_MS   = 125;    // min ms between valid spikes

    bool     above           = false;
    int64_t  last_cross_us   = 0;
    uint32_t last_trigger_ms = 0;

    Serial.printf("[HALL] Poll task ready — threshold=%.0f mT (%d LSB), debounce=%lu ms\n",
                  (float)THRESHOLD_LSB / 250.0f, THRESHOLD_LSB, (unsigned long)DEBOUNCE_MS);

    while (1) {
        int16_t z = read_z();
        // Serial.printf("[HALL] Z=%d (%.1f mT)\n", z, (float)z / 250.0f);
        bool    now_above = (z > THRESHOLD_LSB || z < -THRESHOLD_LSB);

        // Rising edge only (below → above threshold)
        if (now_above && !above) {
            uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);

            if ((now_ms - last_trigger_ms) >= DEBOUNCE_MS) {
                int64_t  now_us   = esp_timer_get_time();
                uint32_t delta_us = (last_cross_us == 0)
                                    ? 0
                                    : (uint32_t)(now_us - last_cross_us);
                last_cross_us   = now_us;
                last_trigger_ms = now_ms;

                store_u32(&s_last_pulse_ms, now_ms);

                if (delta_us == 0) {
                    Serial.printf("[HALL] Spike! Z=%d (%.1f mT) — first pulse\n",
                                  z, (float)z / 250.0f);
                } else {
                    store_u32(&s_last_delta_us, delta_us);
                    float mph = (g_circumference_m / ((float)delta_us * 1e-6f)) * 2.2374f;
                    Serial.printf("[HALL] Spike! Z=%d (%.1f mT)  delta=%lu ms  speed=%.2f mph\n",
                    z, (float)z / 250.0f,
                    (unsigned long)(delta_us / 1000), mph);
                }
            } 
        }

        above = now_above;
        vTaskDelay(pdMS_TO_TICKS(2));  // 500 Hz poll rate
    }
}

// ─────────────────────────────────────────────────────────────
// Raw diagnostic task — shows Z every 500ms
// ─────────────────────────────────────────────────────────────
// static void hall_raw_task(void*) {
//     while (1) {
//         int16_t z   = read_z();
//         Serial.printf("[RAW] Z=%6d (%.2f mT)\n", z, (float)z / 250.0f);
//         vTaskDelay(pdMS_TO_TICKS(500));
//     }
// }


// ─────────────────────────────────────────────────────────────
// hall_init
// ─────────────────────────────────────────────────────────────
esp_err_t hall_init(UBaseType_t task_priority) {
    esp_err_t ret;

    // Z channel only, continuous mode, default averaging
    ret = hall_write_reg(DEVICE_CONFIG_1, 0x00); if (ret != ESP_OK) return ret;
    ret = hall_write_reg(SENSOR_CONFIG_1, 0x40); if (ret != ESP_OK) return ret; // MAG_CH_EN=100b Z only
    ret = hall_write_reg(DEVICE_CONFIG_2, 0x02); if (ret != ESP_OK) return ret; // continuous mode
    vTaskDelay(pdMS_TO_TICKS(20));

    uint8_t rb;
    hall_read_reg(SENSOR_CONFIG_1, &rb); Serial.printf("[HALL] SENSOR_CONFIG_1=0x%02X (expect 0x40)\n", rb);
    hall_read_reg(DEVICE_CONFIG_2, &rb); Serial.printf("[HALL] DEVICE_CONFIG_2=0x%02X (expect 0x02)\n", rb);

    BaseType_t t1 = xTaskCreate(hall_poll_task, "hall_poll", 4096,
                                 NULL, task_priority, NULL);
    if (t1 != pdPASS) { Serial.println("[HALL] ERROR: poll task"); return ESP_ERR_NO_MEM; }

    //xTaskCreate(hall_raw_task, "hall_raw", 4096, NULL, task_priority - 1, NULL);

    Serial.println("[HALL] Init OK — software polling, 20mT threshold, 200ms debounce");
    return ESP_OK;
}

// ─────────────────────────────────────────────────────────────
// Public getters
// ─────────────────────────────────────────────────────────────
float hall_get_speed_kmh(void) {
    uint32_t last_ms = load_u32(&s_last_pulse_ms);
    if (last_ms == 0) return 0.0f;
    uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
    if ((now_ms - last_ms) > HALL_SPEED_TIMEOUT_MS) return 0.0f;
    uint32_t delta_us = load_u32(&s_last_delta_us);
    if (delta_us == 0) return 0.0f;
    return (g_circumference_m / ((float)delta_us * 1e-6f)) * 2.2374f;
}

uint32_t hall_get_last_delta_us(void) {
    return load_u32(&s_last_delta_us);
}

esp_err_t hall_read(hall_measurement_t *out) {
    uint8_t msb, lsb;
    hall_read_reg(X_MSB_RESULT, &msb); hall_read_reg(X_LSB_RESULT, &lsb);
    out->x = (int16_t)((msb << 8) | lsb);
    hall_read_reg(Y_MSB_RESULT, &msb); hall_read_reg(Y_LSB_RESULT, &lsb);
    out->y = (int16_t)((msb << 8) | lsb);
    hall_read_reg(Z_MSB_RESULT, &msb); hall_read_reg(Z_LSB_RESULT, &lsb);
    out->z = (int16_t)((msb << 8) | lsb);
    uint8_t mag; hall_read_reg(MAGNITUDE_RESULT, &mag);
    out->mag = (int16_t)mag;
    return ESP_OK;
}
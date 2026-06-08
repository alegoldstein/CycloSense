#pragma once

// hall.cpp/.h must be compiled as C++ (rename hall.c → hall.cpp) because
// Wire.h is a C++ header. The public API is wrapped in extern "C" so it
// can be called from plain C translation units if needed.

#include <stdint.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── I2C / Device config ───────────────────────────────────────
#define HALL_I2C_ADDRESS        0x35
#define HALL_I2C_SCL_SPEED      100000   // Hz — passed to Wire.setClock()

// GPIO that TMAG5273 INT pin is wired to (active-low, falling edge)
#define HALL_INT_GPIO           4

// TMAG5273 Z-axis threshold (6-bit value written to Z_THR_CONFIG)
#define HALL_Z_THRESHOLD        0x02   // 2.06 mT — above noise (0.7mT), below spoke magnet peak (~4mT)

// ── Wheel geometry ────────────────────────────────────────────
// One TMAG5273 interrupt fires per spoke magnet pass = one wheel revolution.
// Set to your actual wheel circumference in metres.
#define HALL_WHEEL_CIRCUMFERENCE_M  2.18f   // 700x35C
extern float g_circumference_m;

// Zero speed after this many ms without a pulse
#define HALL_SPEED_TIMEOUT_MS   3000

// ── TMAG5273 register map ─────────────────────────────────────
typedef enum {
    DEVICE_CONFIG_1      = 0x00,
    DEVICE_CONFIG_2      = 0x01,
    SENSOR_CONFIG_1      = 0x02,
    SENSOR_CONFIG_2      = 0x03,
    X_THR_CONFIG         = 0x04,
    Y_THR_CONFIG         = 0x05,
    Z_THR_CONFIG         = 0x06,
    T_CONFIG             = 0x07,
    INT_CONFIG_1         = 0x08,
    MAG_GAIN_CONFIG      = 0x09,
    MAG_OFFSET_CONFIG_1  = 0x0A,
    MAG_OFFSET_CONFIG_2  = 0x0B,
    I2C_ADDRESS_REG      = 0x0C,
    DEVICE_ID            = 0x0D,
    MANUFACTURER_ID_LSB  = 0x0E,
    MANUFACTURER_ID_MSB  = 0x0F,
    T_MSB_RESULT         = 0x10,
    T_LSB_RESULT         = 0x11,
    X_MSB_RESULT         = 0x12,
    X_LSB_RESULT         = 0x13,
    Y_MSB_RESULT         = 0x14,
    Y_LSB_RESULT         = 0x15,
    Z_MSB_RESULT         = 0x16,
    Z_LSB_RESULT         = 0x17,
    CONV_STATUS          = 0x18,
    ANGLE_RESULT_MSB     = 0x19,
    ANGLE_RESULT_LSB     = 0x1A,
    MAGNITUDE_RESULT     = 0x1B,
    DEVICE_STATUS        = 0x1C
} hall_reg_t;

// ── Measurement struct ────────────────────────────────────────
typedef struct {
    int16_t x;
    int16_t y;
    int16_t z;
    int16_t mag;
} hall_measurement_t;

// ── Public API ────────────────────────────────────────────────

/**
 * Initialise the TMAG5273 over Wire (I2C) and install the GPIO interrupt
 * that fires each time the spoke magnet passes the sensor.
 *
 * Call Wire.begin(sda, scl) and Wire.setClock() BEFORE calling hall_init().
 *
 * @param task_priority  FreeRTOS priority for the internal spike handler task.
 * @return ESP_OK on success, ESP_FAIL on device ID mismatch or I2C error.
 */
esp_err_t hall_init(UBaseType_t task_priority);

/**
 * Return the most recently computed wheel speed in km/h.
 * Returns 0.0 if no pulse has been received or the wheel has stopped.
 * Safe to call from any task or the Arduino loop.
 */
float hall_get_speed_kmh(void);

/**
 * Return the raw revolution period of the last pulse in microseconds.
 * Returns 0 if no measurement is available yet.
 */
uint32_t hall_get_last_delta_us(void);

/**
 * Read raw X/Y/Z/magnitude from the TMAG5273.
 * Useful for diagnostics; not required for speed measurement.
 */
esp_err_t hall_read(hall_measurement_t *out);

#ifdef __cplusplus
}
#endif
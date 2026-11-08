/*
 * utils_hdc1080.h
 *
 *      Author: Kishan K. Mandal
 */

#ifndef UTILS_UTILS_HDC1080_H_
#define UTILS_UTILS_HDC1080_H_

#include "main.h"
#include "hdc1080.h"
#include <stdbool.h>
/**
 * @brief Initializes the HDC1080 sensor hardware and creates the FreeRTOS
 *        I2C1 bus protection mutex.
 * @note  Must be called in main() AFTER MX_I2C1_Init() and BEFORE
 *        vTaskStartScheduler().
 */
void init_HDC1080(void);

/**
 * @brief Thread-safe wrapper to trigger and read temperature in Celsius.
 * @param[out] temperature Pointer to float where the temperature reading will be stored.
 * @return true if the I2C transaction succeeded (HAL_OK), false on error or NULL pointer.
 */
bool get_Temp_data(float *temperature);

/**
 * @brief Thread-safe wrapper to trigger and read relative humidity (%RH).
 * @param[out] humidity Pointer to float where the humidity reading will be stored.
 * @return true if the I2C transaction succeeded (HAL_OK), false on error or NULL pointer.
 */
bool get_Humidity_data(float *humidity);

#endif /* UTILS_UTILS_HDC1080_H_ */

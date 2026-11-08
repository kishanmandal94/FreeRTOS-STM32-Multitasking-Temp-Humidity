/*
 * utils_oled.h
 *
 *      Author: Kishan K. Mandal
 */

#ifndef UTILS_UTILS_OLED_H_
#define UTILS_UTILS_OLED_H_

#include <stdint.h>
#include "ssd1306.h"
#include "ssd1306_fonts.h"
#include <stdio.h>


/**
 * @brief Initializes the SSD1306 controller over I2C3 and displays a boot screen.
 */
void init_OLED(void);

/**
 * @brief Clears the screen and prints a single string at the specified coordinates.
 * @param[in] x   Horizontal pixel coordinate (0 to 127).
 * @param[in] y   Vertical pixel coordinate (0 to 63).
 * @param[in] str Null-terminated ASCII string to render.
 */
void display_string(uint8_t x, uint8_t y, const char *str);

/**
 * @brief Formats and renders live temperature and humidity telemetry onto the OLED.
 * @param[in] temperature Latest temperature reading in degrees Celsius.
 * @param[in] humidity    Latest relative humidity reading in %RH.
 */
void display_sensor_data(float temperature, float humidity);

#endif /* UTILS_UTILS_OLED_H_ */

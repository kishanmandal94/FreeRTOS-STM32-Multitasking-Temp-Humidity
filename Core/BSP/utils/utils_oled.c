/*
 * utils_oled.c
 *
 *      Author: Kishan K. Mandal
 *
 *  @brief High-level display wrapper implementation for the SSD1306 OLED.
 *         Abstracts frame-buffer manipulation, cursor positioning, and
 *         floating-point string formatting away from the FreeRTOS tasks.
 */

#include "utils_oled.h"

/**
 * @brief Initializes the SSD1306 display and renders the initial startup splash.
 */
void init_OLED(void)
{
    /* Initialize SSD1306 registers over I2C3 (uses 100ms startup delay) */
    ssd1306_Init();

    /* Clear local 1024-byte RAM frame buffer */
    ssd1306_Fill(Black);

    /* Draw startup banner and push buffer to physical screen */
    ssd1306_SetCursor(2, 0);
    ssd1306_WriteString("System Boot...", Font_7x10, White);
    ssd1306_UpdateScreen();
}

/**
 * @brief Renders a custom text string at (x, y) using Font_7x10.
 */
void display_string(uint8_t x, uint8_t y, const char *str)
{
    if (str == NULL) return;

    ssd1306_Fill(Black);
    ssd1306_SetCursor(x, y);
    ssd1306_WriteString((char*)str, Font_7x10, White);
    ssd1306_UpdateScreen();
}

/**
 * @brief Renders the split-screen telemetry UI with header, divider line,
 *        and 1-decimal-place float values for Temperature and Humidity.
 * @note  Requires '-u _printf_float' enabled in MCU GCC Linker settings.
 */
void display_sensor_data(float temperature, float humidity)
{
    char oled_buf[32];

    /* 1. Wipe the local RAM frame buffer before drawing the new frame */
    ssd1306_Fill(Black);

    /* 2. Draw Header Title and horizontal separator line at Y = 12 */
    ssd1306_SetCursor(2, 0);
    ssd1306_WriteString("HDC1080 RTOS Log", Font_7x10, White);
    ssd1306_Line(0, 12, SSD1306_WIDTH - 1, 12, White);

    /* 3. Line 1: Format and draw Temperature in 11x18 font */
    snprintf(oled_buf, sizeof(oled_buf), "T: %.1f C", temperature);
    ssd1306_SetCursor(2, 18);
    ssd1306_WriteString(oled_buf, Font_11x18, White);

    /* 4. Line 2: Format and draw Humidity in 11x18 font (%% escapes the % sign) */
    snprintf(oled_buf, sizeof(oled_buf), "H: %.1f %%", humidity);
    ssd1306_SetCursor(2, 40);
    ssd1306_WriteString(oled_buf, Font_11x18, White);

    /* 5. Flush all 8 pages of the RAM buffer to the OLED over I2C3 (400 kHz) */
    ssd1306_UpdateScreen();
}

/*
 * hdc1080.c
 *
 
 *      Author: Kishan K. Mandal
 *
 *  @brief STM32 F4 compliant library for the HDC1080 sensor.

 */

#ifndef _HDC1080_H_
#define _HDC1080_H_

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* 7-bit I2C Address is 0x40; STM32 HAL requires left-shifted 8-bit address (0x80) */
#define HDC1080_I2C_ADDR            (0x40U << 1U)

/* HDC1080 Register Map */
#define HDC1080_REG_TEMP            0x00U
#define HDC1080_REG_HUMIDITY        0x01U
#define HDC1080_REG_CONFIG          0x02U
#define HDC1080_REG_SERIAL_ID1      0xFBU
#define HDC1080_REG_SERIAL_ID2      0xFCU
#define HDC1080_REG_SERIAL_ID3      0xFDU
#define HDC1080_REG_MANUF_ID        0xFEU
#define HDC1080_REG_DEVICE_ID       0xFFU

/* Expected Factory Identification Values */
#define HDC1080_MANUF_ID_VAL        0x5449U /* "TI" in ASCII */
#define HDC1080_DEV_ID_VAL          0x1050U /* HDC1080 */
#define HDC1000_DEV_ID_VAL          0x1000U /* HDC1000 compatibility */

/* Configuration Register Bit Definitions */
#define HDC1080_CFG_RST             (1U << 15)
#define HDC1080_CFG_HEAT_EN         (1U << 13)
#define HDC1080_CFG_MODE_SEQ        (1U << 12) /* 0 = Individual, 1 = Sequential */

/* Timeout and Delay Constants (in ms) */
#define HDC1080_I2C_TIMEOUT         100U
#define HDC1080_STARTUP_DELAY_MS    15U
#define HDC1080_CONV_DELAY_MS       15U /* Covers 6.50ms max 14-bit conversion + SysTick margin */

/* Temperature Resolution Options (Bit 10) */
typedef enum {
    HDC1080_TEMP_RES_14BIT = (0U << 10),
    HDC1080_TEMP_RES_11BIT = (1U << 10)
} HDC1080_TempRes_t;

/* Humidity Resolution Options (Bits 9:8) */
typedef enum {
    HDC1080_HUM_RES_14BIT  = (0U << 8),
    HDC1080_HUM_RES_11BIT  = (1U << 8),
    HDC1080_HUM_RES_8BIT   = (2U << 8)
} HDC1080_HumRes_t;

/* Sensor Handle Structure */
typedef struct {
    I2C_HandleTypeDef *hi2c;
    HDC1080_TempRes_t  temp_res;
    HDC1080_HumRes_t   hum_res;
} HDC1080_HandleTypeDef;

/* Public Function Prototypes */
HAL_StatusTypeDef HDC1080_Init(HDC1080_HandleTypeDef *hhdc,
                               I2C_HandleTypeDef *hi2c,
                               HDC1080_TempRes_t temp_res,
                               HDC1080_HumRes_t hum_res);

HAL_StatusTypeDef HDC1080_ReadTemperature(HDC1080_HandleTypeDef *hhdc, float *temperature_c);
HAL_StatusTypeDef HDC1080_ReadHumidity(HDC1080_HandleTypeDef *hhdc, float *humidity_rh);
	
	
#endif /* _HDC1080_H_ */

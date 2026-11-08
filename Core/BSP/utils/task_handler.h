/*
 * task_handler.h
 *
 *      Author: Kishan K. Mandal
 *
 *  @brief FreeRTOS application task prototypes, inter-task message queue data
 *         structures, and UART debug telemetry helpers.
 */

#ifndef INC_TASK_HANDLER_H_
#define INC_TASK_HANDLER_H_

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "utils_hdc1080.h"
#include "utils_oled.h"

/* External handles defined in main.c */
extern UART_HandleTypeDef huart2;
extern QueueHandle_t xSensorQueue;

/**
 * @brief Enumeration to tag which physical quantity a queue message holds.
 */
typedef enum {
    SENSOR_TYPE_TEMP,
    SENSOR_TYPE_HUMIDITY
} SensorType_t;

/**
 * @brief Tagged message structure passed from Producer Tasks (Temp/Humidity)
 *        into xSensorQueue for the Consumer Task (Display/Logger).
 */
typedef struct {
    SensorType_t type;  /**< Identifies SENSOR_TYPE_TEMP or SENSOR_TYPE_HUMIDITY */
    float value;        /**< Engineering unit value (Celsius or %RH) */
} SensorMsg_t;

/* ------ Methods for FreeRTOS Sensor & Display Tasks --------- */
void vTempTask(void *pvParameters);
void vHumidityTask(void *pvParameters);
void vDisplayTask(void *pvParameters);

/* ------- Methods for UART Debugging ------------------------- */
void uprintf_data(const char *str, float data);

#endif /* INC_TASK_HANDLER_H_ */

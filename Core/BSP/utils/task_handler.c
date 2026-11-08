/*
 * task_handler.c
 *
 *      Author: Kishan K. Mandal
 *
 *  @brief Implementation of periodic FreeRTOS tasks using vTaskDelayUntil()
 *         and producer-consumer synchronization via a FreeRTOS Message Queue.
 */

#include "task_handler.h"

/**
 * @brief Formats and transmits a labeled floating-point value over USART2.
 * @param[in] str  Label prefix string (e.g., "Temp (C)").
 * @param[in] data Floating-point sensor value.
 */
void uprintf_data(const char *str, float data)
{
    char msg[50];
    int len = snprintf(msg, sizeof(msg), "%s: %.1f\r\n", str, data);
    if (len > 0) {
        HAL_UART_Transmit(&huart2, (uint8_t*)msg, (uint16_t)strlen(msg), HAL_MAX_DELAY);
    }
}

/**
 * @brief 1. Temperature Producer Task (Priority 3, Period = 1000 ms).
 *        Acquires temperature via the mutex-protected HDC1080 wrapper and
 *        pushes tagged readings into xSensorQueue.
 */
void vTempTask(void *pvParameters)
{

    /* Capture initial tick count for drift-free periodic execution */
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(1000);

    SensorMsg_t msg = {
        .type  = SENSOR_TYPE_TEMP,
        .value = 0.0f
    };

    for (;;) {
        /* Read temperature (thread-safe). Only enqueue if I2C read succeeded */
        if (get_Temp_data(&msg.value)) {
            xQueueSend(xSensorQueue, &msg, 0);
        }

        /* Sleep until exact 1000 ms period elapses (absorbs 15ms HAL_Delay) */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/**
 * @brief 2. Humidity Producer Task (Priority 2, Period = 1000 ms).
 *        Acquires relative humidity via the mutex-protected HDC1080 wrapper
 *        and pushes tagged readings into xSensorQueue.
 */
void vHumidityTask(void *pvParameters)
{

    /* Capture initial tick count for drift-free periodic execution */
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(1000);

    SensorMsg_t msg = {
        .type  = SENSOR_TYPE_HUMIDITY,
        .value = 0.0f
    };

    for (;;) {
        /* Read humidity (thread-safe). Only enqueue if I2C read succeeded */
        if (get_Humidity_data(&msg.value)) {
            xQueueSend(xSensorQueue, &msg, 0);
        }

        /* Sleep until exact 1000 ms period elapses (absorbs 15ms HAL_Delay) */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

/**
 * @brief 3. Display & Logger Consumer Task (Priority 1, Period = 500 ms).
 *        Drains all pending sensor messages from xSensorQueue, caches the most
 *        recent readings, and updates the OLED (and optional UART log) once
 *        both sensors have reported at least one valid measurement.
 */
void vDisplayTask(void *pvParameters)
{

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xPeriod = pdMS_TO_TICKS(500);

    SensorMsg_t received_msg;

    /* Local state cache so the screen always shows the newest reading alongside
     * the other quantity's last known value */
    float latest_temp = 0.0f;
    float latest_hum  = 0.0f;

    /* Startup synchronization flags: prevent displaying 0.0 before first read */
    bool has_temp = false;
    bool has_hum  = false;

    for (;;) {
        /* Non-blocking drain of all queued messages accumulated since last wake */
        while (xQueueReceive(xSensorQueue, &received_msg, 0) == pdPASS) {
            if (received_msg.type == SENSOR_TYPE_TEMP) {
                latest_temp = received_msg.value;
                has_temp = true;
            } else if (received_msg.type == SENSOR_TYPE_HUMIDITY) {
                latest_hum = received_msg.value;
                has_hum = true;
            }
        }

        /* Only refresh the OLED once both sensors have delivered valid data */
        if (has_temp && has_hum) {
            display_sensor_data(latest_temp, latest_hum);

        #if USE_USART_DEBUG
            uprintf_data("Temp (C)", latest_temp);
            uprintf_data("Hum (%)", latest_hum);
        #endif
        }

        /* Wait until exact 500 ms period elapses */
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}

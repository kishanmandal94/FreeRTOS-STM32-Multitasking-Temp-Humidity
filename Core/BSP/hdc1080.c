#include "hdc1080.h"

/* Private Helper Function to Read 16-bit Non-Measurement Registers */
static HAL_StatusTypeDef HDC1080_ReadRegister(HDC1080_HandleTypeDef *hhdc, uint8_t reg, uint16_t *data)
{
    HAL_StatusTypeDef status;
    uint8_t rx_buf[2] = {0};

    status = HAL_I2C_Master_Transmit(hhdc->hi2c, HDC1080_I2C_ADDR, &reg, 1U, HDC1080_I2C_TIMEOUT);
    if (status != HAL_OK) {
        return status;
    }

    status = HAL_I2C_Master_Receive(hhdc->hi2c, HDC1080_I2C_ADDR, rx_buf, 2U, HDC1080_I2C_TIMEOUT);
    if (status != HAL_OK) {
        return status;
    }

    /* HDC1080 transmits MSB first */
    *data = ((uint16_t)rx_buf[0] << 8U) | (uint16_t)rx_buf[1];
    return HAL_OK;
}

/**
  * @brief  Initializes the HDC1080 sensor, verifies IDs, and sets resolution.
  * @param  hhdc: Pointer to HDC1080 handle structure
  * @param  hi2c: Pointer to STM32 HAL I2C handle (e.g., &hi2c1)
  * @param  temp_res: Temperature measurement resolution (11-bit or 14-bit)
  * @param  hum_res: Humidity measurement resolution (8-bit, 11-bit, or 14-bit)
  * @retval HAL_StatusTypeDef (HAL_OK on success, HAL_ERROR if ID mismatch)
  */
HAL_StatusTypeDef HDC1080_Init(HDC1080_HandleTypeDef *hhdc,
                               I2C_HandleTypeDef *hi2c,
                               HDC1080_TempRes_t temp_res,
                               HDC1080_HumRes_t hum_res)
{
    if ((hhdc == NULL) || (hi2c == NULL)) {
        return HAL_ERROR;
    }

    hhdc->hi2c     = hi2c;
    hhdc->temp_res = temp_res;
    hhdc->hum_res  = hum_res;

    /* Datasheet Section 8.5: Wait >= 15ms after power-up for sensor readiness */
    HAL_Delay(HDC1080_STARTUP_DELAY_MS);
    /* Poll sensor up to 5 times to ensure I2C state machine is synced and awake */
    if (HAL_I2C_IsDeviceReady(hhdc->hi2c, HDC1080_I2C_ADDR, 5U, HDC1080_I2C_TIMEOUT) != HAL_OK) {
        return HAL_ERROR;
    }
    /* 1. Verify Manufacturer ID (0x5449) */
    uint16_t manuf_id = 0;
    if (HDC1080_ReadRegister(hhdc, HDC1080_REG_MANUF_ID, &manuf_id) != HAL_OK) {
        return HAL_ERROR;
    }
    if (manuf_id != HDC1080_MANUF_ID_VAL) {
        return HAL_ERROR;
    }

    /* 2. Verify Device ID (0x1050 for HDC1080, 0x1000 for HDC1000) */
    uint16_t dev_id = 0;
    if (HDC1080_ReadRegister(hhdc, HDC1080_REG_DEVICE_ID, &dev_id) != HAL_OK) {
        return HAL_ERROR;
    }
    if ((dev_id != HDC1080_DEV_ID_VAL) && (dev_id != HDC1000_DEV_ID_VAL)) {
        return HAL_ERROR;
    }

    /* 3. Perform Software Reset */
    uint16_t rst_config = HDC1080_CFG_RST;
    uint8_t tx_buf[3];
    tx_buf[0] = HDC1080_REG_CONFIG;
    tx_buf[1] = (uint8_t)((rst_config >> 8U) & 0xFFU);
    tx_buf[2] = (uint8_t)(rst_config & 0xFFU);

    if (HAL_I2C_Master_Transmit(hhdc->hi2c, HDC1080_I2C_ADDR, tx_buf, 3U, HDC1080_I2C_TIMEOUT) != HAL_OK) {
        return HAL_ERROR;
    }
    HAL_Delay(HDC1080_STARTUP_DELAY_MS);

    /* 4. Configure Acquisition Mode (MODE = 0 for independent reads) & Resolutions */
    uint16_t config_reg = (uint16_t)temp_res | (uint16_t)hum_res; /* Heater off, Mode = 0 */
    tx_buf[0] = HDC1080_REG_CONFIG;
    tx_buf[1] = (uint8_t)((config_reg >> 8U) & 0xFFU);
    tx_buf[2] = (uint8_t)(config_reg & 0xFFU);

    return HAL_I2C_Master_Transmit(hhdc->hi2c, HDC1080_I2C_ADDR, tx_buf, 3U, HDC1080_I2C_TIMEOUT);
}

/**
  * @brief  Triggers and reads temperature in degrees Celsius.
  * @param  hhdc: Pointer to HDC1080 handle structure
  * @param  temperature_c: Pointer to float where temperature in Celsius will be stored
  * @retval HAL_StatusTypeDef
  */
HAL_StatusTypeDef HDC1080_ReadTemperature(HDC1080_HandleTypeDef *hhdc, float *temperature_c)
{
    if ((hhdc == NULL) || (temperature_c == NULL)) {
        return HAL_ERROR;
    }

    HAL_StatusTypeDef status;
    uint8_t reg = HDC1080_REG_TEMP;
    uint8_t rx_buf[2] = {0};

    /* Step 1: Point to Temperature Register (0x00) to trigger conversion */
    status = HAL_I2C_Master_Transmit(hhdc->hi2c, HDC1080_I2C_ADDR, &reg, 1U, HDC1080_I2C_TIMEOUT);
    if (status != HAL_OK) {
        return status;
    }

    /* Step 2: Wait for conversion to complete (6.35ms typ for 14-bit) */
    HAL_Delay(HDC1080_CONV_DELAY_MS);

    /* Step 3: Read 2 bytes of conversion data (MSB first) */
    status = HAL_I2C_Master_Receive(hhdc->hi2c, HDC1080_I2C_ADDR, rx_buf, 2U, HDC1080_I2C_TIMEOUT);
    if (status != HAL_OK) {
        return status;
    }

    uint16_t raw_temp = ((uint16_t)rx_buf[0] << 8U) | (uint16_t)rx_buf[1];

    /* Step 4: Convert raw value to Celsius per datasheet formula */
    *temperature_c = (((float)raw_temp / 65536.0f) * 165.0f) - 40.0f;

    return HAL_OK;
}

/**
  * @brief  Triggers and reads relative humidity in percentage (%RH).
  * @param  hhdc: Pointer to HDC1080 handle structure
  * @param  humidity_rh: Pointer to float where relative humidity (%) will be stored
  * @retval HAL_StatusTypeDef
  */
HAL_StatusTypeDef HDC1080_ReadHumidity(HDC1080_HandleTypeDef *hhdc, float *humidity_rh)
{
    if ((hhdc == NULL) || (humidity_rh == NULL)) {
        return HAL_ERROR;
    }

    HAL_StatusTypeDef status;
    uint8_t reg = HDC1080_REG_HUMIDITY;
    uint8_t rx_buf[2] = {0};

    /* Step 1: Point to Humidity Register (0x01) to trigger conversion */
    status = HAL_I2C_Master_Transmit(hhdc->hi2c, HDC1080_I2C_ADDR, &reg, 1U, HDC1080_I2C_TIMEOUT);
    if (status != HAL_OK) {
        return status;
    }

    /* Step 2: Wait for conversion to complete (6.50ms typ for 14-bit) */
    HAL_Delay(HDC1080_CONV_DELAY_MS);

    /* Step 3: Read 2 bytes of conversion data (MSB first) */
    status = HAL_I2C_Master_Receive(hhdc->hi2c, HDC1080_I2C_ADDR, rx_buf, 2U, HDC1080_I2C_TIMEOUT);
    if (status != HAL_OK) {
        return status;
    }

    uint16_t raw_hum = ((uint16_t)rx_buf[0] << 8U) | (uint16_t)rx_buf[1];

    /* Step 4: Convert raw value to %RH per datasheet formula */
    *humidity_rh = ((float)raw_hum / 65536.0f) * 100.0f;

    return HAL_OK;
}

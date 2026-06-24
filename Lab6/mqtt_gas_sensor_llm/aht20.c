/**
 * AHT20 I2C driver for Lab6
 * I2C addr: 0x38, Calibrate: [0xE1,0x08,0x00], Measure: [0xAC,0x33,0x00]
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "aht20.h"
#include "iot_i2c.h"
#include "hi_time.h"

#define AHT20_I2C_ADDR  0x38
#define AHT20_I2C_IDX   IOT_I2C_IDX_0

static uint32_t AHT20_Write(const uint8_t *data, uint32_t len)
{
    return IoTI2cWrite(AHT20_I2C_IDX, AHT20_I2C_ADDR, data, len);
}

static uint32_t AHT20_Read(uint8_t *data, uint32_t len)
{
    return IoTI2cRead(AHT20_I2C_IDX, AHT20_I2C_ADDR, data, len);
}

static uint8_t AHT20_Status(void)
{
    uint8_t s = 0;
    AHT20_Read(&s, 1);
    return s;
}

static uint32_t AHT20_WaitIdle(uint32_t timeout_ms)
{
    uint32_t i;
    for (i = 0; i < timeout_ms; i++) {
        if ((AHT20_Status() & 0x80) == 0) return 0;
        usleep(1000);
    }
    return 1;
}

uint32_t AHT20_Calibrate(void)
{
    IoTI2cInit(AHT20_I2C_IDX, 400000);
    uint8_t cmd[3] = {0xE1, 0x08, 0x00};
    uint32_t ret = AHT20_Write(cmd, 3);
    if (ret != 0) return ret;
    usleep(40000);
    uint8_t status = AHT20_Status();
    printf("[AHT20] Calibrate status: 0x%02X\r\n", status);
    return 0;
}

uint32_t AHT20_StartMeasure(void)
{
    uint8_t cmd[3] = {0xAC, 0x33, 0x00};
    uint32_t ret = AHT20_Write(cmd, 3);
    if (ret != 0) return ret;
    usleep(80000);
    if (AHT20_WaitIdle(100) != 0) return 1;
    return 0;
}

uint32_t AHT20_GetMeasureResult(float *temp, float *humi)
{
    uint8_t data[6] = {0};
    uint32_t ret = AHT20_Read(data, 6);
    if (ret != 0) return ret;
    if (data[0] & 0x80) return 1;

    uint32_t rh = ((uint32_t)data[1] << 12) | ((uint32_t)data[2] << 4) | ((uint32_t)data[3] >> 4);
    uint32_t rt = (((uint32_t)data[3] & 0x0F) << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[5];

    *humi = (float)rh / 1048576.0f * 100.0f;
    *temp = (float)rt / 1048576.0f * 200.0f - 50.0f;
    return 0;
}

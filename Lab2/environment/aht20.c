/**
 * AHT20 温湿度传感器 I2C 驱动实现 / AHT20 I2C Driver
 *
 * 通信协议:
 *   校准: 写 [0xE1, 0x08, 0x00] -> 等40ms -> 读状态字节
 *   测量: 写 [0xAC, 0x33, 0x00] -> 等80ms -> 读6字节数据
 *   解析: humi = (raw[1]<<12 | raw[2]<<4 | raw[3]>>4) / 1048576.0 * 100
 *         temp = ((raw[3]&0x0F)<<16 | raw[4]<<8 | raw[5]) / 1048576.0 * 200 - 50
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "aht20.h"
#include "iot_i2c.h"
#include "hi_io.h"
#include "hi_stdlib.h"
#include "hi_time.h"

/* AHT20 I2C 地址 (7-bit) */
#define AHT20_I2C_ADDR   0x38
#define AHT20_I2C_IDX    IOT_I2C_IDX_0

/* AHT20 命令 */
#define AHT20_CMD_CALIBRATE    0xE1
#define AHT20_CMD_MEASURE      0xAC

/**
 * I2C 写数据到 AHT20
 */
static uint32_t AHT20_I2cWrite(const uint8_t *data, uint32_t len)
{
    return IoTI2cWrite(AHT20_I2C_IDX, AHT20_I2C_ADDR, data, len);
}

/**
 * I2C 从 AHT20 读数据
 */
static uint32_t AHT20_I2cRead(uint8_t *data, uint32_t len)
{
    return IoTI2cRead(AHT20_I2C_IDX, AHT20_I2C_ADDR, data, len);
}

/**
 * 读取 AHT20 状态字节
 */
static uint8_t AHT20_ReadStatus(void)
{
    uint8_t status = 0;
    AHT20_I2cRead(&status, 1);
    return status;
}

/**
 * 等待 AHT20 测量完成 (busy bit 清零)
 */
static uint32_t AHT20_WaitIdle(uint32_t timeout_ms)
{
    uint32_t i;
    for (i = 0; i < timeout_ms; i++) {
        uint8_t status = AHT20_ReadStatus();
        if ((status & 0x80) == 0) {
            return 0;
        }
        usleep(1000);
    }
    return 1;
}

uint32_t AHT20_Calibrate(void)
{
    /* 初始化 I2C0, 波特率 400000 */
    IoTI2cInit(AHT20_I2C_IDX, 400000);

    /* 发送校准命令: [0xE1, 0x08, 0x00] */
    uint8_t cmd[3] = { AHT20_CMD_CALIBRATE, 0x08, 0x00 };
    uint32_t ret = AHT20_I2cWrite(cmd, 3);
    if (ret != 0) {
        printf("[AHT20] Calibrate write failed: %u\r\n", ret);
        return ret;
    }

    usleep(40000);

    /* 读取状态 */
    uint8_t status = AHT20_ReadStatus();
    printf("[AHT20] Status after calibrate: 0x%02X\r\n", status);

    if ((status & 0x08) == 0) {
        printf("[AHT20] WARNING: Calibrate bit not set\r\n");
    }

    return 0;
}

uint32_t AHT20_StartMeasure(void)
{
    /* 发送触发测量命令: [0xAC, 0x33, 0x00] */
    uint8_t cmd[3] = { AHT20_CMD_MEASURE, 0x33, 0x00 };
    uint32_t ret = AHT20_I2cWrite(cmd, 3);
    if (ret != 0) {
        printf("[AHT20] StartMeasure write failed: %u\r\n", ret);
        return ret;
    }

    usleep(80000);

    if (AHT20_WaitIdle(100) != 0) {
        printf("[AHT20] Measure timeout\r\n");
        return 1;
    }

    return 0;
}

uint32_t AHT20_GetMeasureResult(float *temp, float *humi)
{
    uint8_t data[6] = {0};
    uint32_t ret = AHT20_I2cRead(data, 6);
    if (ret != 0) {
        printf("[AHT20] Read data failed: %u\r\n", ret);
        return ret;
    }

    if (data[0] & 0x80) {
        printf("[AHT20] Sensor still busy\r\n");
        return 1;
    }

    /* 解析原始数据 */
    uint32_t raw_humi = ((uint32_t)data[1] << 12) |
                        ((uint32_t)data[2] << 4) |
                        ((uint32_t)data[3] >> 4);
    uint32_t raw_temp = (((uint32_t)data[3] & 0x0F) << 16) |
                        ((uint32_t)data[4] << 8) |
                        ((uint32_t)data[5]);

    *humi = (float)raw_humi / 1048576.0f * 100.0f;
    *temp = (float)raw_temp / 1048576.0f * 200.0f - 50.0f;

    return 0;
}

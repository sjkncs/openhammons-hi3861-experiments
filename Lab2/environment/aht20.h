/**
 * AHT20 温湿度传感器驱动头文件 / AHT20 Driver Header
 *
 * I2C 接口: SDA=GPIO13, SCL=GPIO14, I2C0, 400kHz
 * I2C 地址: 0x38 (7-bit)
 */

#ifndef AHT20_H
#define AHT20_H

#include <stdint.h>

/**
 * 校准 AHT20 传感器
 * @return 0=成功, 非0=失败
 */
uint32_t AHT20_Calibrate(void);

/**
 * 触发一次温湿度测量
 * @return 0=成功, 非0=失败
 */
uint32_t AHT20_StartMeasure(void);

/**
 * 获取测量结果
 * @param temp  温度输出指针 (°C)
 * @param humi  湿度输出指针 (%)
 * @return 0=成功, 非0=失败
 */
uint32_t AHT20_GetMeasureResult(float *temp, float *humi);

#endif /* AHT20_H */

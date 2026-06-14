/**
 * MQ-2 可燃气体传感器驱动
 *
 * 传感器接口文件，实现 GetGasLevel() 函数
 * 返回值: 传感器电阻值 (kΩ)，浓度越高电阻越低
 */

#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "iot_gpio.h"
#include "hi_io.h"
#include "hi_adc.h"

// ADC5 通道对应 MQ-2 传感器
#define GAS_SENSOR_ADC_CHANNEL  HI_ADC_CHANNEL_5

/**
 * 将 ADC 原始值转换为电压值
 * Hi3861 ADC: 12位精度, 参考电压 1.8V, 满量程 4096
 * 硬件上 ADC 引脚前有 4x 模拟增益
 */
static float AdcToVoltage(unsigned short raw)
{
    return (float)raw * 1.8f * 4.0f / 4096.0f;
}

/**
 * 获取可燃气体浓度
 *
 * 原理: MQ-2 传感器串联 1kΩ 电阻接在 5V 和 GND 之间
 * ADC 采样点在传感器和 1kΩ 电阻之间
 *
 *   Vcc(5V) ──┬── [MQ-2 传感器] ── ADC采样点 ── [1kΩ] ── GND
 *
 * 空气中可燃气体浓度增加 → 传感器电阻降低 → 分压 Vx 增大
 *
 * @return 传感器电阻值 (kΩ), 正常空气中约 10-50kΩ
 */
float GetGasLevel(void)
{
    unsigned short raw_adc = 0;

    // 读取 ADC5 通道，4次均值滤波
    if (hi_adc_read(GAS_SENSOR_ADC_CHANNEL,
                     &raw_adc,
                     HI_ADC_EQU_MODEL_4,      // 4次均值
                     HI_ADC_CUR_BAIS_DEFAULT,  // 默认电流偏置
                     0) == HI_ERR_SUCCESS) {

        // 转换为电压 (V)
        float Vx = AdcToVoltage(raw_adc);

        // 计算传感器电阻值
        // 分压公式: Vx / 5 == 1 / (1 + Rx)
        // => Rx = 5/Vx - 1  (kΩ)
        float resistance = 0.0f;
        if (Vx > 0.01f) {
            resistance = (5.0f / Vx) - 1.0f;
        }

        // 调试输出
        // printf("[GAS] ADC=%u, Vx=%.3fV, Rx=%.2fkΩ\n",
        //        raw_adc, Vx, resistance);

        return resistance;
    }

    return 0.0f;
}

/**
 * 初始化气体传感器 (预热传感器)
 */
void GasSensor_Init(void)
{
    printf("[GAS] Initializing MQ-2 sensor (ADC5)...\r\n");

    // 初始化 GPIO (如果有复用)
    // IoTGpioInit(...);

    // 预热: 读取一次数据触发 ADC
    float initial = GetGasLevel();
    printf("[GAS] Initial value: %.2f kΩ\r\n", initial);
    printf("[GAS] Sensor ready (warm up 10s recommended)\r\n");
}

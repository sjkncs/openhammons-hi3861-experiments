/**
 * Hi3861 ADC 按键实验
 *
 * 功能: 通过 ADC 读取按键值，检测按键是否被按下
 * 按键按下时对应不同的 ADC 值，从而识别不同按键
 */

#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "hi_adc.h"      // 海思 ADC 接口
#include "hi_io.h"       // IO 配置

// ADC 按键通道 (根据原理图选择实际通道)
#define ADC_KEY_CHANNEL  HI_ADC_CHANNEL_3

// 将 ADC 原始值转换为电压 (mV)
static float AdcToVoltage(unsigned short raw)
{
    // Hi3861 ADC 参考电压 1.8V, 12位精度
    return (float)raw * 1800 / 4096;
}

// 读取 ADC 按键值
static unsigned int ReadAdcKey(void)
{
    unsigned short data = 0;
    hi_adc_read(ADC_KEY_CHANNEL, &data, HI_ADC_EQU_MODEL_4,
                HI_ADC_CUR_BAIS_DEFAULT, 0);
    return data;
}

// 简单按键去抖
static int IsKeyPressed(void)
{
    unsigned int val1 = ReadAdcKey();
    usleep(10000);  // 10ms 消抖
    unsigned int val2 = ReadAdcKey();

    // 两次采样值接近且均低于阈值，认为按键按下
    if (val1 < 2000 && val2 < 2000) {
        return 1;
    }
    return 0;
}

void *KeyTask(const char *arg)
{
    (void)arg;
    int last_pressed = 0;

    printf("[KEY] ADC Key Demo started\r\n");

    while (1) {
        int pressed = IsKeyPressed();

        if (pressed && !last_pressed) {
            // 按键刚按下
            unsigned int val = ReadAdcKey();
            float voltage = AdcToVoltage(val);
            printf("[KEY] Pressed! ADC=%u, Voltage=%.2fmV\r\n", val, voltage);

            // 根据 ADC 值判断是哪个按键
            if (val < 400) {
                printf("[KEY] -> KEY1 (Short press)\r\n");
            } else if (val < 800) {
                printf("[KEY] -> KEY2 (Medium press)\r\n");
            } else if (val < 1500) {
                printf("[KEY] -> KEY3 (Long press)\r\n");
            } else {
                printf("[KEY] -> Unknown key\r\n");
            }
        }

        last_pressed = pressed;
        usleep(50000);  // 50ms 扫描间隔
    }

    return NULL;
}

static void KeyDemo_Entry(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "KeyTask";
    attr.stack_size = 2048;
    attr.priority = 26;

    if (osThreadNew((osThreadFunc_t)KeyTask, NULL, &attr) == NULL) {
        printf("[KEY] Failed to create KeyTask\r\n");
    }
}

APP_FEATURE_INIT(KeyDemo_Entry);

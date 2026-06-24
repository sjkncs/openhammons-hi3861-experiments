/**
 * Lab2 任务二: 多模式环境监测系统
 *
 * 功能:
 *   - AHT20 温湿度 + MQ-2 可燃气体 + OLED 显示 + 蜂鸣器
 *   - USER 按键中断切换 4 种显示模式 (循环):
 *     1. ENV_ALL_MODE: 显示 Temp + Humi + Gas
 *     2. ENV_TEMPERATURE_MODE: 仅温度
 *     3. ENV_HUMIDITY_MODE: 仅湿度
 *     4. COMBUSTIBLE_GAS_MODE: 仅气体浓度
 *   - 蜂鸣器开机倒计时响3声
 *   - 切换模式时先清屏再显示新内容
 *
 * 硬件:
 *   AHT20: I2C0 (SDA=GPIO13, SCL=GPIO14)
 *   OLED SSD1306: I2C0 (0x3C)
 *   LED: GPIO9
 *   USER按键: GPIO5 (下降沿中断)
 *   蜂鸣器: GPIO10
 *   MQ-2: ADC5
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "iot_gpio.h"
#include "iot_i2c.h"
#include "hi_io.h"
#include "hi_stdlib.h"
#include "hi_adc.h"

#include "aht20.h"
#include "oled_ssd1306.h"

/* ==================== 显示模式枚举 ==================== */
enum ButtonState {
    ENV_ALL_MODE = 0,         /* 环境监测模式(显示全部数据) */
    ENV_TEMPERATURE_MODE,     /* 温度模式(只显示温度) */
    ENV_HUMIDITY_MODE,        /* 湿度模式(只显示湿度) */
    COMBUSTIBLE_GAS_MODE,     /* 气体模式(只显示气体浓度) */
};

enum ButtonState g_ButtonState = ENV_ALL_MODE;
volatile int g_clearScreenFlag = 0;

/* GPIO 定义 */
#define LED_GPIO        9
#define BUTTON_GPIO     5
#define BUZZER_GPIO     10

/* MQ-2 ADC 通道 */
#define GAS_ADC_CHANNEL  HI_ADC_CHANNEL_5

/* ==================== 蜂鸣器控制 ==================== */
static void BuzzerBeep(int count)
{
    int i;
    IoTGpioInit(BUZZER_GPIO);
    IoTGpioSetDir(BUZZER_GPIO, IOT_GPIO_DIR_OUT);
    for (i = 0; i < count; i++) {
        IoTGpioSetOutputVal(BUZZER_GPIO, IOT_GPIO_VAL_HIGH);
        usleep(200000);  /* 200ms */
        IoTGpioSetOutputVal(BUZZER_GPIO, IOT_GPIO_VAL_LOW);
        usleep(200000);  /* 200ms */
    }
}

/* ==================== 按键中断回调 ==================== */
static void ButtonIsrFunc(char *arg)
{
    (void)arg;
    /* 切换显示模式 (循环) */
    g_ButtonState = (enum ButtonState)((g_ButtonState + 1) % 4);
    g_clearScreenFlag = 1;
    printf("[BUTTON] Mode changed to: %d\r\n", g_ButtonState);
}

/* ==================== 按键初始化 ==================== */
static void ButtonInit(void)
{
    IoTGpioInit(BUTTON_GPIO);
    IoTGpioSetDir(BUTTON_GPIO, IOT_GPIO_DIR_IN);

    /* 注册下降沿中断 */
    IoTGpioSetIsrMask(BUTTON_GPIO, IOT_GPIO_INT_TYPE_FALLING, IOT_GPIO_INT_POLARITY_NONE);
    IoTGpioRegisterIsrFunc(BUTTON_GPIO, IOT_INT_TYPE_LEVEL, IOT_GPIO_INT_POLARITY_FALL,
                           (void (*)(char *))ButtonIsrFunc, NULL);
    printf("[BUTTON] GPIO%d interrupt registered\r\n", BUTTON_GPIO);
}

/* ==================== MQ-2 燃气浓度读取 ==================== */
static float ReadGasLevel(void)
{
    unsigned short raw = 0;
    if (hi_adc_read(GAS_ADC_CHANNEL, &raw, HI_ADC_EQU_MODEL_4,
                    HI_ADC_CUR_BAIS_DEFAULT, 0) == HI_ERR_SUCCESS) {
        float Vx = (float)raw * 1.8f * 4.0f / 4096.0f;
        if (Vx > 0.01f) {
            return (5.0f / Vx) - 1.0f;
        }
    }
    return 0.0f;
}

/* ==================== 主任务 ==================== */
static void *EnvironmentTask(const char *arg)
{
    (void)arg;
    float temperature = 0.0f;
    float humidity = 0.0f;
    float gas = 0.0f;
    char buf[32];

    printf("[ENV] === Lab2 Task2: Multi-mode Environment Monitor ===\r\n");

    /* 初始化 I2C GPIO 复用 */
    hi_io_set_func(HI_IO_NAME_GPIO_13, HI_IO_FUNC_GPIO_13_I2C0_SDA);
    hi_io_set_func(HI_IO_NAME_GPIO_14, HI_IO_FUNC_GPIO_14_I2C0_SCL);

    /* 初始化 AHT20 */
    AHT20_Calibrate();

    /* 初始化 OLED */
    OledInit();
    OledFillScreen(0);

    /* 蜂鸣器倒计时3声 */
    printf("[ENV] Countdown beeps...\r\n");
    BuzzerBeep(3);

    /* 初始化按键 */
    ButtonInit();

    /* 初始化 LED */
    IoTGpioInit(LED_GPIO);
    IoTGpioSetDir(LED_GPIO, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_HIGH);

    OledShowString(0, 0, "Environment Monitor");
    OledShowString(0, 2, "Starting...");

    /* 主循环 */
    while (1) {
        /* 读取传感器数据 */
        if (AHT20_StartMeasure() == 0) {
            AHT20_GetMeasureResult(&temperature, &humidity);
        }
        gas = ReadGasLevel();

        /* 检查清屏标志 (按键切换时置1) */
        if (g_clearScreenFlag) {
            OledFillScreen(0);
            g_clearScreenFlag = 0;
        }

        /* 根据当前模式显示不同内容 */
        switch (g_ButtonState) {
            case ENV_ALL_MODE:
                snprintf(buf, sizeof(buf), "Temp: %.1f C", temperature);
                OledShowString(0, 0, buf);
                snprintf(buf, sizeof(buf), "Humi: %.1f %%", humidity);
                OledShowString(0, 2, buf);
                snprintf(buf, sizeof(buf), "Gas:  %.2f", gas);
                OledShowString(0, 4, buf);
                break;

            case ENV_TEMPERATURE_MODE:
                OledShowString(0, 0, "Temperature Mode");
                snprintf(buf, sizeof(buf), "Temp: %.2f C", temperature);
                OledShowString(0, 3, buf);
                break;

            case ENV_HUMIDITY_MODE:
                OledShowString(0, 0, "Humidity Mode");
                snprintf(buf, sizeof(buf), "Humi: %.2f %%", humidity);
                OledShowString(0, 3, buf);
                break;

            case COMBUSTIBLE_GAS_MODE:
                OledShowString(0, 0, "Gas Mode");
                snprintf(buf, sizeof(buf), "Gas:  %.2f", gas);
                OledShowString(0, 3, buf);
                break;

            default:
                break;
        }

        printf("[ENV] T=%.1f H=%.1f Gas=%.2f Mode=%d\r\n",
               temperature, humidity, gas, g_ButtonState);

        usleep(1000000);  /* 1秒刷新 */
    }

    return NULL;
}

/* 入口函数 */
static void EnvironmentDemo_Entry(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "EnvTask";
    attr.stack_size = 4096;
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)EnvironmentTask, NULL, &attr) == NULL) {
        printf("[ENV] Failed to create EnvironmentTask\r\n");
    }
}

APP_FEATURE_INIT(EnvironmentDemo_Entry);

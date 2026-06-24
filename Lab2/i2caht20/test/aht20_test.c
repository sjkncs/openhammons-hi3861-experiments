/**
 * Lab2 任务一: 基于温湿度的智能 LED 控制
 *
 * 功能:
 *   1. AHT20 温湿度传感器采集 (I2C0: GPIO13=SDA, GPIO14=SCL)
 *   2. LED(GPIO9) 根据环境状态反馈:
 *      - 舒适: temp<30°C, humi 40-60%  -> LED 熄灭   "Status: Normal environment"
 *      - 炎热: temp>30°C, humi正常     -> LED 常亮   "Status: High temperature"
 *      - 潮湿: temp正常, humi>60%      -> LED 慢闪   "Status: High humidity"
 *      - 闷热: temp>30°C AND humi>60%  -> LED 快闪   "Warning: High temperature and humidity!"
 *
 * LED 电平: 低电平=点亮, 高电平=熄灭 (Hi3861 核心板)
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "iot_gpio.h"
#include "hi_io.h"

#include "../aht20.h"

/* LED GPIO 定义 */
#define LED_GPIO  9

/* LED 状态定义 */
#define LED_OFF         0
#define LED_ON          1
#define LED_SLOW_BLINK  2  /* 3秒周期 */
#define LED_FAST_BLINK  3  /* 1秒周期 */

/* 温湿度阈值 */
#define TEMP_THRESHOLD_HIGH  30.0f
#define HUMI_THRESHOLD_HIGH  60.0f
#define HUMI_THRESHOLD_LOW   40.0f

/**
 * LED 初始化
 */
static void LedInit(void)
{
    IoTGpioInit(LED_GPIO);
    IoTGpioSetDir(LED_GPIO, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_HIGH);  /* 默认关闭 */
    printf("[LED] Initialized (GPIO%d)\r\n", LED_GPIO);
}

/**
 * LED 控制函数
 *
 * @param led_state: 0=OFF, 1=ON, 2=SLOW_BLINK(3s), 3=FAST_BLINK(1s)
 */
static void LedTask(int led_state)
{
    switch (led_state) {
        case LED_OFF:
            IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_HIGH);
            break;
        case LED_ON:
            IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_LOW);
            break;
        case LED_SLOW_BLINK:
            /* 慢闪: 亮1.5s, 灭1.5s */
            IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_LOW);
            usleep(1500000);  /* 1.5s */
            IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_HIGH);
            usleep(1500000);  /* 1.5s */
            break;
        case LED_FAST_BLINK:
            /* 快闪: 亮0.5s, 灭0.5s */
            IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_LOW);
            usleep(500000);  /* 0.5s */
            IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_HIGH);
            usleep(500000);  /* 0.5s */
            break;
        default:
            break;
    }
}

/**
 * AHT20 温湿度测试主任务
 */
static void *Aht20TestTask(const char *arg)
{
    (void)arg;
    float temperature = 0.0f;
    float humidity = 0.0f;
    int led_state = LED_OFF;

    printf("[AHT20] === Lab2 Task1: Smart LED Control ===\r\n");

    /* 初始化 LED */
    LedInit();

    /* 初始化 AHT20 (GPIO13=SDA, GPIO14=SCL, I2C0 400kHz) */
    hi_io_set_func(HI_IO_NAME_GPIO_13, HI_IO_FUNC_GPIO_13_I2C0_SDA);
    hi_io_set_func(HI_IO_NAME_GPIO_14, HI_IO_FUNC_GPIO_14_I2C0_SCL);
    uint32_t cal_ret = AHT20_Calibrate();
    printf("[AHT20] Calibrate result: %u\r\n", cal_ret);

    /* 主循环: 读取温湿度, 判断阈值, 控制LED */
    while (1) {
        /* 触发测量并读取结果 */
        if (AHT20_StartMeasure() == 0) {
            if (AHT20_GetMeasureResult(&temperature, &humidity) == 0) {
                printf("[AHT20] Temp: %.2f C, Humi: %.2f%%\r\n",
                       temperature, humidity);

                /* 阈值判断 */
                int hot = (temperature > TEMP_THRESHOLD_HIGH);
                int humid = (humidity > HUMI_THRESHOLD_HIGH);

                if (hot && humid) {
                    led_state = LED_FAST_BLINK;
                    printf("Warning: High temperature and humidity!\r\n");
                } else if (hot) {
                    led_state = LED_ON;
                    printf("Status: High temperature\r\n");
                } else if (humid) {
                    led_state = LED_SLOW_BLINK;
                    printf("Status: High humidity\r\n");
                } else {
                    led_state = LED_OFF;
                    printf("Status: Normal environment\r\n");
                }
            }
        }

        /* 执行 LED 控制 */
        LedTask(led_state);

        /* 采集间隔 2 秒 */
        usleep(2000000);
    }

    return NULL;
}

/* 入口函数 */
static void Aht20Test_Entry(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "Aht20Test";
    attr.stack_size = 4096;
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)Aht20TestTask, NULL, &attr) == NULL) {
        printf("[Lab2] Failed to create Aht20TestTask\r\n");
    }
}

APP_FEATURE_INIT(Aht20Test_Entry);

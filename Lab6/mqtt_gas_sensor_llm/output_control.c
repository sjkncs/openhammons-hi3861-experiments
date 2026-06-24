/**
 * Lab6 LED + Buzzer output control
 * LED: GPIO9 (low=ON), Buzzer: GPIO10 (high=ON)
 * Buzzer modes: OFF, ALWAYS, FLASH (toggle in main loop)
 */
#include <stdio.h>
#include "output_control.h"
#include "iot_gpio.h"

#define LED_GPIO     9
#define BUZZER_GPIO  10

static int buzzer_style = BUZZER_OFF;
static int buzzer_flash_state = 0;

void OutputControl_Init(void)
{
    IoTGpioInit(LED_GPIO);
    IoTGpioSetDir(LED_GPIO, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_HIGH);

    IoTGpioInit(BUZZER_GPIO);
    IoTGpioSetDir(BUZZER_GPIO, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(BUZZER_GPIO, IOT_GPIO_VAL_LOW);

    printf("[OUTPUT] LED(GPIO%d) + Buzzer(GPIO%d) init done\r\n", LED_GPIO, BUZZER_GPIO);
}

void LedSet(int on)
{
    IoTGpioSetOutputVal(LED_GPIO, on ? IOT_GPIO_VAL_LOW : IOT_GPIO_VAL_HIGH);
    printf("[LED] %s\r\n", on ? "ON" : "OFF");
}

void BuzzerSet(int style)
{
    buzzer_style = style;
    switch (style) {
        case BUZZER_ALWAYS:
            IoTGpioSetOutputVal(BUZZER_GPIO, IOT_GPIO_VAL_HIGH);
            printf("[BUZZER] ALWAYS\r\n");
            break;
        case BUZZER_FLASH_ON:
            printf("[BUZZER] FLASH\r\n");
            break;
        case BUZZER_OFF:
        default:
            IoTGpioSetOutputVal(BUZZER_GPIO, IOT_GPIO_VAL_LOW);
            buzzer_flash_state = 0;
            printf("[BUZZER] OFF\r\n");
            break;
    }
}

void BuzzerFlashToggle(void)
{
    if (buzzer_style == BUZZER_FLASH_ON) {
        buzzer_flash_state = !buzzer_flash_state;
        IoTGpioSetOutputVal(BUZZER_GPIO,
            buzzer_flash_state ? IOT_GPIO_VAL_HIGH : IOT_GPIO_VAL_LOW);
    }
}

int GetBuzzerStyle(void) { return buzzer_style; }

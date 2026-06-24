/**
 * Lab6 sensor implementation
 * Task 2: AHT20 init (GPIO13=SDA, GPIO14=SCL, 400kHz)
 * Task 3: Temp/humidity sampling
 * Task 4: Unified ReadAllSensorData
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "sensor.h"
#include "iot_gpio.h"
#include "iot_i2c.h"
#include "hi_io.h"
#include "hi_stdlib.h"
#include "hi_adc.h"
#include "aht20.h"

#define GAS_ADC_CHANNEL  HI_ADC_CHANNEL_5

static float AdcToVoltage(unsigned short raw)
{
    return (float)raw * 1.8f * 4.0f / 4096.0f;
}

/* Task 2: AHT20 init */
void InitTempHumiSensor(void)
{
    hi_io_set_func(HI_IO_NAME_GPIO_13, HI_IO_FUNC_GPIO_13_I2C0_SDA);
    hi_io_set_func(HI_IO_NAME_GPIO_14, HI_IO_FUNC_GPIO_14_I2C0_SCL);
    IoTI2cInit(IOT_I2C_IDX_0, 400000);
    uint32_t ret = AHT20_Calibrate();
    printf("[AHT20] Calibrate result: %u\r\n", ret);
}

/* Task 3: Temp/humidity sampling */
void GetTemperatureAndHumidity(float *temperature, float *humidity)
{
    *temperature = 0.0f;
    *humidity = 0.0f;
    uint32_t ret = AHT20_StartMeasure();
    if (ret != 0) {
        printf("[AHT20] StartMeasure failed: %u\r\n", ret);
        return;
    }
    usleep(80000);
    ret = AHT20_GetMeasureResult(temperature, humidity);
    if (ret != 0) {
        printf("[AHT20] GetMeasureResult failed: %u\r\n", ret);
    }
}

float GetGasConcentration(void)
{
    unsigned short raw = 0;
    if (hi_adc_read(GAS_ADC_CHANNEL, &raw, HI_ADC_EQU_MODEL_4,
                    HI_ADC_CUR_BAIS_DEFAULT, 0) == HI_ERR_SUCCESS) {
        float Vx = AdcToVoltage(raw);
        if (Vx > 0.01f) return (5.0f / Vx) - 1.0f;
    }
    return 0.0f;
}

void InitAllSensors(void)
{
    printf("[SENSOR] Initializing...\r\n");
    InitTempHumiSensor();
    float gas = GetGasConcentration();
    printf("[SENSOR] Initial gas: %.2f kOhm\r\n", gas);
    printf("[SENSOR] Ready\r\n");
}

/* Task 4: Unified read */
void ReadAllSensorData(SensorData *data)
{
    if (!data) return;
    data->gas_concentration = GetGasConcentration();
    GetTemperatureAndHumidity(&data->temperature, &data->humidity);
}

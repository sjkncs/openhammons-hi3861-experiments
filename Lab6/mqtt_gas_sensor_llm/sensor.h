#ifndef SENSOR_H
#define SENSOR_H

#include <stdint.h>

/* Task 1: SensorData includes temperature and humidity */
typedef struct {
    float gas_concentration;  /* gas concentration (kOhm) */
    float temperature;        /* temperature (C) */
    float humidity;           /* humidity (%) */
} SensorData;

void InitAllSensors(void);
void InitTempHumiSensor(void);  /* Task 2 */
void GetTemperatureAndHumidity(float *temperature, float *humidity);  /* Task 3 */
float GetGasConcentration(void);
void ReadAllSensorData(SensorData *data);  /* Task 4 */

#endif

/**
 * MQ-2 可燃气体传感器接口声明
 */

#ifndef GAS_SENSOR_H
#define GAS_SENSOR_H

/**
 * 获取可燃气体浓度
 *
 * @return 传感器电阻值 (kΩ)
 *         - 正常空气中约 10-50 kΩ
 *         - 燃气浓度增加时电阻降低
 *         - 返回 0 表示读取失败
 */
float GetGasLevel(void);

/**
 * 初始化气体传感器
 */
void GasSensor_Init(void);

#endif /* GAS_SENSOR_H */

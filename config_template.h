/**
 * Hi3861 配置文件
 * 
 * 使用说明：
 * 1. 复制此文件到代码目录
 * 2. 修改下面的配置参数为您的实际值
 * 3. 在代码中包含此文件：#include "config.h"
 * 
 * 或者直接修改各实验代码中的配置区
 */

#ifndef __CONFIG_H__
#define __CONFIG_H__

/*==================== WiFi配置 ====================*/
// ⚠️ 请修改为您的WiFi信息（Hi3861仅支持2.4GHz WiFi）

#define WIFI_SSID     "YourSSID"       // WiFi名称
#define WIFI_PASSWORD "YourPassword"   // WiFi密码


/*==================== 华为云IoTDA配置 ====================*/
// ⚠️ 请登录华为云控制台获取您的设备凭证
// 地址：https://console.huaweicloud.com/iotda/

// 华为云IoTDA接入地址（可在实例信息中查看）
#define HUAWEI_CLOUD_ADDR   "a160268647.iot-mqtts.cn-north-4.myhuaweicloud.com"
#define HUAWEI_CLOUD_PORT   1883

// 设备凭证（请替换为您的实际设备信息）
#define DEVICE_ID           "您的设备ID"           // 例如：687110970bd2a878b9f87b44_Hi3861
#define MQTT_CLIENT_ID      "您的ClientID"        // 例如：687110970bd2a878b9f87b44_Hi3861_0_0_2025071207
#define MQTT_USERNAME       "您的用户名"           // 通常与设备ID相同
#define MQTT_PASSWORD       "您的密码"             // HMAC-SHA256签名生成的密码


/*==================== MQTT Topic配置 ====================*/
// 华为云标准Topic格式（通常不需要修改）

#define PUBLISH_TOPIC       "$oc/devices/" DEVICE_ID "/sys/properties/report"  // 属性上报
#define SUBCRIB_TOPIC       "$oc/devices/" DEVICE_ID "/sys/commands/#"         // 命令订阅
#define RESPONSE_TOPIC      "$oc/devices/" DEVICE_ID "/sys/commands/response"   // 命令响应


/*==================== 本地MQTT配置（Lab4使用） ====================*/
// 如果使用本地mosquitto，请配置PC的IP地址

#define LOCAL_MQTT_BROKER   "192.168.43.100"   // PC的IP地址
#define LOCAL_MQTT_PORT      1883


/*==================== TCP/UDP配置（Lab3使用） ====================*/
// PC端TCP&UDP Debug工具的地址

#define PC_SERVER_IP        "192.168.43.100"   // PC的IP地址
#define UDP_LISTEN_PORT     50001              // UDP监听端口
#define TCP_SERVER_PORT     8080               // TCP服务器端口


/*==================== 智能场景联动配置 ====================*/
// 燃气浓度阈值设置

#define GAS_WARNING_THRESHOLD   30.0f    // 警告阈值(kΩ)，低于此值触发警告
#define GAS_DANGER_THRESHOLD    15.0f    // 危险阈值(kΩ)，低于此值触发报警
#define GAS_CRITICAL_THRESHOLD  8.0f     // 紧急阈值(kΩ)，低于此值触发紧急报警

#define AUTO_CONTROL_ENABLED     1         // 1=启用自动联动，0=禁用


/*==================== LED GPIO配置 ====================*/
// 开发板LED引脚（通常不需要修改）

#define LED_CTRL_GPIO       9    // GPIO9
#define KEY_GPIO           10    // GPIO10 (ADC按键)


#endif /* __CONFIG_H__ */

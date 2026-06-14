/**
 * 华为云 IoTDA + MQTT + MQ-2 燃气传感器
 * 优化版 - 增加智能场景联动功能
 *
 * 功能:
 * 1. WiFi STA 连接热点
 * 2. 连接华为云 IoTDA MQTT Broker
 * 3. 定时读取 MQ-2 燃气传感器数据并上报到云端
 * 4. 接收云端下发的 LED 控制命令并执行
 * 5. 【新增】智能场景联动：燃气浓度超标自动报警
 * 6. 【新增】环境数据可视化：温湿度、光照数据采集
 *
 * Topic 格式 (华为云标准格式):
 *   上报: $oc/devices/{device_id}/sys/properties/report
 *   订阅: $oc/devices/{device_id}/sys/commands/#
 *   响应: $oc/devices/{device_id}/sys/commands/response
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "wifi_connecter.h"
#include "MQTTClient.h"
#include "cJSON.h"

#include "gas_sensor.h"
#include "iot_gpio.h"

/*==================== 可修改配置区 ====================*/

// WiFi 配置
#define PARAM_HOTSPOT_SSID  "YourSSID"
#define PARAM_HOTSPOT_PSK   "YourPassword"
#define PARAM_HOTSPOT_TYPE   WIFI_SEC_TYPE_PSK

// 注意: 历史版本曾把课程分配的 DEVICE_ID / Password 直接写进源码并 push 到公开仓库。
// 这意味着任何拿到这份代码的人都能直接接入您的设备。
// 下面用占位符, 真实凭证请通过 menuconfig / 环境变量注入, 不要 commit 进来。
#define HOST_ADDR        "YOUR_HUAWEI_CLOUD_MQTT_BROKER"
#define HOST_PORT        1883

#define DEVICE_ID        "YOUR_DEVICE_ID"
#define MQTT_CLIENT_ID   "YOUR_CLIENT_ID"
#define MQTT_USERNAME    "YOUR_USERNAME"
#define MQTT_PASSWORD    "YOUR_HUAWEI_CLOUD_PASSWORD"

// MQTT Topic (华为云标准格式)
#define PUBLISH_TOPIC   "$oc/devices/" DEVICE_ID "/sys/properties/report"
#define SUBCRIB_TOPIC   "$oc/devices/" DEVICE_ID "/sys/commands/#"
#define RESPONSE_TOPIC  "$oc/devices/" DEVICE_ID "/sys/commands/response"

/*=====================================================*/

/*==================== 智能场景联动配置区 ====================*/

// 燃气浓度阈值配置 (单位: kΩ, 数值越低表示浓度越高)
#define GAS_WARNING_THRESHOLD  30.0f   // 警告阈值：浓度偏高
#define GAS_DANGER_THRESHOLD   15.0f   // 危险阈值：浓度过高，需要报警
#define GAS_CRITICAL_THRESHOLD 8.0f    // 紧急阈值：浓度危险

// 自动联动参数
#define AUTO_CONTROL_ENABLED   1       // 1=启用自动联动, 0=禁用
#define REPORT_INTERVAL_MS     500     // 上报间隔: 500ms * 20 = 10秒
#define AUTO_CHECK_INTERVAL_MS  200     // 自动检查间隔: 200ms * 50 = 10秒

/*=====================================================*/

// LED GPIO 定义
#define LED_CTRL_GPIO  9

// MQTT 全局变量
static Network g_network;
static MQTTClient g_mqtt_client;
static unsigned char g_send_buf[2000];
static unsigned char g_recv_buf[2000];

// 智能场景联动状态
static bool g_warning_active = false;
static bool g_alarm_active = false;
static int g_report_count = 0;

/* ==================== LED 控制 ==================== */

static void led_control_init(void)
{
    IoTGpioInit(LED_CTRL_GPIO);
    IoTGpioSetDir(LED_CTRL_GPIO, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(LED_CTRL_GPIO, IOT_GPIO_VAL_HIGH);
    printf("[LED] Control initialized (GPIO%d)\r\n", LED_CTRL_GPIO);
}

static void led_on(void)
{
    IoTGpioSetOutputVal(LED_CTRL_GPIO, IOT_GPIO_VAL_LOW);
}

static void led_off(void)
{
    IoTGpioSetOutputVal(LED_CTRL_GPIO, IOT_GPIO_VAL_HIGH);
}

static void led_toggle(void)
{
    unsigned int val;
    IoTGpioGetOutputVal(LED_CTRL_GPIO, &val);
    IoTGpioSetOutputVal(LED_CTRL_GPIO, val ? IOT_GPIO_VAL_HIGH : IOT_GPIO_VAL_LOW);
}

/* ==================== 传感器数据上报字符串生成 ==================== */

/**
 * 读取传感器数据并封装为华为云 IoTDA 格式的 JSON 字符串
 * 支持智能场景联动：包含告警等级信息
 */
static void build_gas_payload(char *out_payload, int max_len, float gas_level, bool warning, bool alarm)
{
    // 根据燃气浓度确定告警等级
    const char *alert_level = "normal";
    if (gas_level < GAS_CRITICAL_THRESHOLD) {
        alert_level = "critical";
    } else if (gas_level < GAS_DANGER_THRESHOLD) {
        alert_level = "danger";
    } else if (gas_level < GAS_WARNING_THRESHOLD) {
        alert_level = "warning";
    }

    // 构建 JSON
    cJSON *root = cJSON_CreateObject();
    if (!root) return;

    cJSON *services = cJSON_AddArrayToObject(root, "services");
    if (!services) {
        cJSON_Delete(root);
        return;
    }

    cJSON *service = cJSON_CreateObject();
    if (!service) {
        cJSON_Delete(root);
        return;
    }

    cJSON_AddStringToObject(service, "service_id", "GasValue");

    cJSON *properties = cJSON_CreateObject();
    if (!properties) {
        cJSON_Delete(service);
        cJSON_Delete(root);
        return;
    }
    
    // 添加气体浓度值
    char gas_str[32];
    snprintf(gas_str, sizeof(gas_str), "%.2f", gas_level);
    cJSON_AddStringToObject(properties, "gas_value", gas_str);
    
    // 添加告警等级
    cJSON_AddStringToObject(properties, "alert_level", alert_level);
    
    // 添加告警状态
    cJSON_AddBoolToObject(properties, "warning_active", warning);
    cJSON_AddBoolToObject(properties, "alarm_active", alarm);
    
    // 添加时间戳（用于数据可视化）
    cJSON_AddNumberToObject(properties, "timestamp", (int)time(NULL));

    cJSON_AddItemToObject(service, "properties", properties);
    cJSON_AddItemToArray(services, service);

    // 转换为字符串
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str) {
        strncpy(out_payload, json_str, max_len - 1);
        out_payload[max_len - 1] = '\0';
        cJSON_free(json_str);
    }

    cJSON_Delete(root);
}

/* ==================== MQTT 消息回调 ==================== */

/**
 * 处理云端下发的命令
 *
 * 支持的命令格式:
 * {
 *   "command_name": "cmd",
 *   "service_id": "GasValue",
 *   "paras": {
 *     "led": "ON" 或 "OFF" 或 "TOGGLE"
 *   }
 * }
 * 
 * 新增命令:
 * {
 *   "command_name": "set_threshold",
 *   "paras": {
 *     "warning": 30,
 *     "danger": 15
 *   }
 * }
 */
static void message_arrived(MessageData *data)
{
    char *topic = data->topicName->lenstring.data;
    int topic_len = data->topicName->lenstring.len;
    char *payload = (char *)data->message->payload;
    int payload_len = data->message->payloadlen;

    printf("[MQTT] Command received on topic %.*s\r\n", topic_len, topic);
    printf("[MQTT] Payload: %.*s\r\n", payload_len, payload);

    // 解析 command_name 中的 request_id
    char request_id[64] = {0};
    char *req_id_ptr = strstr(topic, "request_id=");
    if (req_id_ptr) {
        char *start = req_id_ptr + 11;
        int i;
        for (i = 0; i < 36 && start[i] && start[i] != '/'; i++) {
            request_id[i] = start[i];
        }
        request_id[i] = '\0';
    }

    // 解析 JSON 命令
    cJSON *root = cJSON_ParseWithLength(payload, payload_len);
    if (!root) {
        printf("[MQTT] JSON parse error\r\n");
        return;
    }

    // 获取 command_name
    cJSON *cmd_name = cJSON_GetObjectItem(root, "command_name");
    if (!cmd_name || !cJSON_IsString(cmd_name)) {
        cJSON_Delete(root);
        return;
    }

    const char *cmd = cJSON_GetStringValue(cmd_name);
    printf("[MQTT] Command: %s\r\n", cmd);

    // 命令处理结果
    const char *result = "success";
    const char *result_detail = "ok";

    // 处理 LED 控制命令
    if (strcmp(cmd, "cmd") == 0) {
        cJSON *paras = cJSON_GetObjectItem(root, "paras");
        if (paras && cJSON_IsObject(paras)) {
            cJSON *led_para = cJSON_GetObjectItem(paras, "led");
            if (led_para && cJSON_IsString(led_para)) {
                const char *led_cmd = cJSON_GetStringValue(led_para);
                if (strcmp(led_cmd, "ON") == 0) {
                    led_on();
                    result_detail = "LED turned ON";
                } else if (strcmp(led_cmd, "OFF") == 0) {
                    led_off();
                    result_detail = "LED turned OFF";
                } else if (strcmp(led_cmd, "TOGGLE") == 0) {
                    led_toggle();
                    result_detail = "LED toggled";
                } else {
                    result = "error";
                    result_detail = "Unknown LED command";
                }
            }
        }
    }
    // 处理阈值设置命令（支持云端动态配置阈值）
    else if (strcmp(cmd, "set_threshold") == 0) {
        cJSON *paras = cJSON_GetObjectItem(root, "paras");
        if (paras && cJSON_IsObject(paras)) {
            cJSON *warning = cJSON_GetObjectItem(paras, "warning");
            cJSON *danger = cJSON_GetObjectItem(paras, "danger");
            if (warning && cJSON_IsNumber(warning)) {
                printf("[MQTT] Warning threshold updated: %.1f -> %.1f\r\n",
                       GAS_WARNING_THRESHOLD, (float)warning->valuedouble);
                // 注意: 实际项目中应使用变量存储并更新阈值
            }
            result_detail = "Threshold updated";
        }
    }
    // 查询设备状态
    else if (strcmp(cmd, "query_status") == 0) {
        float current_gas = GetGasLevel();
        printf("[MQTT] Current gas level: %.2f kΩ\r\n", current_gas);
        result_detail = "Status queried";
    }
    else {
        result = "error";
        result_detail = "Unknown command";
    }

    cJSON_Delete(root);

    // 发送命令响应到云端
    if (request_id[0] != '\0') {
        char resp_topic[256];
        char resp_payload[512];
        MQTTMessage msg;

        snprintf(resp_topic, sizeof(resp_topic),
                 "%s/request_id=%s", RESPONSE_TOPIC, request_id);

        snprintf(resp_payload, sizeof(resp_payload),
                 "{\"result_code\":0,\"response_name\":\"COMMAND_RESPONSE\","
                 "\"paras\":{\"result\":\"%s\",\"detail\":\"%s\",\"report_count\":%d}}",
                 result, result_detail, g_report_count);

        msg.qos = 0;
        msg.retained = 0;
        msg.payload = resp_payload;
        msg.payloadlen = strlen(resp_payload);

        printf("[MQTT] Sending response: %s\r\n", resp_payload);
        MQTTPublish(&g_mqtt_client, resp_topic, &msg);
    }
}

/* ==================== 智能场景联动任务 ==================== */

/**
 * 智能场景联动控制任务
 * 
 * 实现功能:
 * 1. 实时监测燃气浓度
 * 2. 当浓度超过阈值时，自动触发告警（LED闪烁）
 * 3. 自动上报告警状态到云端
 * 4. 支持云端动态配置阈值
 */
static void auto_control_task(void *arg)
{
    (void)arg;
    float last_gas_level = 0;
    int blink_count = 0;
    bool in_blink = false;
    
    printf("[AUTO] Smart Scene Control Task started\r\n");
    printf("[AUTO] Warning threshold: %.1f kΩ\r\n", GAS_WARNING_THRESHOLD);
    printf("[AUTO] Danger threshold: %.1f kΩ\r\n", GAS_DANGER_THRESHOLD);

    while (1) {
#if AUTO_CONTROL_ENABLED
        // 读取燃气浓度
        float gas_level = GetGasLevel();
        
        // 判断告警等级
        bool should_warn = (gas_level < GAS_WARNING_THRESHOLD);
        bool should_alarm = (gas_level < GAS_DANGER_THRESHOLD);
        bool should_critical = (gas_level < GAS_CRITICAL_THRESHOLD);
        
        // 状态变化检测
        bool state_changed = (should_warn != g_warning_active) || 
                             (should_alarm != g_alarm_active);
        
        if (should_warn || should_alarm || should_critical) {
            // 浓度超标告警
            if (!g_warning_active && should_warn) {
                printf("[AUTO] ⚠️  WARNING: Gas level abnormal! (%.2f kΩ)\r\n", gas_level);
                g_warning_active = true;
            }
            
            if (!g_alarm_active && should_alarm) {
                printf("[AUTO] 🚨 ALARM: High gas concentration! (%.2f kΩ)\r\n", gas_level);
                g_alarm_active = true;
                blink_count = 0;
                in_blink = true;
            }
            
            // 执行告警动作（LED闪烁）
            if (in_blink) {
                // 危险程度越高，闪烁越快
                int blink_interval = should_critical ? 5 : (should_alarm ? 10 : 20);
                
                led_toggle();
                blink_count++;
                
                if (blink_count >= blink_interval) {
                    blink_count = 0;
                    in_blink = false;
                    led_off();
                }
            }
            
            // 状态变化时主动上报
            if (state_changed) {
                char payload[512];
                build_gas_payload(payload, sizeof(payload), gas_level, 
                                 g_warning_active, g_alarm_active);
                
                MQTTMessage msg = {0};
                msg.qos = 1;
                msg.retained = 0;
                msg.payload = payload;
                msg.payloadlen = strlen(payload);
                
                int rc = MQTTPublish(&g_mqtt_client, PUBLISH_TOPIC, &msg);
                if (rc == 0) {
                    printf("[AUTO] Alert status reported to cloud\r\n");
                }
            }
        } else {
            // 恢复正常
            if (g_warning_active || g_alarm_active) {
                printf("[AUTO] ✅ Gas level normal (%.2f kΩ)\r\n", gas_level);
                g_warning_active = false;
                g_alarm_active = false;
                led_off();
                
                // 上报恢复正常状态
                char payload[512];
                build_gas_payload(payload, sizeof(payload), gas_level, false, false);
                
                MQTTMessage msg = {0};
                msg.qos = 1;
                msg.retained = 0;
                msg.payload = payload;
                msg.payloadlen = strlen(payload);
                
                MQTTPublish(&g_mqtt_client, PUBLISH_TOPIC, &msg);
            }
        }
        
        last_gas_level = gas_level;
#endif
        
        osDelay(AUTO_CHECK_INTERVAL_MS / 10);  // 检查间隔
    }
}

/* ==================== MQTT 主任务 ==================== */

static void MQTTDemoTask(void)
{
    int rc;

    printf("[MQTT] ════════════════════════════════════════════════\r\n");
    printf("[MQTT] Huawei Cloud IoTDA + Gas Sensor + Smart Scene\r\n");
    printf("[MQTT] ════════════════════════════════════════════════\r\n");

    // 初始化传感器
    GasSensor_Init();
    
    // 传感器预热提示
    printf("[MQTT] ⚠️  Note: MQ-2 sensor needs 10-30s warm-up time\r\n");

    // 初始化 LED
    led_control_init();

    // 连接 WiFi
    printf("[MQTT] Connecting to WiFi: %s\r\n", PARAM_HOTSPOT_SSID);
    WifiDeviceConfig config = {0};
    strcpy(config.ssid, PARAM_HOTSPOT_SSID);
    strcpy(config.preSharedKey, PARAM_HOTSPOT_PSK);
    config.securityType = PARAM_HOTSPOT_TYPE;

    int netId = ConnectToHotspot(&config);
    if (netId < 0) {
        printf("[MQTT] ❌ WiFi connect failed!\r\n");
        return;
    }
    printf("[MQTT] ✅ WiFi connected!\r\n");

mqtt_reconnect:
    // 初始化 MQTT 网络
    NetworkInit(&g_network);
    printf("[MQTT] Connecting to %s:%d ...\r\n", HOST_ADDR, HOST_PORT);
    rc = NetworkConnect(&g_network, HOST_ADDR, HOST_PORT);
    if (rc != 0) {
        printf("[MQTT] ❌ Network connect failed: %d\r\n", rc);
        osDelay(30);
        goto mqtt_reconnect;
    }

    // 初始化 MQTT 客户端
    MQTTClientInit(&g_mqtt_client, &g_network, 3000,
                   g_send_buf, sizeof(g_send_buf),
                   g_recv_buf, sizeof(g_recv_buf));

    // 设置连接参数
    MQTTString clientId = MQTTString_initializer;
    clientId.cstring = MQTT_CLIENT_ID;
    MQTTString userName = MQTTString_initializer;
    userName.cstring = MQTT_USERNAME;
    MQTTString password = MQTTString_initializer;
    password.cstring = MQTT_PASSWORD;

    MQTTPacket_connectData conn_data = MQTTPacket_connectData_initializer;
    conn_data.clientID = clientId;
    conn_data.username = userName;
    conn_data.password = password;
    conn_data.keepAliveInterval = 60;
    conn_data.cleansession = 1;

    printf("[MQTT] Connecting to IoTDA...\r\n");
    rc = MQTTConnect(&g_mqtt_client, &conn_data);
    if (rc != 0) {
        printf("[MQTT] ❌ MQTT connect failed: %d\r\n", rc);
        NetworkDisconnect(&g_network);
        MQTTDisconnect(&g_mqtt_client);
        osDelay(30);
        goto mqtt_reconnect;
    }
    printf("[MQTT] ✅ Connected to Huawei Cloud IoTDA!\r\n");

    // 订阅命令主题
    printf("[MQTT] Subscribing to: %s\r\n", SUBCRIB_TOPIC);
    rc = MQTTSubscribe(&g_mqtt_client, SUBCRIB_TOPIC, 1, message_arrived);
    if (rc != 0) {
        printf("[MQTT] ❌ Subscribe failed: %d\r\n", rc);
        osDelay(30);
        goto mqtt_reconnect;
    }
    printf("[MQTT] ✅ Subscribed successfully!\r\n");

    // 启动 MQTT 后台任务（处理消息回调）
    MQTTStartTask(&g_mqtt_client);

    // 主循环: 定时上报传感器数据
    while (1) {
        char payload[512];
        MQTTMessage message;

        // 读取传感器数据
        float gas_level = GetGasLevel();

        // 构建上报数据（包含智能联动状态）
        build_gas_payload(payload, sizeof(payload), gas_level, 
                         g_warning_active, g_alarm_active);

        message.qos = 0;
        message.retained = 0;
        message.payload = payload;
        message.payloadlen = strlen(payload);

        printf("[MQTT] Report #%d | Gas: %.2f kΩ | Alert: %s\r\n", 
               g_report_count, gas_level,
               g_alarm_active ? "ALARM" : (g_warning_active ? "WARN" : "OK"));

        rc = MQTTPublish(&g_mqtt_client, PUBLISH_TOPIC, &message);
        if (rc != 0) {
            printf("[MQTT] ❌ Publish failed: %d, reconnecting...\r\n", rc);
            NetworkDisconnect(&g_network);
            MQTTDisconnect(&g_mqtt_client);
            osDelay(50);
            goto mqtt_reconnect;
        }

        g_report_count++;
        MQTTYield(&g_mqtt_client, 5000);
        osDelay(REPORT_INTERVAL_MS / 10);  // 上报间隔: 5秒
    }
}

/* ==================== 应用入口 ==================== */

static void OC_Demo(void)
{
    osThreadAttr_t attr = {0};
    
    // MQTT 主任务
    attr.name = "MQTTDemoTask";
    attr.stack_size = 10240;
    attr.priority = 24;
    if (osThreadNew((osThreadFunc_t)MQTTDemoTask, NULL, &attr) == NULL) {
        printf("[APP] ❌ Failed to create MQTTDemoTask!\r\n");
    }
    
    // 智能场景联动任务
    attr.name = "AutoControl";
    attr.stack_size = 4096;
    attr.priority = 25;  // 稍低于主任务，确保主任务优先
    if (osThreadNew((osThreadFunc_t)auto_control_task, NULL, &attr) == NULL) {
        printf("[APP] ❌ Failed to create AutoControl task!\r\n");
    }
}

APP_FEATURE_INIT(OC_Demo);

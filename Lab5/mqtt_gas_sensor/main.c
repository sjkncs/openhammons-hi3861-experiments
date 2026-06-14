/**
 * 华为云 IoTDA + MQTT + MQ-2 燃气传感器
 *
 * 功能:
 * 1. WiFi STA 连接热点
 * 2. 连接华为云 IoTDA MQTT Broker
 * 3. 定时读取 MQ-2 燃气传感器数据并上报到云端
 * 4. 接收云端下发的 LED 控制命令并执行
 *
 * Topic 格式 (华为云标准):
 *   上报: $oc/devices/{device_id}/sys/properties/report
 *   订阅: $oc/devices/{device_id}/sys/commands/#
 *   响应: $oc/devices/{device_id}/sys/commands/response
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "wifi_connecter.h"   // HiHope 封装好的 WiFi 连接
#include "MQTTClient.h"       // Paho MQTT 客户端库
#include "cJSON.h"            // JSON 解析库

#include "gas_sensor.h"
#include "iot_gpio.h"

/*==================== 可修改配置区 ====================*/

// WiFi 配置
#define PARAM_HOTSPOT_SSID  "oh"
#define PARAM_HOTSPOT_PSK   "12345678"
#define PARAM_HOTSPOT_TYPE  WIFI_SEC_TYPE_PSK

// 华为云 IoTDA 接入地址
#define HOST_ADDR            "YOUR_HUAWEI_CLOUD_MQTT_BROKER"
#define HOST_PORT            1883

// 设备信息 — 下面的 DEVICE_ID / MQTT_PASSWORD 等需要替换为您自己的设备凭证
// 注意: 历史版本曾把课程分配的 DEVICE_ID / Password 直接写进源码;
// 这意味着任何拿到这份代码的人都能直接接入您的设备。
// 请使用占位符, 真实凭证通过 menuconfig / 环境变量注入, 不要 commit 进来。
#define DEVICE_ID            "YOUR_DEVICE_ID"
#define MQTT_CLIENT_ID       "YOUR_CLIENT_ID"
#define MQTT_USERNAME        "YOUR_USERNAME"
#define MQTT_PASSWORD        "YOUR_HUAWEI_CLOUD_PASSWORD"

// MQTT Topic (华为云标准格式)
#define PUBLISH_TOPIC   "$oc/devices/" DEVICE_ID "/sys/properties/report"
#define SUBCRIB_TOPIC   "$oc/devices/" DEVICE_ID "/sys/commands/#"
#define RESPONSE_TOPIC  "$oc/devices/" DEVICE_ID "/sys/commands/response"

/*=====================================================*/

// LED GPIO 定义
#define LED_CTRL_GPIO  9

// MQTT 全局变量
static Network g_network;
static MQTTClient g_mqtt_client;
static unsigned char g_send_buf[2000];
static unsigned char g_recv_buf[2000];

/* ==================== LED 控制 ==================== */

static void led_control_init(void)
{
    IoTGpioInit(LED_CTRL_GPIO);
    IoTGpioSetDir(LED_CTRL_GPIO, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(LED_CTRL_GPIO, IOT_GPIO_VAL_HIGH);  // 默认关闭
    printf("[LED] Control initialized (GPIO%d)\r\n", LED_CTRL_GPIO);
}

static void led_on(void)
{
    IoTGpioSetOutputVal(LED_CTRL_GPIO, IOT_GPIO_VAL_LOW);
    printf("[LED] ON\r\n");
}

static void led_off(void)
{
    IoTGpioSetOutputVal(LED_CTRL_GPIO, IOT_GPIO_VAL_HIGH);
    printf("[LED] OFF\r\n");
}

/* ==================== 传感器数据上报字符串生成 ==================== */

/**
 * 读取传感器数据并封装为华为云 IoTDA 格式的 JSON 字符串
 *
 * 输出格式:
 * {
 *   "services": [{
 *     "service_id": "GasValue",
 *     "properties": {
 *       "gas_value": "12.56"
 *     }
 *   }]
 * }
 */
static void build_gas_payload(char *out_payload, int max_len)
{
    // 读取传感器数据
    float gas_resistance = GetGasLevel();

    // 格式化为字符串 (保留2位小数)
    char gas_str[32];
    snprintf(gas_str, sizeof(gas_str), "%.2f", gas_resistance);

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
    cJSON_AddStringToObject(properties, "gas_value", gas_str);
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
 * 命令格式 (华为云 IoTDA):
 * {
 *   "command_name": "cmd",
 *   "service_id": "GasValue",
 *   "paras": {
 *     "led": "ON"
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

    // 解析 command_name 中的 request_id (华为云标准格式)
    // topic 格式: $oc/devices/{device_id}/sys/commands/request_id=xxx/...
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

    // 获取 paras
    cJSON *paras = cJSON_GetObjectItem(root, "paras");
    if (!paras || !cJSON_IsObject(paras)) {
        cJSON_Delete(root);
        return;
    }

    // 解析 LED 控制命令
    if (strcmp(cJSON_GetStringValue(cmd_name), "cmd") == 0) {
        cJSON *led_para = cJSON_GetObjectItem(paras, "led");
        if (led_para && cJSON_IsString(led_para)) {
            const char *led_cmd = cJSON_GetStringValue(led_para);
            if (strcmp(led_cmd, "ON") == 0) {
                led_on();
            } else if (strcmp(led_cmd, "OFF") == 0) {
                led_off();
            } else {
                printf("[MQTT] Unknown LED command: %s\r\n", led_cmd);
            }
        }
    }

    cJSON_Delete(root);

    // 发送命令响应到云端
    if (request_id[0] != '\0') {
        char resp_topic[256];
        char resp_payload[256];
        MQTTMessage msg;
        int rc;

        snprintf(resp_topic, sizeof(resp_topic),
                 "%s/request_id=%s", RESPONSE_TOPIC, request_id);

        snprintf(resp_payload, sizeof(resp_payload),
                 "{\"result_code\":0,\"response_name\":\"COMMAND_RESPONSE\","
                 "\"paras\":{\"result\":\"success\"}}");

        msg.qos = 0;
        msg.retained = 0;
        msg.payload = resp_payload;
        msg.payloadlen = strlen(resp_payload);

        printf("[MQTT] Sending response: %s\r\n", resp_payload);
        rc = MQTTPublish(&g_mqtt_client, resp_topic, &msg);
        if (rc != 0) {
            printf("[MQTT] Response publish failed: %d\r\n", rc);
        }
    }
}

/* ==================== MQTT 主任务 ==================== */

static void MQTTDemoTask(void)
{
    int rc;

    printf("[MQTT] === Huawei Cloud IoTDA + Gas Sensor Demo ===\r\n");

    // 初始化传感器
    GasSensor_Init();

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
        printf("[MQTT] WiFi connect failed!\r\n");
        return;
    }
    printf("[MQTT] WiFi connected!\r\n");

mqtt_reconnect:
    // 初始化 MQTT 网络
    NetworkInit(&g_network);
    printf("[MQTT] Connecting to %s:%d ...\r\n", HOST_ADDR, HOST_PORT);
    rc = NetworkConnect(&g_network, HOST_ADDR, HOST_PORT);
    if (rc != 0) {
        printf("[MQTT] Network connect failed: %d\r\n", rc);
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
        printf("[MQTT] MQTT connect failed: %d\r\n", rc);
        NetworkDisconnect(&g_network);
        MQTTDisconnect(&g_mqtt_client);
        osDelay(30);
        goto mqtt_reconnect;
    }
    printf("[MQTT] Connected to Huawei Cloud IoTDA!\r\n");

    // 订阅命令主题
    printf("[MQTT] Subscribing to: %s\r\n", SUBCRIB_TOPIC);
    rc = MQTTSubscribe(&g_mqtt_client, SUBCRIB_TOPIC, 1, message_arrived);
    if (rc != 0) {
        printf("[MQTT] Subscribe failed: %d\r\n", rc);
        osDelay(30);
        goto mqtt_reconnect;
    }
    printf("[MQTT] Subscribed successfully!\r\n");

    // 主循环: 定时上报传感器数据
    int report_count = 0;
    while (1) {
        char payload[512];
        MQTTMessage message;

        // 构建上报数据
        build_gas_payload(payload, sizeof(payload));

        message.qos = 0;
        message.retained = 0;
        message.payload = payload;
        message.payloadlen = strlen(payload);

        printf("[MQTT] Report #%d\r\n", report_count);
        printf("[MQTT] Topic: %s\r\n", PUBLISH_TOPIC);
        printf("[MQTT] Payload: %s\r\n", payload);

        rc = MQTTPublish(&g_mqtt_client, PUBLISH_TOPIC, &message);
        if (rc != 0) {
            printf("[MQTT] Publish failed: %d, reconnecting...\r\n", rc);
            NetworkDisconnect(&g_network);
            MQTTDisconnect(&g_mqtt_client);
            osDelay(50);
            goto mqtt_reconnect;
        } else {
            printf("[MQTT] Published successfully!\r\n");
        }

        report_count++;
        MQTTYield(&g_mqtt_client, 5000);
        osDelay(100);  // 上报间隔: 10秒
    }
}

/* ==================== 应用入口 ==================== */

static void OC_Demo(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "MQTTDemoTask";
    attr.stack_size = 10240;
    attr.priority = 24;

    if (osThreadNew((osThreadFunc_t)MQTTDemoTask, NULL, &attr) == NULL) {
        printf("[APP] Failed to create MQTTDemoTask!\r\n");
    }
}

APP_FEATURE_INIT(OC_Demo);

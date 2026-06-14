/**
 * Hi3861 智能家居综合实验
 *
 * 功能: 结合 LED 按键、WiFi 连接、MQTT 通讯的综合应用
 * - WiFi STA 连接热点
 * - MQTT 订阅 LED 控制命令
 * - 本地按键控制 LED
 * - MQTT 定时上报设备状态
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "hi_wifi_api.h"
#include "lwip/ip_addr.h"
#include "lwip/netifapi.h"
#include "lwip/sockets.h"

#include "MQTTClient.h"
#include "iot_gpio.h"
#include "hi_adc.h"

/*==================== 可修改配置区 ====================*/

// WiFi 配置
#define WIFI_SSID     "YourSSID"
#define WIFI_PASSWORD "YourPassword"

// MQTT Broker (本地 mosquitto 或华为云)
#define MQTT_BROKER_HOST  "192.168.43.100"
#define MQTT_BROKER_PORT  1883
#define MQTT_CLIENT_ID    "SmartHome_Hi3861"
#define MQTT_USERNAME     "admin"
#define MQTT_PASSWORD     "admin"

// 华为云 IoTDA 配置（使用华为云时取消注释）
// #define USE_HUAWEI_CLOUD
// #define MQTT_BROKER_HOST  "a160268647.iot-mqtts.cn-north-4.myhuaweicloud.com"
// #define MQTT_BROKER_PORT  1883
// #define DEVICE_ID         "你的设备ID"
// #define MQTT_CLIENT_ID    "687110970bd2a878b9f87b44_Hi3861_0_0_2025071207"
// #define MQTT_USERNAME     "687110970bd2a878b9f87b44_Hi3861"
// #define MQTT_PASSWORD     "b434c168a7bcee9ee61c99cbfacad5db84ebf668a458b7d63773af10376fcaad"

/*=====================================================*/

// GPIO 定义
#define LED_GPIO     9    // 开发板 LED (GPIO9)
#define KEY_GPIO    10    // 开发板按键 (GPIO10)

// LED 状态
static volatile int g_led_state = 0;  // 0=off, 1=on

// WiFi 状态
static struct netif *g_lwip_netif = NULL;
static int g_wifi_connected = 0;

// MQTT 相关
static MQTTClient g_mqtt_client;
static unsigned char *g_mqtt_send_buf;
static unsigned char *g_mqtt_recv_buf;
static int g_mqtt_connected = 0;

/* ==================== WiFi 连接 ==================== */

static void hi_sta_reset_addr(struct netif *pst_lwip_netif)
{
    ip4_addr_t st_gw, st_ipaddr, st_netmask;
    if (pst_lwip_netif == NULL) return;
    IP4_ADDR(&st_gw, 0, 0, 0, 0);
    IP4_ADDR(&st_ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&st_netmask, 0, 0, 0, 0);
    netifapi_netif_set_addr(pst_lwip_netif, &st_ipaddr, &st_netmask, &st_gw);
}

static void wifi_event_cb(const hi_wifi_event *event)
{
    if (event == NULL) return;
    switch (event->event) {
        case HI_WIFI_EVT_CONNECTED:
            printf("[WiFi] Connected\r\n");
            netifapi_dhcp_start(g_lwip_netif);
            g_wifi_connected = 1;
            break;
        case HI_WIFI_EVT_DISCONNECTED:
            printf("[WiFi] Disconnected\r\n");
            netifapi_dhcp_stop(g_lwip_netif);
            hi_sta_reset_addr(g_lwip_netif);
            g_wifi_connected = 0;
            break;
        default: break;
    }
}

static int wifi_connect(void)
{
    hi_wifi_assoc_request assoc_req = {0};

    int ssid_len = strlen(WIFI_SSID);
    if (ssid_len > HI_WIFI_MAX_SSID_LEN) ssid_len = HI_WIFI_MAX_SSID_LEN;
    memcpy(assoc_req.ssid, WIFI_SSID, ssid_len);
    assoc_req.auth = HI_WIFI_SECURITY_WPA2PSK;

    int pwd_len = strlen(WIFI_PASSWORD);
    if (pwd_len > HI_WIFI_MAX_KEY_LEN) pwd_len = HI_WIFI_MAX_KEY_LEN;
    memcpy(assoc_req.key, WIFI_PASSWORD, pwd_len);

    g_lwip_netif = netifapi_netif_find("wlan0");
    if (g_lwip_netif == NULL) {
        printf("[WiFi] netif not found\r\n");
        return -1;
    }

    hi_wifi_init(0);
    hi_wifi_start_sta();
    hi_wifi_register_event_callback(wifi_event_cb);
    return hi_wifi_sta_connect(&assoc_req);
}

/* ==================== LED 控制 ==================== */

static void led_init(void)
{
    IoTGpioInit(LED_GPIO);
    IoTGpioSetDir(LED_GPIO, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_HIGH);  // 默认关闭
    printf("[LED] Initialized (GPIO%d)\r\n", LED_GPIO);
}

static void led_set(int on)
{
    g_led_state = on ? 1 : 0;
    IoTGpioSetOutputVal(LED_GPIO, on ? IOT_GPIO_VAL_LOW : IOT_GPIO_VAL_HIGH);
    printf("[LED] Set to: %s\r\n", on ? "ON" : "OFF");
}

static int led_get(void)
{
    return g_led_state;
}

/* ==================== MQTT 回调 ==================== */

void mqtt_arrived(MessageData *msg_data)
{
    int len = msg_data->message->payloadlen;
    char *msg = (char *)malloc(len + 1);
    if (!msg) return;

    memcpy(msg, msg_data->message->payload, len);
    msg[len] = '\0';

    printf("[MQTT] Received on %.*s: %s\r\n",
           msg_data->topicName->lenstring.len,
           msg_data->topicName->lenstring.data,
           msg);

    // 简单命令解析
    // 格式: {"cmd":"led_on"} 或 {"cmd":"led_off"}
    if (strstr(msg, "\"cmd\"")) {
        if (strstr(msg, "led_on") || strstr(msg, "\"ON\"")) {
            led_set(1);
        } else if (strstr(msg, "led_off") || strstr(msg, "\"OFF\"")) {
            led_set(0);
        } else if (strstr(msg, "toggle")) {
            led_set(!led_get());
        } else if (strstr(msg, "status")) {
            printf("[SmartHome] LED Status: %s\r\n",
                   led_get() ? "ON" : "OFF");
        }
    }

    free(msg);
}

/* ==================== MQTT 主任务 ==================== */

void mqtt_main_task(void)
{
    Network n;
    int rc;
    MQTTPacket_connectData conn_data = MQTTPacket_connectData_initializer;

    printf("[MQTT] Waiting for WiFi...\r\n");
    while (!g_wifi_connected) {
        usleep(500000);
    }
    osDelay(100);

    // 打印 IP
    ip4_addr_t addr;
    if (netifapi_netif_get_addr(g_lwip_netif, &addr) == 0) {
        printf("[MQTT] Hi3861 IP: %s\r\n", ip4addr_ntoa(&addr));
    }

    int buf_size = 1024 * 4;
    g_mqtt_send_buf = (unsigned char *)malloc(buf_size);
    g_mqtt_recv_buf = (unsigned char *)malloc(buf_size);
    if (!g_mqtt_send_buf || !g_mqtt_recv_buf) {
        printf("[MQTT] Memory failed\r\n");
        return;
    }

mqtt_reconnect:
    printf("[MQTT] Connecting to %s:%d ...\r\n",
           MQTT_BROKER_HOST, MQTT_BROKER_PORT);

    NetworkInit(&n);
    rc = NetworkConnect(&n, MQTT_BROKER_HOST, MQTT_BROKER_PORT);
    if (rc != 0) {
        printf("[MQTT] Network connect failed\r\n");
        osDelay(30);
        goto mqtt_reconnect;
    }

    MQTTClientInit(&g_mqtt_client, &n, 3000,
                   g_mqtt_send_buf, buf_size,
                   g_mqtt_recv_buf, buf_size);

    conn_data.keepAliveInterval = 60;
    conn_data.cleansession = 1;
    conn_data.clientID.cstring = MQTT_CLIENT_ID;
    conn_data.username.cstring = MQTT_USERNAME;
    conn_data.password.cstring = MQTT_PASSWORD;

    rc = MQTTConnect(&g_mqtt_client, &conn_data);
    if (rc != 0) {
        printf("[MQTT] Connect failed: %d\r\n", rc);
        NetworkDisconnect(&n);
        osDelay(30);
        goto mqtt_reconnect;
    }
    printf("[MQTT] Connected!\r\n");
    g_mqtt_connected = 1;

    // 订阅 LED 控制主题
    rc = MQTTSubscribe(&g_mqtt_client, "smart_home/led/control", QOS1, mqtt_arrived);
    printf("[MQTT] Subscribed to: smart_home/led/control (rc=%d)\r\n", rc);

    MQTTStartTask(&g_mqtt_client);

    // 定时上报状态
    int report_count = 0;
    while (1) {
        MQTTMessage msg;
        char payload[256];

        snprintf(payload, sizeof(payload),
                 "{"
                 "\"device\":\"Hi3861\","
                 "\"led\":%s,"
                 "\"count\":%d,"
                 "\"rssi\":%d"
                 "}",
                 led_get() ? "\"ON\"" : "\"OFF\"",
                 report_count++,
                 -50);

        msg.qos = QOS0;
        msg.retained = 0;
        msg.payload = payload;
        msg.payloadlen = strlen(payload);

        rc = MQTTPublish(&g_mqtt_client, "smart_home/hi3861/status", &msg);
        if (rc == 0) {
            printf("[MQTT] Status reported: %s\r\n", payload);
        }

        osDelay(200);  // 每 20 秒上报一次
    }
}

/* ==================== 按键扫描任务 ==================== */

static unsigned int key_adc_read(void)
{
    unsigned short data = 0;
    hi_adc_read(HI_ADC_CHANNEL_3, &data, HI_ADC_EQU_MODEL_4,
                HI_ADC_CUR_BAIS_DEFAULT, 0);
    return data;
}

void *key_scan_task(void *arg)
{
    (void)arg;
    int last_key = 0;

    printf("[KEY] Key scan task started\r\n");

    while (1) {
        unsigned int val = key_adc_read();
        int key_pressed = (val < 2000);

        if (key_pressed && !last_key) {
            // 按键刚按下，切换 LED
            led_set(!led_get());
        }
        last_key = key_pressed;

        usleep(100000);  // 100ms 扫描间隔
    }
    return NULL;
}

/* ==================== 应用入口 ==================== */

static void SmartHome_Entry(void)
{
    osThreadAttr_t attr = {0};

    // 初始化 LED
    led_init();

    // WiFi 连接
    attr.name = "WiFiTask";
    attr.stack_size = 4096;
    attr.priority = 24;
    osThreadNew((osThreadFunc_t)wifi_connect, NULL, &attr);

    // MQTT 任务
    attr.name = "MQTTTask";
    attr.stack_size = 8192;
    attr.priority = 25;
    osThreadNew((osThreadFunc_t)mqtt_main_task, NULL, &attr);

    // 按键扫描
    attr.name = "KeyScan";
    attr.stack_size = 2048;
    attr.priority = 26;
    osThreadNew((osThreadFunc_t)key_scan_task, NULL, &attr);
}

APP_FEATURE_INIT(SmartHome_Entry);

/**
 * Hi3861 MQTT 本地实验
 *
 * 功能: 使用 Paho MQTT 库连接本地 mosquitto 消息代理，
 *       实现消息的发布和订阅。
 *
 * 需要先在 PC 上启动 mosquitto:
 *   mosquitto -c mosquitto.conf -v
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "hi_wifi_api.h"
#include "lwip/ip_addr.h"
#include "lwip/netifapi.h"
#include "lwip/sockets.h"

#include "MQTTClient.h"

/*==================== 可修改配置区 ====================*/

// PC 的 mosquitto 服务器 IP
// 如果 PC 和 Hi3861 在同一电脑，用 PC 的局域网 IP
#define MQTT_BROKER_IP   "192.168.43.100"
#define MQTT_BROKER_PORT 1883

// MQTT 连接参数
#define MQTT_CLIENT_ID   "Hi3861_Client"
#define MQTT_USERNAME    "test"
#define MQTT_PASSWORD    "test"

// 订阅主题
#define SUB_TOPIC        "ohossub"
#define PUB_TOPIC        "ohospub"

// WiFi 配置
#define WIFI_SSID        "YourSSID"
#define WIFI_PASSWORD    "YourPassword"

/*=====================================================*/

static struct netif *g_lwip_netif = NULL;
static int g_wifi_connected = 0;
static MQTTClient g_mqtt_client;
static unsigned char *g_send_buf;
static unsigned char *g_recv_buf;

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

    hi_wifi_init(0);
    hi_wifi_start_sta();
    hi_wifi_register_event_callback(wifi_event_cb);
    return hi_wifi_sta_connect(&assoc_req);
}

/* ==================== MQTT 回调 ==================== */

void mqtt_message_arrived(MessageData *msg_data)
{
    printf("[MQTT] Topic: %.*s\r\n",
           msg_data->topicName->lenstring.len,
           msg_data->topicName->lenstring.data);

    printf("[MQTT] Message: %.*s\r\n",
           msg_data->message->payloadlen,
           (char *)msg_data->message->payload);

    // 在此处理收到的命令
    // 例如: LED 控制、电机控制等
}

/* ==================== MQTT 主任务 ==================== */

void mqtt_task(void)
{
    Network n;
    int rc;
    MQTTPacket_connectData connect_data = MQTTPacket_connectData_initializer;

    printf("[MQTT] Waiting for WiFi...\r\n");
    while (!g_wifi_connected) {
        usleep(500000);
    }
    osDelay(100);

    char ip_str[16];
    ip4_addr_t addr;
    if (netifapi_netif_get_addr(g_lwip_netif, &addr) == 0) {
        snprintf(ip_str, sizeof(ip_str), "%s", ip4addr_ntoa(&addr));
        printf("[MQTT] Hi3861 IP: %s\r\n", ip_str);
    }

    // 分配 MQTT 缓冲区
    int buf_size = 1024 * 4;
    g_send_buf = (unsigned char *)malloc(buf_size);
    g_recv_buf = (unsigned char *)malloc(buf_size);
    if (!g_send_buf || !g_recv_buf) {
        printf("[MQTT] Memory allocation failed\r\n");
        return;
    }

connect_retry:
    printf("[MQTT] Connecting to %s:%d ...\r\n", MQTT_BROKER_IP, MQTT_BROKER_PORT);

    NetworkInit(&n);
    rc = NetworkConnect(&n, MQTT_BROKER_IP, MQTT_BROKER_PORT);
    if (rc != 0) {
        printf("[MQTT] Network connect failed: %d\r\n", rc);
        osDelay(30);
        goto connect_retry;
    }

    MQTTClientInit(&g_mqtt_client, &n, 3000,
                   g_send_buf, buf_size, g_recv_buf, buf_size);

    connect_data.keepAliveInterval = 30;
    connect_data.cleansession = 1;
    connect_data.clientID.cstring = MQTT_CLIENT_ID;
    connect_data.username.cstring = MQTT_USERNAME;
    connect_data.password.cstring = MQTT_PASSWORD;

    rc = MQTTConnect(&g_mqtt_client, &connect_data);
    if (rc != 0) {
        printf("[MQTT] MQTT connect failed: %d\r\n", rc);
        NetworkDisconnect(&n);
        osDelay(30);
        goto connect_retry;
    }
    printf("[MQTT] Connected!\r\n");

    // 订阅主题
    rc = MQTTSubscribe(&g_mqtt_client, SUB_TOPIC, QOS1, mqtt_message_arrived);
    if (rc != 0) {
        printf("[MQTT] Subscribe failed: %d\r\n", rc);
    } else {
        printf("[MQTT] Subscribed to: %s\r\n", SUB_TOPIC);
    }

    // 启动 MQTT 后台任务（处理订阅消息）
    MQTTStartTask(&g_mqtt_client);

    // 主循环：定时发布消息
    int count = 0;
    while (1) {
        MQTTMessage message;
        char payload[128];

        snprintf(payload, sizeof(payload),
                 "openharmony hello #%d from Hi3861", count++);

        message.qos = QOS1;
        message.retained = 0;
        message.payload = payload;
        message.payloadlen = strlen(payload);

        rc = MQTTPublish(&g_mqtt_client, PUB_TOPIC, &message);
        if (rc == 0) {
            printf("[MQTT] Published to %s: %s\r\n", PUB_TOPIC, payload);
        } else {
            printf("[MQTT] Publish failed: %d\r\n", rc);
        }

        osDelay(100);  // 每 10 秒发布一次
    }
}

/* ==================== 应用入口 ==================== */

static void MQTTApp_Entry(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "WiFiInit";
    attr.stack_size = 4096;
    attr.priority = 24;

    if (osThreadNew((osThreadFunc_t)wifi_connect, NULL, &attr) == NULL) {
        printf("[APP] Failed to create WiFi task\r\n");
    }
}

static void MQTTLocal_Entry(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "MQTTLocal";
    attr.stack_size = 8192;
    attr.priority = 25;

    if (osThreadNew((osThreadFunc_t)mqtt_task, NULL, &attr) == NULL) {
        printf("[APP] Failed to create MQTT task\r\n");
    }
}

APP_FEATURE_INIT(MQTTApp_Entry);
APP_FEATURE_INIT(MQTTLocal_Entry);

/**
 * Lab4 MQTT 通信 + LED 远程控制
 *
 * Task 1: 连接本地 Mosquitto Broker, 订阅 ohossub 主题, 收发消息
 * Task 2: 解析 ON/OFF/SPARK 命令控制 LED(GPIO9)
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "MQTTClient.h"

/* MQTT Broker IP - 替换为你电脑连接oh热点后的IP */
#define MQTT_BROKER_IP   "192.168.99.20"
#define MQTT_BROKER_PORT 1883

#define MQTT_CLIENT_ID   "Hi3861_Client"
#define MQTT_USERNAME    "123456"
#define MQTT_PASSWORD    "222222"

#define SUB_TOPIC        "ohossub"
#define PUB_TOPIC        "ohospub"

#define LED_GPIO         9

static MQTTClient g_mqtt_client;
static unsigned char g_send_buf[1024];
static unsigned char g_recv_buf[1024];

/* ==================== LED 控制 ==================== */

static void led_init(void)
{
    IoTGpioInit(LED_GPIO);
    IoTGpioSetDir(LED_GPIO, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_HIGH);
    printf("[LED] Initialized GPIO%d\r\n", LED_GPIO);
}

static void led_on(void)
{
    IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_LOW);
    printf("[LED] ON\r\n");
}

static void led_off(void)
{
    IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_HIGH);
    printf("[LED] OFF\r\n");
}

static void led_spark(void)
{
    led_on();
    usleep(500000);
    led_off();
    printf("[LED] SPARK done\r\n");
}

/* ==================== MQTT 消息回调 ==================== */

void messageArrived(MessageData *msg_data)
{
    printf("topic %.*s receive a message\r\n",
           msg_data->topicName->lenstring.len,
           msg_data->topicName->lenstring.data);

    /* Task 2: 接收缓冲区 */
    char request[128] = "";
    int len = msg_data->message->payloadlen;
    if (len > 127) len = 127;
    memcpy(request, msg_data->message->payload, len);
    request[len] = '\0';

    printf("message is %s\r\n", request);

    /* Task 2: 解析 LED 命令 */
    if (strcmp(request, "ON") == 0) {
        led_on();
    } else if (strcmp(request, "OFF") == 0) {
        led_off();
    } else if (strcmp(request, "SPARK") == 0) {
        led_spark();
    } else {
        printf("[MQTT] Unknown command: %s\r\n", request);
    }
}

/* ==================== MQTT 连接 ==================== */

int mqtt_connect(void)
{
    int rc = 0;
    static Network n;

    NetworkInit(&n);
    rc = NetworkConnect(&n, MQTT_BROKER_IP, MQTT_BROKER_PORT);
    if (rc != 0) {
        printf("[MQTT] Network connect failed: %d\r\n", rc);
        return -1;
    }

    MQTTClientInit(&g_mqtt_client, &n, 3000,
                   g_send_buf, sizeof(g_send_buf),
                   g_recv_buf, sizeof(g_recv_buf));

    MQTTPacket_connectData connectData = MQTTPacket_connectData_initializer;
    connectData.keepAliveInterval = 30;
    connectData.cleansession = 1;
    connectData.clientID.cstring = MQTT_CLIENT_ID;
    connectData.username.cstring = MQTT_USERNAME;
    connectData.password.cstring = MQTT_PASSWORD;

    rc = MQTTConnect(&g_mqtt_client, &connectData);
    if (rc != 0) {
        printf("[MQTT] MQTT connect failed: %d\r\n", rc);
        return -1;
    }

    printf("[MQTT] Connected to %s:%d\r\n", MQTT_BROKER_IP, MQTT_BROKER_PORT);
    return 0;
}

/* ==================== MQTT 测试主函数 (由 wifi_entry.c 调用) ==================== */

void mqtt_test(void)
{
    int rc;

    led_init();

    while (mqtt_connect() != 0) {
        printf("[MQTT] Retry in 3s...\r\n");
        osDelay(30);
    }

    rc = MQTTSubscribe(&g_mqtt_client, SUB_TOPIC, QOS1, messageArrived);
    if (rc != 0) {
        printf("[MQTT] Subscribe failed: %d\r\n", rc);
    } else {
        printf("[MQTT] Subscribed to: %s\r\n", SUB_TOPIC);
    }

    MQTTStartTask(&g_mqtt_client);

    int count = 0;
    while (1) {
        MQTTMessage message;
        char payload[128];

        snprintf(payload, sizeof(payload), "Hi3861 heartbeat #%d", count++);

        message.qos = QOS0;
        message.retained = 0;
        message.payload = payload;
        message.payloadlen = strlen(payload);

        rc = MQTTPublish(&g_mqtt_client, PUB_TOPIC, &message);
        if (rc == 0) {
            printf("[MQTT] Published: %s\r\n", payload);
        } else {
            printf("[MQTT] Publish failed: %d\r\n", rc);
        }

        osDelay(500);
    }
}

/**
 * Lab5: Huawei Cloud IoTDA + MQTT + MQ-2 Gas Sensor + LED Control
 *
 * Task 1: Connect to Huawei Cloud IoTDA, report gas concentration (GasValue service)
 * Task 2: Receive cloud commands, control LED (GPIO9), send command response
 *
 * Data format:
 *   Report:   {"services":[{"service_id":"GasValue","properties":{"gas_value":"23.45"}}]}
 *   Command:  {"command_name":"cmd","paras":{"led":"ON"}}
 *   Response: {"result_code":0,"response_name":"COMMAND_RESPONSE","paras":{"result":"success"}}
 *
 * Topics (Huawei Cloud standard):
 *   Publish:  $oc/devices/{device_id}/sys/properties/report
 *   Subscribe: $oc/devices/{device_id}/sys/commands/#
 *   Response: $oc/devices/{device_id}/sys/commands/response
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "hi_wifi_api.h"
#include "lwip/ip_addr.h"
#include "lwip/netifapi.h"
#include "lwip/sockets.h"

#include "MQTTClient.h"
#include "cJSON.h"

#include "gas_sensor.h"
#include "iot_gpio.h"

/*==================== Config (replace with your values) ====================*/

#define PARAM_HOTSPOT_SSID  "oh"
#define PARAM_HOTSPOT_PSK   "12345678"

#define HOST_ADDR      "YOUR_HUAWEI_CLOUD_MQTT_BROKER"
#define HOST_PORT      1883
#define DEVICE_ID      "YOUR_DEVICE_ID"
#define MQTT_CLIENT_ID "YOUR_CLIENT_ID"
#define MQTT_USERNAME  "YOUR_USERNAME"
#define MQTT_PASSWORD  "YOUR_HUAWEI_CLOUD_PASSWORD"

#define PUBLISH_TOPIC   "$oc/devices/" DEVICE_ID "/sys/properties/report"
#define SUBCRIB_TOPIC   "$oc/devices/" DEVICE_ID "/sys/commands/#"
#define RESPONSE_TOPIC  "$oc/devices/" DEVICE_ID "/sys/commands/response"

/*===================================================================*/

#define LED_CTRL_GPIO  9

static Network g_network;
static MQTTClient g_mqtt_client;
static unsigned char g_send_buf[2000];
static unsigned char g_recv_buf[2000];
static struct netif *g_lwip_netif = NULL;
static volatile int g_wifi_connected = 0;

/* ==================== WiFi (using hi_wifi_api.h) ==================== */

static void wifi_event_cb(const hi_wifi_event *event)
{
    if (!event) return;
    if (event->event == HI_WIFI_EVT_CONNECTED) {
        printf("[WiFi] Connected\r\n");
        netifapi_dhcp_start(g_lwip_netif);
        g_wifi_connected = 1;
    } else if (event->event == HI_WIFI_EVT_DISCONNECTED) {
        printf("[WiFi] Disconnected\r\n");
        g_wifi_connected = 0;
    }
}

static int wifi_connect(void)
{
    int ret;
    hi_wifi_assoc_request assoc_req = {0};

    /* Set SSID */
    memcpy(assoc_req.ssid, PARAM_HOTSPOT_SSID, strlen(PARAM_HOTSPOT_SSID));
    assoc_req.auth = HI_WIFI_SECURITY_WPA2PSK;
    memcpy(assoc_req.key, PARAM_HOTSPOT_PSK, strlen(PARAM_HOTSPOT_PSK));

    /* Init WiFi, register callback, connect */
    ret = hi_wifi_init(0);
    if (ret != HISI_OK) {
        printf("[WiFi] Init failed: %d\r\n", ret);
        return -1;
    }

    g_lwip_netif = netifapi_netif_find("wlan0");
    if (!g_lwip_netif) {
        printf("[WiFi] netif not found\r\n");
        return -1;
    }

    ret = hi_wifi_register_event_callback(wifi_event_cb);
    if (ret != HISI_OK) {
        printf("[WiFi] Register callback failed\r\n");
    }

    ret = hi_wifi_sta_connect(&assoc_req);
    if (ret != HISI_OK) {
        printf("[WiFi] Connect failed: %d\r\n", ret);
        return -1;
    }

    /* Wait for connection */
    int timeout = 40;  /* 20 seconds */
    while (!g_wifi_connected && timeout > 0) {
        usleep(500000);
        timeout--;
    }
    if (!g_wifi_connected) {
        printf("[WiFi] Connection timeout!\r\n");
        return -1;
    }

    /* Wait for DHCP */
    usleep(2000000);

    /* Print IP */
    ip4_addr_t ip, nm, gw;
    if (netifapi_netif_get_addr(g_lwip_netif, &ip, &nm, &gw) == ERR_OK) {
        printf("[WiFi] IP: %s\r\n", ip4addr_ntoa(&ip));
    }
    return 0;
}

/* ==================== LED Control ==================== */

static void led_init(void)
{
    IoTGpioInit(LED_CTRL_GPIO);
    IoTGpioSetDir(LED_CTRL_GPIO, IOT_GPIO_DIR_OUT);
    IoTGpioSetOutputVal(LED_CTRL_GPIO, IOT_GPIO_VAL_HIGH);
    printf("[LED] Initialized (GPIO%d)\r\n", LED_CTRL_GPIO);
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

/* ==================== JSON Payload ==================== */

static void build_gas_payload(char *out, int max_len)
{
    float gas = GetGasLevel();
    char gas_str[32];
    snprintf(gas_str, sizeof(gas_str), "%.2f", gas);

    cJSON *root = cJSON_CreateObject();
    cJSON *services = cJSON_AddArrayToObject(root, "services");
    cJSON *service = cJSON_CreateObject();
    cJSON_AddStringToObject(service, "service_id", "GasValue");
    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "gas_value", gas_str);
    cJSON_AddItemToObject(service, "properties", props);
    cJSON_AddItemToArray(services, service);

    char *json = cJSON_PrintUnformatted(root);
    if (json) {
        strncpy(out, json, max_len - 1);
        out[max_len - 1] = '\0';
        cJSON_free(json);
    }
    cJSON_Delete(root);
}

/* ==================== MQTT Command Callback ==================== */

static void messageArrived(MessageData *data)
{
    char *payload = (char *)data->message->payload;
    int payload_len = data->message->payloadlen;
    char *topic = data->topicName->lenstring.data;
    int topic_len = data->topicName->lenstring.len;

    printf("[MQTT] Command on %.*s\r\n", topic_len, topic);
    printf("[MQTT] Payload: %.*s\r\n", payload_len, payload);

    /* Extract request_id from topic */
    char request_id[64] = {0};
    char *p = strstr(topic, "request_id=");
    if (p) {
        char *s = p + 11;
        int i;
        for (i = 0; i < 63 && s[i] && s[i] != '/'; i++) request_id[i] = s[i];
        request_id[i] = '\0';
    }

    cJSON *root = cJSON_ParseWithLength(payload, payload_len);
    if (!root) { printf("[MQTT] JSON parse error\r\n"); return; }

    cJSON *cmd_name = cJSON_GetObjectItem(root, "command_name");
    cJSON *paras = cJSON_GetObjectItem(root, "paras");

    /* Task 2: Parse LED command */
    if (cmd_name && cJSON_IsString(cmd_name) &&
        strcmp(cJSON_GetStringValue(cmd_name), "cmd") == 0) {
        if (paras && cJSON_IsObject(paras)) {
            cJSON *led = cJSON_GetObjectItem(paras, "led");
            if (led && cJSON_IsString(led)) {
                const char *val = cJSON_GetStringValue(led);
                if (strcmp(val, "ON") == 0) led_on();
                else if (strcmp(val, "OFF") == 0) led_off();
                printf("[MQTT] LED: %s\r\n", val);
            }
        }
    }
    cJSON_Delete(root);

    /* Send command response */
    if (request_id[0] != '\0') {
        char resp_topic[256], resp_payload[256];
        MQTTMessage msg;
        snprintf(resp_topic, sizeof(resp_topic),
                 "%s/request_id=%s", RESPONSE_TOPIC, request_id);
        snprintf(resp_payload, sizeof(resp_payload),
                 "{\"result_code\":0,\"response_name\":\"COMMAND_RESPONSE\","
                 "\"paras\":{\"result\":\"success\"}}");
        msg.qos = 0; msg.retained = 0;
        msg.payload = resp_payload; msg.payloadlen = strlen(resp_payload);
        printf("[MQTT] Response sent\r\n");
        MQTTPublish(&g_mqtt_client, resp_topic, &msg);
    }
}

/* ==================== MQTT Main Task ==================== */

static void MQTTDemoTask(void)
{
    int rc;
    printf("[MQTT] === Lab5: Huawei Cloud IoTDA + Gas Sensor ===\r\n");

    GasSensor_Init();
    led_init();

    printf("[MQTT] Connecting WiFi: %s\r\n", PARAM_HOTSPOT_SSID);
    if (wifi_connect() < 0) { printf("[MQTT] WiFi failed!\r\n"); return; }

mqtt_reconnect:
    NetworkInit(&g_network);
    printf("[MQTT] Connecting to %s:%d ...\r\n", HOST_ADDR, HOST_PORT);
    rc = NetworkConnect(&g_network, HOST_ADDR, HOST_PORT);
    if (rc != 0) { osDelay(30); goto mqtt_reconnect; }

    MQTTClientInit(&g_mqtt_client, &g_network, 3000,
                   g_send_buf, sizeof(g_send_buf),
                   g_recv_buf, sizeof(g_recv_buf));

    MQTTPacket_connectData conn = MQTTPacket_connectData_initializer;
    conn.keepAliveInterval = 60;
    conn.cleansession = 1;
    conn.clientID.cstring = MQTT_CLIENT_ID;
    conn.username.cstring = MQTT_USERNAME;
    conn.password.cstring = MQTT_PASSWORD;

    rc = MQTTConnect(&g_mqtt_client, &conn);
    if (rc != 0) { NetworkDisconnect(&g_network); osDelay(30); goto mqtt_reconnect; }
    printf("[MQTT] Connected to IoTDA!\r\n");

    MQTTSubscribe(&g_mqtt_client, SUBCRIB_TOPIC, QOS1, messageArrived);
    printf("[MQTT] Subscribed\r\n");

    int count = 0;
    while (1) {
        char payload[512];
        MQTTMessage msg;
        build_gas_payload(payload, sizeof(payload));
        msg.qos = QOS0; msg.retained = 0;
        msg.payload = payload; msg.payloadlen = strlen(payload);
        printf("[MQTT] Report #%d: %s\r\n", count++, payload);
        rc = MQTTPublish(&g_mqtt_client, PUBLISH_TOPIC, &msg);
        if (rc != 0) {
            NetworkDisconnect(&g_network);
            MQTTDisconnect(&g_mqtt_client);
            osDelay(50); goto mqtt_reconnect;
        }
        MQTTYield(&g_mqtt_client, 5000);
        osDelay(100);
    }
}

static void Lab5Entry(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "MQTTDemoTask";
    attr.stack_size = 10240;
    attr.priority = osPriorityNormal;
    osThreadNew((osThreadFunc_t)MQTTDemoTask, NULL, &attr);
}

APP_FEATURE_INIT(Lab5Entry);

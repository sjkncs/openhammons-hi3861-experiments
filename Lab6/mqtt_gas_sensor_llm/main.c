/**
 * Lab6: AIoT Full Chain - Gas + Temp/Humi + LED + Buzzer + LLM
 *
 * 8 Tasks implemented:
 * Task 1: sensor.h - temperature/humidity fields in SensorData
 * Task 2: sensor.c - InitTempHumiSensor() AHT20 init
 * Task 3: sensor.c - GetTemperatureAndHumidity() sampling
 * Task 4: sensor.c - ReadAllSensorData() unified read
 * Task 5: main.c - temp/humidity string buffers
 * Task 6: main.c - snprintf format to string
 * Task 7: main.c - add temp/humidity to JSON report
 * Task 8: main.c - parse buzzer commands (ALWAYS/FLASH/OFF)
 *
 * Product model:
 *   Service: AirEnvironment
 *   Properties: gas_concentration, temperature, humidity
 *   Command: cmd (paras: led=ON/OFF, buzzer=ALWAYS/FLASH/OFF)
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

#include "wifi_device.h"
#include "wifi_hotspot.h"
#include "lwip/ip_addr.h"
#include "lwip/netifapi.h"
#include "lwip/sockets.h"

#include "MQTTClient.h"
#include "cJSON.h"

#include "sensor.h"
#include "output_control.h"

/*==================== Config ====================*/

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

#define SERVICE_ID "AirEnvironment"

/*================================================*/

static Network g_network;
static MQTTClient g_mqtt_client;
static unsigned char g_send_buf[2000];
static unsigned char g_recv_buf[2000];
static volatile int g_wifi_connected = 0;

/* ==================== WiFi ==================== */

static void wifi_event_cb(const WifiEvent *event)
{
    if (!event) return;
    if (event->event == WIFI_EVT_CONNECTED) {
        printf("[WiFi] Connected\r\n");
        g_wifi_connected = 1;
    } else if (event->event == WIFI_EVT_DISCONNECTED) {
        printf("[WiFi] Disconnected\r\n");
        g_wifi_connected = 0;
    }
}

static int wifi_connect(void)
{
    WifiDeviceConfig config = {0};
    int netId = 0;
    strcpy(config.ssid, PARAM_HOTSPOT_SSID);
    strcpy(config.preSharedKey, PARAM_HOTSPOT_PSK);
    config.securityType = WIFI_SEC_TYPE_PSK;

    WifiInit();
    WifiRegisterCallBack(wifi_event_cb);
    WifiScan();
    usleep(2000000);

    WifiAddDevice(&config, &netId);
    WifiEnableDevice(netId);

    int timeout = 20;
    while (!g_wifi_connected && timeout--) usleep(500000);
    if (!g_wifi_connected) return -1;

    usleep(2000000);
    struct netif *netif = netifapi_netif_find("wlan0");
    if (netif) {
        ip4_addr_t ip, nm, gw;
        netifapi_netif_get_addr(netif, &ip, &nm, &gw);
        printf("[WiFi] IP: %s\r\n", ip4addr_ntoa(&ip));
    }
    return 0;
}

/* ==================== Task 5-7: JSON Report ==================== */

/**
 * get_sensor_data_json - build JSON payload
 * Task 5: string buffers for temp/humidity
 * Task 6: snprintf float to string
 * Task 7: add temp/humidity to properties
 */
static void get_sensor_data_json(char *out, int max_len)
{
    SensorData data = {0};
    ReadAllSensorData(&data);

    /* Task 5: string buffers */
    char gas_str[32];
    char temp_str[32];   /* Task 5 new */
    char humi_str[32];   /* Task 5 new */

    /* Task 6: format to string */
    snprintf(gas_str, sizeof(gas_str), "%.2f", data.gas_concentration);
    snprintf(temp_str, sizeof(temp_str), "%.2f", data.temperature);  /* Task 6 */
    snprintf(humi_str, sizeof(humi_str), "%.2f", data.humidity);     /* Task 6 */

    /* Task 7: build JSON with all 3 properties */
    cJSON *root = cJSON_CreateObject();
    cJSON *services = cJSON_AddArrayToObject(root, "services");
    cJSON *service = cJSON_CreateObject();
    cJSON_AddStringToObject(service, "service_id", SERVICE_ID);

    cJSON *props = cJSON_CreateObject();
    cJSON_AddStringToObject(props, "gas_concentration", gas_str);
    cJSON_AddStringToObject(props, "temperature", temp_str);   /* Task 7 */
    cJSON_AddStringToObject(props, "humidity", humi_str);      /* Task 7 */
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

/* ==================== Task 8: Command Callback ==================== */

/**
 * messageArrived - handle cloud commands
 * Task 8: parse buzzer commands (ALWAYS/FLASH/OFF)
 */
static void messageArrived(MessageData *data)
{
    char *payload = (char *)data->message->payload;
    int payload_len = data->message->payloadlen;
    char *topic = data->topicName->lenstring.data;
    int topic_len = data->topicName->lenstring.len;

    printf("[MQTT] Command on %.*s\r\n", topic_len, topic);
    printf("[MQTT] Payload: %.*s\r\n", payload_len, payload);

    /* Extract request_id */
    char request_id[64] = {0};
    char *p = strstr(topic, "request_id=");
    if (p) {
        char *s = p + 11;
        int i;
        for (i = 0; i < 63 && s[i] && s[i] != '/'; i++) request_id[i] = s[i];
        request_id[i] = '\0';
    }

    cJSON *root = cJSON_ParseWithLength(payload, payload_len);
    if (!root) { printf("[MQTT] JSON error\r\n"); return; }

    cJSON *cmd_name = cJSON_GetObjectItem(root, "command_name");
    cJSON *paras = cJSON_GetObjectItem(root, "paras");

    if (cmd_name && cJSON_IsString(cmd_name) &&
        strcmp(cJSON_GetStringValue(cmd_name), "cmd") == 0 && paras) {

        /* LED control */
        cJSON *led = cJSON_GetObjectItem(paras, "led");
        if (led && cJSON_IsString(led)) {
            const char *val = cJSON_GetStringValue(led);
            if (strcmp(val, "ON") == 0) LedSet(1);
            else if (strcmp(val, "OFF") == 0) LedSet(0);
        }

        /* Task 8: Buzzer control */
        cJSON *buzzer = cJSON_GetObjectItem(paras, "buzzer");
        if (buzzer && cJSON_IsString(buzzer)) {
            const char *val = cJSON_GetStringValue(buzzer);
            if (strcmp(val, "ALWAYS") == 0) {
                BuzzerSet(BUZZER_ALWAYS);
            } else if (strcmp(val, "FLASH") == 0) {
                BuzzerSet(BUZZER_FLASH_ON);
            } else if (strcmp(val, "OFF") == 0) {
                BuzzerSet(BUZZER_OFF);
            }
            printf("[MQTT] Buzzer: %s\r\n", val);
        }
    }

    cJSON_Delete(root);

    /* Send response */
    if (request_id[0]) {
        char resp_topic[256], resp_payload[256];
        MQTTMessage msg;
        snprintf(resp_topic, sizeof(resp_topic),
                 "%s/request_id=%s", RESPONSE_TOPIC, request_id);
        snprintf(resp_payload, sizeof(resp_payload),
                 "{\"result_code\":0,\"response_name\":\"COMMAND_RESPONSE\","
                 "\"paras\":{\"result\":\"success\"}}");
        msg.qos = 0; msg.retained = 0;
        msg.payload = resp_payload; msg.payloadlen = strlen(resp_payload);
        MQTTPublish(&g_mqtt_client, resp_topic, &msg);
        printf("[MQTT] Response sent\r\n");
    }
}

/* ==================== Main Task ==================== */

static void MainTask(void)
{
    printf("[Lab6] === AIoT: Gas+Temp/Humi+LED+Buzzer ===\r\n");

    InitAllSensors();
    OutputControl_Init();

    if (wifi_connect() < 0) {
        printf("[Lab6] WiFi failed!\r\n");
        return;
    }

mqtt_reconnect:
    NetworkInit(&g_network);
    NetworkConnect(&g_network, HOST_ADDR, HOST_PORT);

    MQTTClientInit(&g_mqtt_client, &g_network, 3000,
                   g_send_buf, sizeof(g_send_buf),
                   g_recv_buf, sizeof(g_recv_buf));

    MQTTPacket_connectData conn = MQTTPacket_connectData_initializer;
    conn.keepAliveInterval = 60; conn.cleansession = 1;
    conn.clientID.cstring = MQTT_CLIENT_ID;
    conn.username.cstring = MQTT_USERNAME;
    conn.password.cstring = MQTT_PASSWORD;

    int rc = MQTTConnect(&g_mqtt_client, &conn);
    if (rc != 0) { NetworkDisconnect(&g_network); osDelay(30); goto mqtt_reconnect; }
    printf("[MQTT] Connected to IoTDA!\r\n");

    MQTTSubscribe(&g_mqtt_client, SUBCRIB_TOPIC, QOS1, messageArrived);

    int count = 0, flash_counter = 0;
    while (1) {
        char payload[512];
        MQTTMessage msg;
        get_sensor_data_json(payload, sizeof(payload));
        msg.qos = QOS0; msg.retained = 0;
        msg.payload = payload; msg.payloadlen = strlen(payload);
        printf("[MQTT] Report #%d: %s\r\n", count++, payload);
        rc = MQTTPublish(&g_mqtt_client, PUBLISH_TOPIC, &msg);
        if (rc != 0) {
            NetworkDisconnect(&g_network); MQTTDisconnect(&g_mqtt_client);
            osDelay(50); goto mqtt_reconnect;
        }
        /* Buzzer flash toggle every ~500ms */
        flash_counter++;
        if (flash_counter >= 5) { BuzzerFlashToggle(); flash_counter = 0; }
        MQTTYield(&g_mqtt_client, 5000);
        osDelay(100);
    }
}

static void Lab6Entry(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "MainTask";
    attr.stack_size = 10240;
    attr.priority = osPriorityNormal;
    osThreadNew((osThreadFunc_t)MainTask, NULL, &attr);
}

APP_FEATURE_INIT(Lab6Entry);

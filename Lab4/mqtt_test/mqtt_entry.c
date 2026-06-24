/**
 * Lab4 WiFi 连接模块
 * 连接WiFi热点, 打印IP/网关/子网掩码, 然后启动MQTT测试
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "hi_wifi_api.h"
#include "lwip/ip_addr.h"
#include "lwip/netifapi.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"

#define WIFI_SSID     "oh"
#define WIFI_PASSWORD "12345678"

static struct netif *g_lwip_netif = NULL;
static volatile int wifi_ok_flg = 0;

static void wifi_event_cb(const hi_wifi_event *event)
{
    if (!event) return;
    if (event->event == HI_WIFI_EVT_CONNECTED) {
        printf("[WiFi] Connected\r\n");
        netifapi_dhcp_start(g_lwip_netif);
        wifi_ok_flg = 1;
    } else if (event->event == HI_WIFI_EVT_DISCONNECTED) {
        printf("[WiFi] Disconnected\r\n");
        wifi_ok_flg = 0;
    }
}

int hi_wifi_start_connect(void)
{
    int ret;
    errno_t rc;
    hi_wifi_assoc_request assoc_req = {0};

    rc = memcpy_s(assoc_req.ssid, HI_WIFI_MAX_SSID_LEN + 1, WIFI_SSID, strlen(WIFI_SSID));
    if (rc != EOK) return -1;

    assoc_req.auth = HI_WIFI_SECURITY_WPA2PSK;
    memcpy(assoc_req.key, WIFI_PASSWORD, strlen(WIFI_PASSWORD));

    ret = hi_wifi_sta_connect(&assoc_req);
    if (ret != HISI_OK) return -1;
    return 0;
}

extern void mqtt_test(void);

void wifi_sta_task(void *arg)
{
    (void)arg;

    hi_wifi_init(0);
    hi_wifi_start_sta();
    g_lwip_netif = netifapi_netif_find("wlan0");
    hi_wifi_register_event_callback(wifi_event_cb);
    hi_wifi_start_connect();

    while (wifi_ok_flg == 0) {
        usleep(30000);
    }
    usleep(2000000);

    ip4_addr_t ip = {0}, netmask = {0}, gw = {0};
    err_t ret = netifapi_netif_get_addr(g_lwip_netif, &ip, &netmask, &gw);
    if (ret == ERR_OK) {
        printf("ip = %s\r\n", ip4addr_ntoa(&ip));
        printf("netmask = %s\r\n", ip4addr_ntoa(&netmask));
        printf("gw = %s\r\n", ip4addr_ntoa(&gw));
    }
    printf("netifapi_netif_get_addr: %d\r\n", ret);

    mqtt_test();
}

static void WifiEntry(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "WifiTask";
    attr.stack_size = 4096;
    attr.priority = osPriorityNormal;
    osThreadNew((osThreadFunc_t)wifi_sta_task, NULL, &attr);
}

APP_FEATURE_INIT(WifiEntry);

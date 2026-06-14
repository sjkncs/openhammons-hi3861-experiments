/**
 * Hi3861 TCP 客户端实验
 *
 * 功能: 作为 TCP 客户端连接 PC 的 TCP 服务器，
 *       定时发送心跳消息，并接收服务器下发的指令。
 *
 * 服务器: PC 的 IP 地址 + 端口 (使用 TCP&UDP Debug 工具开启)
 * 协议: TCP
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

/*==================== 可修改配置区 ====================*/

// PC 服务器 IP 和端口（需修改为实际地址）
#define TCP_SERVER_IP   "192.168.43.100"
#define TCP_SERVER_PORT 8080

// WiFi 配置（修改为实际热点）
#define WIFI_SSID     "YourSSID"
#define WIFI_PASSWORD "YourPassword"

/*=====================================================*/

static struct netif *g_lwip_netif = NULL;
static int g_wifi_connected = 0;

#define APP_INIT_VAP_NUM    2
#define APP_INIT_USR_NUM    2

// ==================== WiFi 连接相关 ====================

static void hi_sta_reset_addr(struct netif *pst_lwip_netif)
{
    ip4_addr_t st_gw, st_ipaddr, st_netmask;
    if (pst_lwip_netif == NULL) return;
    IP4_ADDR(&st_gw, 0, 0, 0, 0);
    IP4_ADDR(&st_ipaddr, 0, 0, 0, 0);
    IP4_ADDR(&st_netmask, 0, 0, 0, 0);
    netifapi_netif_set_addr(pst_lwip_netif, &st_ipaddr, &st_netmask, &st_gw);
}

static void wifi_wpa_event_cb(const hi_wifi_event *hisi_event)
{
    if (hisi_event == NULL) return;
    switch (hisi_event->event) {
        case HI_WIFI_EVT_CONNECTED:
            printf("[WiFi] Connected to AP\r\n");
            netifapi_dhcp_start(g_lwip_netif);
            g_wifi_connected = 1;
            break;
        case HI_WIFI_EVT_DISCONNECTED:
            printf("[WiFi] Disconnected\r\n");
            netifapi_dhcp_stop(g_lwip_netif);
            hi_sta_reset_addr(g_lwip_netif);
            g_wifi_connected = 0;
            break;
        default:
            break;
    }
}

static int wifi_start_sta(void)
{
    int ret;
    hi_wifi_assoc_request assoc_req = {0};

    int ssid_len = strlen(WIFI_SSID);
    if (ssid_len > HI_WIFI_MAX_SSID_LEN) ssid_len = HI_WIFI_MAX_SSID_LEN;
    memcpy(assoc_req.ssid, WIFI_SSID, ssid_len);
    assoc_req.auth = HI_WIFI_SECURITY_WPA2PSK;

    int pwd_len = strlen(WIFI_PASSWORD);
    if (pwd_len > HI_WIFI_MAX_KEY_LEN) pwd_len = HI_WIFI_MAX_KEY_LEN;
    memcpy(assoc_req.key, WIFI_PASSWORD, pwd_len);

    ret = hi_wifi_sta_connect(&assoc_req);
    if (ret != HISI_OK) return -1;
    return 0;
}

static int wifi_init(void)
{
    int ret;
    const unsigned char vap_num = APP_INIT_VAP_NUM;
    const unsigned char user_num = APP_INIT_USR_NUM;

    ret = hi_wifi_init(0);
    if (ret != HISI_OK) return -1;

    ret = hi_wifi_start_sta();
    if (ret != HISI_OK) return -1;

    ret = hi_wifi_register_event_callback(wifi_wpa_event_cb);
    if (ret != HISI_OK) return -1;

    g_lwip_netif = netifapi_netif_find("wlan0");
    if (g_lwip_netif == NULL) return -1;

    return wifi_start_sta();
}

// ==================== TCP 客户端任务 ====================

char recv_buf[512];

void tcp_client_thread(void *arg)
{
    (void)arg;
    int sockfd;
    struct sockaddr_in server_addr;
    int retry_count = 0;
    int connected = 0;

    printf("[TCP] Waiting for WiFi connection...\r\n");
    while (!g_wifi_connected) {
        usleep(500000);
    }

    // 等待 DHCP
    printf("[TCP] Waiting for DHCP...\r\n");
    osDelay(100);
    char ip_str[16] = {0};
    ip4_addr_t addr;
    if (netifapi_netif_get_addr(g_lwip_netif, &addr) == 0) {
        snprintf(ip_str, sizeof(ip_str), "%s", ip4addr_ntoa(&addr));
        printf("[TCP] Hi3861 IP: %s\r\n", ip_str);
    }

connect_retry:
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("[TCP] Socket create failed\r\n");
        return;
    }

    // 设置服务器地址
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(TCP_SERVER_PORT);
    inet_pton(AF_INET, TCP_SERVER_IP, &server_addr.sin_addr);

    printf("[TCP] Connecting to %s:%d ...\r\n", TCP_SERVER_IP, TCP_SERVER_PORT);
    int ret = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));

    if (ret < 0) {
        printf("[TCP] Connect failed (errno=%d), retrying in 3s...\r\n", errno);
        close(sockfd);
        retry_count++;
        if (retry_count < 10) {
            osDelay(30);  // 延迟 3 秒
            goto connect_retry;
        } else {
            printf("[TCP] Max retries reached, giving up\r\n");
            return;
        }
    }

    connected = 1;
    printf("[TCP] Connected to server!\r\n");

    // 发送注册消息
    const char *register_msg = "{\"device\":\"hi3861\",\"status\":\"online\"}";
    send(sockfd, register_msg, strlen(register_msg), 0);
    printf("[TCP] Sent: %s\r\n", register_msg);

    // 循环发送心跳 + 接收数据
    while (connected) {
        // 发送心跳
        const char *heartbeat = "{\"type\":\"heartbeat\",\"time\":0}";
        ret = send(sockfd, heartbeat, strlen(heartbeat), 0);
        if (ret < 0) {
            printf("[TCP] Send failed, reconnecting...\r\n");
            connected = 0;
            close(sockfd);
            retry_count = 0;
            goto connect_retry;
        }
        printf("[TCP] Heartbeat sent\r\n");

        // 非阻塞接收（设置超时）
        struct timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 100000;  // 100ms 超时
        setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        memset(recv_buf, 0, sizeof(recv_buf));
        ret = recv(sockfd, recv_buf, sizeof(recv_buf) - 1, 0);
        if (ret > 0) {
            recv_buf[ret] = '\0';
            printf("[TCP] Received: %s\r\n", recv_buf);
        }

        // 每 5 秒发送一次心跳
        osDelay(50);  // 延迟 5 秒
    }

    close(sockfd);
}

// ==================== 应用入口 ====================

static void TCPApp_Entry(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "WiFiInit";
    attr.stack_size = 4096;
    attr.priority = 25;

    if (osThreadNew((osThreadFunc_t)wifi_init, NULL, &attr) == NULL) {
        printf("[APP] Failed to create WiFi task\r\n");
    }
}

static void TCPClient_Entry(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "TCPClient";
    attr.stack_size = 4096;
    attr.priority = 26;

    if (osThreadNew((osThreadFunc_t)tcp_client_thread, NULL, &attr) == NULL) {
        printf("[APP] Failed to create TCP task\r\n");
    }
}

APP_FEATURE_INIT(TCPApp_Entry);
APP_FEATURE_INIT(TCPClient_Entry);

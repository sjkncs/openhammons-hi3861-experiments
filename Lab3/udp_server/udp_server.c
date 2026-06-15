/**
 * Hi3861 UDP 服务器实验
 *
 * 功能: 接收 PC 端 TCP&UDP Debug 工具发送的 JSON 格式命令，
 *       解析并打印到串口，同时向 PC 回复响应消息。
 *
 * 端口: 50001 (可自行修改)
 * 协议: UDP
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

// PC 的 IP 地址（需修改为实际地址）
#define PC_SERVER_IP "192.168.43.100"

// UDP 监听端口
#define UDP_LISTEN_PORT 50001

// WiFi 配置（修改为实际热点）
#define WIFI_SSID     "YourSSID"       // WiFi 名称
#define WIFI_PASSWORD "YourPassword"    // WiFi 密码

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
        case HI_WIFI_EVT_SCAN_DONE:
            printf("[WiFi] Scan done\r\n");
            break;
        default:
            break;
    }
}

static int wifi_start_sta(void)
{
    int ret;
    hi_wifi_assoc_request assoc_req = {0};

    // 复制 SSID
    int ssid_len = strlen(WIFI_SSID);
    if (ssid_len > HI_WIFI_MAX_SSID_LEN) ssid_len = HI_WIFI_MAX_SSID_LEN;
    memcpy(assoc_req.ssid, WIFI_SSID, ssid_len);

    // 加密方式 WPA2PSK
    assoc_req.auth = HI_WIFI_SECURITY_WPA2PSK;

    // 复制密码
    int pwd_len = strlen(WIFI_PASSWORD);
    if (pwd_len > HI_WIFI_MAX_KEY_LEN) pwd_len = HI_WIFI_MAX_KEY_LEN;
    memcpy(assoc_req.key, WIFI_PASSWORD, pwd_len);

    ret = hi_wifi_sta_connect(&assoc_req);
    if (ret != HISI_OK) {
        printf("[WiFi] Connect failed: %d\r\n", ret);
        return -1;
    }
    return 0;
}

static int wifi_init_and_connect(void)
{
    int ret;
    char ifname[WIFI_IFNAME_MAX_SIZE + 1] = {0};

    ret = hi_wifi_init(0);
    if (ret != HISI_OK) {
        printf("[WiFi] Init failed: %d\r\n", ret);
        return -1;
    }

    ret = hi_wifi_start_sta();
    if (ret != HISI_OK) {
        printf("[WiFi] Start STA failed\r\n");
        return -1;
    }

    ret = hi_wifi_register_event_callback(wifi_wpa_event_cb);
    if (ret != HISI_OK) {
        printf("[WiFi] Register event cb failed\r\n");
        return -1;
    }

    // 获取 netif
    g_lwip_netif = netifapi_netif_find("wlan0");
    if (g_lwip_netif == NULL) {
        printf("[WiFi] netif not found\r\n");
        return -1;
    }

    return wifi_start_sta();
}

// ==================== UDP 服务器任务 ====================

char recv_buffer[1024];

void udp_server_thread(void *arg)
{
    (void)arg;
    int sockfd;
    struct sockaddr_in servaddr, clientaddr;
    int ret;

    printf("[UDP] Waiting for WiFi connection...\r\n");
    while (!g_wifi_connected) {
        usleep(500000);  // 等待 500ms
    }

    // 等待 DHCP 获取 IP
    printf("[UDP] Waiting for DHCP...\r\n");
    osDelay(100);  // 延迟 5 秒等待 DHCP
    char ip_str[16] = {0};
    ip4_addr_t addr;
    if (netifapi_netif_get_addr(g_lwip_netif, &addr) == 0) {
        snprintf(ip_str, sizeof(ip_str), "%s", ip4addr_ntoa(&addr));
        printf("[UDP] Hi3861 IP: %s\r\n", ip_str);
    }

    // 创建 UDP 套接字
    sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        printf("[UDP] Socket create failed\r\n");
        return;
    }

    // 设置地址复用
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 绑定地址和端口
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(UDP_LISTEN_PORT);

    ret = bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
    if (ret < 0) {
        printf("[UDP] Bind failed\r\n");
        close(sockfd);
        return;
    }

    printf("[UDP] Server started, listening on port %d\r\n", UDP_LISTEN_PORT);
    printf("[UDP] Waiting for commands from PC...\r\n");

    // 循环接收数据
    while (1) {
        socklen_t client_len = sizeof(clientaddr);
        memset(&clientaddr, 0, sizeof(clientaddr));
        memset(recv_buffer, 0, sizeof(recv_buffer));

        ret = recvfrom(sockfd, recv_buffer, sizeof(recv_buffer) - 1,
                       0, (struct sockaddr *)&clientaddr, &client_len);

        if (ret > 0) {
            recv_buffer[ret] = '\0';
            char *client_ip = inet_ntoa(clientaddr.sin_addr);
            unsigned short client_port = ntohs(clientaddr.sin_port);

            printf("[UDP] %s:%d says: %s\r\n",
                   client_ip, client_port, recv_buffer);

            // 简单的命令解析（实际项目可用 cJSON）
            // 格式: {"cmd":"forward"} 或 {"mode":"step"}
            if (strstr(recv_buffer, "\"cmd\"")) {
                if (strstr(recv_buffer, "forward")) {
                    printf("  -> CMD: FORWARD\r\n");
                } else if (strstr(recv_buffer, "backward")) {
                    printf("  -> CMD: BACKWARD\r\n");
                } else if (strstr(recv_buffer, "left")) {
                    printf("  -> CMD: LEFT\r\n");
                } else if (strstr(recv_buffer, "right")) {
                    printf("  -> CMD: RIGHT\r\n");
                } else if (strstr(recv_buffer, "stop")) {
                    printf("  -> CMD: STOP\r\n");
                } else {
                    printf("  -> Unknown cmd\r\n");
                }
            } else if (strstr(recv_buffer, "\"mode\"")) {
                if (strstr(recv_buffer, "step")) {
                    printf("  -> MODE: STEP\r\n");
                } else if (strstr(recv_buffer, "alway")) {
                    printf("  -> MODE: ALWAYS\r\n");
                }
            } else {
                printf("  -> Unknown message format\r\n");
            }

            // 向 PC 回复响应
            const char *response = "{\"result\":\"ok\"}";
            sendto(sockfd, response, strlen(response), 0,
                   (struct sockaddr *)&clientaddr, sizeof(clientaddr));
            printf("[UDP] Response sent: %s\r\n", response);
        }
    }

    close(sockfd);
}

// ==================== 应用入口 ====================

static void UDPApp_Entry(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "WiFiConnect";
    attr.stack_size = 4096;
    attr.priority = 25;

    if (osThreadNew((osThreadFunc_t)wifi_init_and_connect, NULL, &attr) == NULL) {
        printf("[APP] Failed to create WiFi task\r\n");
    }
}

static void UDPServer_Entry(void)
{
    osThreadAttr_t attr = {0};
    attr.name = "UDPServer";
    attr.stack_size = 4096;
    attr.priority = 26;

    if (osThreadNew((osThreadFunc_t)udp_server_thread, NULL, &attr) == NULL) {
        printf("[APP] Failed to create UDP task\r\n");
    }
}

// 模块初始化，自动启动
APP_FEATURE_INIT(UDPApp_Entry);
APP_FEATURE_INIT(UDPServer_Entry);

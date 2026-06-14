# Lab3: TCP/UDP 通讯实验

## 实验目的

1. 掌握 Hi3861 芯片的 lwIP 网络编程接口
2. 理解 UDP 广播/单播的数据收发机制
3. 理解 TCP 客户端连接服务器的工作流程
4. 学会使用 PC 端 TCP&UDP Debug 工具进行网络调试

## 实验环境

| 项目 | 说明 |
|------|------|
| 开发板 | HiSpark Pegasus (Hi3861) |
| 协议栈 | lwIP (已集成在 OpenHarmony) |
| PC工具 | TCP&UDP Debug (见 `pc_tool/` 目录) |
| 通讯模式 | UDP 单播/广播 + TCP 客户端 |

## 硬件连接

```
PC (WiFi热点)  <---WiFi--->  Hi3861开发板
   |                         |
   +------ TCP/UDP --------+
         同一局域网
```

- PC 与 Hi3861 连接同一 WiFi 热点
- Hi3861 作为 TCP 客户端连接 PC 的 TCP 服务器
- Hi3861 作为 UDP 端，PC 可向其发送 UDP 广播/单播

## 实验原理

### UDP 通讯

UDP (User Datagram Protocol) 是无连接的传输层协议，无需建立连接即可发送数据报。

```
Hi3861 (UDP Server)                    PC (UDP Client)
    |                                        |
    |  recvfrom() <--  UDP数据报 --------+  |
    |  sendto()  -->  UDP数据报 --------+  |
```

**关键 API:**
- `socket(PF_INET, SOCK_DGRAM, 0)` — 创建 UDP 套接字
- `bind()` — 绑定 IP 和端口
- `recvfrom()` — 接收数据
- `sendto()` — 发送数据

### TCP 通讯

TCP (Transmission Control Protocol) 是面向连接的可靠传输协议。

```
Hi3861 (TCP Client)                     PC (TCP Server)
    |                                        |
    |  connect()  --> SYN --------->         |
    |  <-- SYN+ACK ---                       |
    |  send()   --> 数据 --------->          |
    |  recv()   <-- 数据 <--------           |
```

**关键 API:**
- `socket(PF_INET, SOCK_STREAM, 0)` — 创建 TCP 套接字
- `connect()` — 连接服务器
- `send()` — 发送数据
- `recv()` — 接收数据

## 代码说明

### UDP 服务器 (udp_server/)

Hi3861 作为 UDP 服务器，监听指定端口，接收 PC 发来的 JSON 格式命令并解析执行。

**核心流程:**
```c
// 1. 创建 UDP 套接字
int sockfd = socket(PF_INET, SOCK_DGRAM, 0);

// 2. 绑定地址和端口
bind(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

// 3. 循环接收并处理数据
while(1) {
    recvfrom(sockfd, recvline, 1024, 0,
             (struct sockaddr*)&addrClient, &size);
    // 解析 JSON 命令
    // 处理 forward/backward/left/right/stop
    // 发送响应
    sendto(sockfd, response, len, 0,
           (struct sockaddr*)&addrClient, sizeof(addrClient));
}
```

**支持的命令 (JSON 格式):**
```json
{"cmd": "forward"}    // 前进
{"cmd": "backward"}   // 后退
{"cmd": "left"}       // 左转
{"cmd": "right"}      // 右转
{"cmd": "stop"}       // 停止
{"cmd": "query"}      // 查询状态
{"mode": "step"}      // 步进模式
{"mode": "alway"}     // 持续模式
```

### TCP 客户端 (tcp_client/)

Hi3861 作为 TCP 客户端，连接 PC 的 TCP 服务器并发送数据。

**核心流程:**
```c
// 1. 创建 TCP 套接字
int sockfd = socket(PF_INET, SOCK_STREAM, 0);

// 2. 设置服务器地址
servaddr.sin_family = AF_INET;
servaddr.sin_port = htons(SERVER_PORT);
inet_pton(AF_INET, SERVER_IP, &servaddr.sin_addr);

// 3. 连接服务器
connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));

// 4. 发送/接收数据
send(sockfd, data, len, 0);
recv(sockfd, buffer, size, 0);
```

## PC 端操作步骤

### 步骤 1: 配置 PC 热点

将 PC 的 WiFi 设为热点，或连接一个路由器，确保 PC 和 Hi3861 在同一网络。

### 步骤 2: 获取 PC 的 IP 地址

```cmd
ipconfig
```

记下 IPv4 地址，如 `192.168.43.100`。

### 步骤 3: 运行 TCP&UDP Debug 工具

1. 双击运行 `pc_tool/TCPUDPDbg.exe`
2. 选择 **UDP** 或 **TCP** 模式
3. 输入 IP 地址和端口
4. 点击"连接"或"打开"

### 步骤 4: 发送命令测试

在 TCP&UDP Debug 工具中发送以下内容，观察 Hi3861 串口输出:

```
{"cmd":"forward"}
{"cmd":"stop"}
```

## 编译与烧录

### 编译

在 Hi3861 SDK 目录下执行:

```bash
hb build -f
```

### 烧录

使用 HiBurn 工具烧录 `out/` 目录下的 `Hi3861_demo.bin` 文件。

### 串口观察

使用 SSCOM 或其他串口工具连接开发板，波特率 115200，观察日志输出。

## 串口预期输出

```
wifi connected!
udp_thread started, port: 50001
192.168.43.100-1234 says: {"cmd":"forward"}
cmd : forward
forward
192.168.43.100-1234 says: {"cmd":"stop"}
cmd : stop
stop
```

## 实验扩展

1. **添加心跳机制**: 定期发送心跳包检测连接状态
2. **多客户端支持**: 使用 select/poll 处理多个连接
3. **数据加密**: 使用 TLS/DTLS 加密传输
4. **UDP 广播发现**: 开发板广播自身状态，PC 自动发现

## 注意事项

- 修改代码中的 PC IP 地址为实际地址
- 确保防火墙允许相应端口的通讯
- WiFi 热点推荐使用 2.4GHz 频段（Hi3861 不支持 5GHz）

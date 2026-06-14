# Lab4: 智能家居综合实验

## 实验目的

1. 掌握 Hi3861 GPIO 外设的编程方法（LED、按键）
2. 掌握 Hi3861 WiFi STA 模式连接热点的流程
3. 理解 MQTT 协议的基本原理和发布/订阅模型
4. 掌握在 OpenHarmony 上使用 Paho MQTT 库实现 MQTT 客户端
5. 学会使用 mosquitto 本地消息代理进行 MQTT 调试

## 实验环境

| 项目 | 说明 |
|------|------|
| 开发板 | HiSpark Pegasus (Hi3861) |
| MQTT Broker | mosquitto (Windows/Linux) |
| MQTT 客户端工具 | MQTTX (Windows) 或 mosquitto_sub/pub |
| 依赖库 | Paho MQTT, cJSON |

## 实验内容

本实验包含以下子实验，循序渐进：

```
Lab4
├── 4.1 LED 闪烁实验     —— GPIO 输出控制
├── 4.2 按键输入实验     —— GPIO 输入检测
├── 4.3 WiFi STA 实验    —— 连接热点获取 IP
├── 4.4 OLED 显示实验    —— I2C 屏幕驱动
├── 4.5 本地 MQTT 实验   —— mosquitto 消息代理
└── 4.6 综合智能家居     —— 小车 + MQTT 远程控制
```

## 实验原理

### GPIO 控制

Hi3861 芯片有多个 GPIO 引脚，可配置为输入或输出模式：

```c
// 初始化 GPIO
IoTGpioInit(GPIO9);

// 设置为输出
IoTGpioSetDir(GPIO9, IOT_GPIO_DIR_OUT);

// 输出高/低电平
IoTGpioSetOutputVal(GPIO9, IOT_GPIO_VAL_HIGH);  // LED 灭
IoTGpioSetOutputVal(GPIO9, IOT_GPIO_VAL_LOW);   // LED 亮
```

### 按键检测

按键按下时，GPIO 被拉低；松开时，被拉高（取决于电路连接方式）。

```c
// 设置为输入
IoTGpioSetDir(KEY_GPIO, IOT_GPIO_DIR_IN);

// 读取按键值
unsigned int val;
IoTGpioGetInputVal(KEY_GPIO, &val);
```

### MQTT 协议

MQTT (Message Queuing Telemetry Transport) 是一种轻量级的发布/订阅消息协议：

```
Publisher                  Broker                  Subscriber
   |                         |                        |
   |  publish("topicA")      |                        |
   +------------------------>|+---------------------->|
   |                         |   deliver("topicA")    |
   |                         |                        |
```

**核心概念:**
- **Broker**: 消息代理服务器（如 mosquitto、华为云 IoTDA）
- **Topic**: 主题，消息的分类路径（如 `home/livingroom/led`）
- **Publisher**: 发布者，向主题发送消息
- **Subscriber**: 订阅者，接收订阅主题的消息
- **QoS**: 服务质量（0=最多一次，1=至少一次，2=恰好一次）

## 子实验详解

### 4.1 LED 闪烁

Hi3861 开发板上有一颗连接到 GPIO9 的 LED。程序通过控制 GPIO9 的输出电平，实现 LED 闪烁。

```c
void *LedTask(const char *arg)
{
    IoTGpioInit(LED_GPIO);
    IoTGpioSetDir(LED_GPIO, IOT_GPIO_DIR_OUT);

    while (1) {
        IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_LOW);  // 亮
        usleep(300000);  // 300ms
        IoTGpioSetOutputVal(LED_GPIO, IOT_GPIO_VAL_HIGH); // 灭
        usleep(300000);  // 300ms
    }
}
```

### 4.2 按键输入

使用 ADC 按键（ADC Key），通过 ADC 采样判断按下的按键。

```c
uint32_t AdcKeyRead(void)
{
    unsigned short data = 0;
    hi_adc_read(HI_ADC_CHANNEL_3, &data, HI_ADC_EQU_MODEL_4,
                HI_ADC_CUR_BAIS_DEFAULT, 0);
    return data;
}
```

### 4.3 WiFi STA

WiFi STA 模式让 Hi3861 作为客户端连接到热点路由器：

```c
hi_wifi_init();
hi_wifi_start_sta();
hi_wifi_register_event_callback(wifi_event_cb);
hi_wifi_sta_connect(&assoc_req);
netifapi_dhcp_start(g_lwip_netif);
```

### 4.4 OLED 显示

使用 SSD1306 0.96 寸 OLED 屏幕（I2C 接口），在屏幕上显示文字和图形：

```c
ssd1306_Init();
ssd1306_Fill(Black);
ssd1306_SetCursor(0, 0);
ssd1306_String("Hello OpenHarmony!");
ssd1306_UpdateScreen();
```

### 4.5 本地 MQTT

使用 mosquitto 作为本地消息代理，实现 MQTT 通讯：

**mosquitto 配置** (`mosquitto.conf`):
```conf
allow_anonymous true
listener 1883
```

**订阅主题**:
```bash
mosquitto_sub -t "ohospub" -v
```

**发布消息**:
```bash
mosquitto_pub -t "ohossub" -m "123456"
```

**MQTTX 工具使用**:
1. 连接 `localhost:1883`
2. 订阅 `ohospub`
3. 发布到 `ohossub`
4. 观察双向消息收发

### 4.6 智能小车综合

结合 UDP 通讯和 MQTT，实现远程控制小车：

```
PC (MQTTX) --> mosquitto --> Hi3861 --> 小车电机控制
              |
              +--> Hi3861 --> PC (TCP&UDP Debug)
```

## 代码文件说明

| 文件 | 功能 |
|------|------|
| `led_demo/` | LED 闪烁 |
| `key_demo/` | ADC 按键读取 |
| `demo_wifi_sta/` | WiFi STA 连接热点 |
| `ssd1306/` | OLED SSD1306 驱动 |
| `mqtt_test/` | MQTT 基本实验（华为云 OneNET） |
| `mqtt_local/` | MQTT 本地实验（mosquitto） |
| `car_mqtt/` | MQTT 控制智能小车 |

## 编译方法

```bash
# 完整编译
hb build -f

# 或单独编译某个模块
hb build -f --target led_demo
```

## 实验步骤

### 步骤 1: 启动 mosquitto

```bash
# Windows
mosquitto -c mosquitto.conf -v

# Linux/macOS
mosquitto -c mosquitto.conf -v
```

### 步骤 2: 启动 MQTTX

1. 下载 MQTTX: https://mqttx.app/
2. 新建连接，填写:
   - Name: `local-mqtt`
   - Host: `localhost`
   - Port: `1883`
3. 点击连接

### 步骤 3: 订阅和发布

**订阅 `ohospub`:**
- Topic: `ohospub`
- QoS: 1

**发布到 `ohossub`:**
- Topic: `ohossub`
- Message: `hello from MQTTX`

### 步骤 4: 观察结果

在 Hi3861 串口输出中观察:
- 接收到的消息: `message is hello from MQTTX`
- 定时发布: `openharmony` (每 1 秒)

## 扩展实验

1. **结合 LED**: 订阅 `led/control`，控制 LED 开关
2. **定时上报**: 定时将温度/光强数据发布到 `sensor/temperature`
3. **OLED 显示**: 将收到的 MQTT 消息显示在 OLED 屏幕上
4. **多设备**: 多台 Hi3861 订阅同一主题，实现广播控制

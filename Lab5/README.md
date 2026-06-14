# Lab5: 华为云 IoTDA + MQTT + 燃气传感器

## 实验目的

1. 掌握华为云 IoTDA 平台的设备接入流程
2. 理解 MQTT 协议在物联网云平台中的应用
3. 掌握 MQ-2 可燃气体传感器的使用方法
4. 学会实现端云协同的物联网应用（数据上报 + 命令下发）
5. 掌握 cJSON 库的 JSON 格式封装与解析

## 实验环境

| 项目 | 说明 |
|------|------|
| 开发板 | HiSpark Pegasus + 环境监测板 (Hi3861 + MQ-2) |
| 云平台 | 华为云 IoTDA (设备接入服务) |
| 协议 | MQTT over TCP (端口 1883) |
| 数据格式 | JSON (华为云标准物模型) |

## 华为云 IoTDA 平台配置

### 步骤 1: 创建 IoTDA 实例

1. 登录 [华为云控制台](https://console.huaweicloud.com/iotda/)
2. 选择 "设备接入" 服务
3. 创建标准版实例（或使用已有的）
4. 记下 **接入地址** (形如 `a16xxxxxxxxx.iot-mqtts.cn-north-4.myhuaweicloud.com`，**不要直接 commit 真实地址**)

### 步骤 2: 注册设备

1. 进入 "设备" -> "注册设备"
2. 选择产品（没有则先创建产品）
3. 填写设备信息:
   - 设备 ID: 如 `my_gas_sensor_001`
   - 设备名称: 如 `燃气传感器001`
4. **重要**: 保存生成的 **设备密钥** (Device Secret)

### 步骤 3: 产品模型定义

在产品中定义服务（服务 ID）和属性：

**服务: GasValue (燃气监测)**
| 属性名 | 数据类型 | 描述 |
|--------|----------|------|
| gas_value | string | 燃气传感器值 (kΩ) |

### 步骤 4: 获取连接信息

在设备详情页获取:
- **设备 ID** (Device ID)
- **设备密钥** (Device Secret)
- **接入地址** (从实例信息获取)

然后使用设备 ID 和密钥，通过华为云提供的工具生成:
- **Client ID**: `{device_id}_0_0_{timestamp}`
- **用户名**: `{device_id}`
- **密码**: 通过 `HMAC-SHA256` 算法，用设备密钥对 `clientId{device_id}...` 签名生成

## MQTT 连接参数

```c
// 华为云 IoTDA MQTT 接入地址（占位符, 真实地址请通过 menuconfig 注入）
#define HOST_ADDR     "YOUR_HUAWEI_CLOUD_MQTT_BROKER"
#define HOST_PORT     1883

// 设备信息（占位符, 真实凭证请通过 menuconfig 注入, 不要 commit 真实值）
#define DEVICE_ID     "YOUR_DEVICE_ID"
#define MQTT_CLIENT_ID "YOUR_CLIENT_ID"
#define MQTT_USERNAME "YOUR_USERNAME"
#define MQTT_PASSWORD "YOUR_HUAWEI_CLOUD_PASSWORD"

// 华为云标准 Topic 格式
#define PUBLISH_TOPIC   "$oc/devices/" DEVICE_ID "/sys/properties/report"  // 属性上报
#define SUBCRIB_TOPIC   "$oc/devices/" DEVICE_ID "/sys/commands/#"          // 命令订阅
#define RESPONSE_TOPIC  "$oc/devices/" DEVICE_ID "/sys/commands/response"   // 命令响应
```

## MQTT 消息格式

### 属性上报 (Hi3861 -> 云平台)

```json
{
  "services": [{
    "service_id": "GasValue",
    "properties": {
      "gas_value": "12.56"
    }
  }]
}
```

### 命令下发 (云平台 -> Hi3861)

```json
{
  "command_name": "cmd",
  "service_id": "GasValue",
  "paras": {
    "led": "ON"
  }
}
```

### 命令响应 (Hi3861 -> 云平台)

```json
{
  "result_code": 0,
  "response_name": "COMMAND_RESPONSE",
  "paras": {
    "result": "success"
  }
}
```

## MQ-2 燃气传感器原理

MQ-2 传感器是一种基于金属氧化物半导体的气敏元件，空气中可燃气体浓度增加时，传感器的电阻值降低。

```
  Vcc (5V)  ──┬── [MQ-2 传感器] ──+── GND
               │                      │
           (ADC 采样点)              1kΩ
               │                      │
              ADC5                  1kom
```

**计算公式:**
```
Vx / 5 == 1k / (1k + Rx)
=> Rx = 5/Vx - 1  (kΩ)
```

其中 `Vx` 为 ADC 采样点电压，当空气中燃气浓度增加时:
- 传感器电阻 Rx 降低
- 分压 Vx 增大
- ADC 值增大

## 代码架构

```
mqtt_gas_sensor/
├── main.c          - MQTT 主逻辑：连接、上报、命令处理
├── gas_sensor.c    - MQ-2 传感器驱动：ADC 读取
├── gas_sensor.h    - 传感器接口声明
└── BUILD.gn       - 编译配置
```

**主流程:**
1. 初始化 GPIO 和 ADC
2. 连接 WiFi 热点
3. 连接华为云 MQTT Broker
4. 订阅命令主题
5. 循环: 读取传感器数据 → JSON 封装 → 上报到云端
6. 收到命令 → 解析 → 控制 LED → 回复响应

## 编译与烧录

```bash
# 1. 将代码文件复制到 Hi3861 SDK 的 applications/sample/wifi-iot/app/ 目录

# 2. 修改 app/BUILD.gn，添加:
static_library("mqtt_gas_sensor") {
    sources = [ ... ]
    deps = [ ... ]
}

# 3. 编译
hb build -f

# 4. 使用 HiBurn 烧录
```

## 云平台验证

### 1. 查看设备在线状态

在华为云 IoTDA 控制台 "设备" 页面，确认设备状态为 **在线**。

### 2. 查看属性上报

在 "设备详情" -> "设备影子" 或 "历史数据" 中查看:
- service_id: `GasValue`
- gas_value: 不断变化的传感器数值

### 3. 下发命令

在云平台发送命令:
```json
{
  "command_name": "cmd",
  "service_id": "GasValue",
  "paras": {
    "led": "ON"
  }
}
```

预期结果:
- Hi3861 串口输出: `LED ON!`
- 云平台收到响应: `{"result_code":0,"response_name":"COMMAND_RESPONSE",...}`

## 已知问题与修复

### 问题: gas_sensor.c 中 GetGasLevel() 返回值错误

原始代码直接返回了 ADC 原始值 `return data`，而不是计算后的传感器电阻值。

**修复方案:** 返回传感器电阻值（kΩ），供 JSON 封装使用:

```c
float GetGasLevel()
{
    unsigned short data = 0;
    if (hi_adc_read(...) == HI_ERR_SUCCESS) {
        float Vx = (float)data * 1.8 * 4 / 4096;
        float Rx = 5 / Vx - 1;  // 传感器电阻 kΩ
        return Rx;  // 修正: 返回计算值而非原始 ADC 值
    }
    return 0;
}
```

### 问题: LED 控制未实现

原始代码的 LED 控制部分为 TODO 空实现。

**修复方案:** 在 `main.c` 的 `messageArrived()` 回调中实现 GPIO 控制。

## 实验扩展

1. **多传感器**: 添加温湿度传感器 (DHT11)、光敏传感器
2. **数据可视化**: 在华为云 IoTDA 控制台配置数据仪表盘
3. **规则引擎**: 设置告警规则，当燃气值超过阈值时自动触发短信/邮件通知
4. **命令下发**: 云端下发配置参数（如上报间隔）给设备
5. **OTA 升级**: 通过华为云实现固件远程升级

## 参考资料

- [华为云 IoTDA 文档](https://support.huaweicloud.com/iotdm/)
- [Paho MQTT 官方仓库](https://github.com/eclipse/paho.mqtt.embedded-c)
- [Hi3861 SDK 文档](https://gitee.com/hihope_iot/HiHope_Pegasus_Doc)

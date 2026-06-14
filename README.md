# openharmons-hi3861-experiments

> OpenHarmony Hi3861 轻量系统开发 — 课程实验报告与源码  
> 学号 2023315113 · 姓名 宋阳霆 · 班级 城规一

本仓库包含 26 春《OpenHarmony Hi3861 轻量系统开发》课程的 4 个实验代码与实验报告，对应课程 4 个考核项：

| 课程实验 | 主题 | 源码目录 |
|:---------|:-----|:---------|
| 实验二 | 环境感知与交互控制 | [`Lab2/`](./Lab2) |
| 实验三 | Wi-Fi 和网络编程 | [`Lab3/`](./Lab3) |
| 实验四 | MQTT 协议编程 | [`Lab4/`](./Lab4) |
| 实验五 | 设备上云（华为云 IoTDA） | [`Lab5/`](./Lab5) |

实验报告正文：[`report/实验报告.md`](./report/实验报告.md)

## 硬件平台

- **开发板**：HiSpark Pegasus (Hi3861)
- **主控**：海思 Hi3861V100，160 MHz，352 KB SRAM
- **操作系统**：OpenHarmony LiteOS-M
- **传感器**（仅 Lab5）：MQ-2 可燃气体传感器（ADC5 通道）
- **外设**：板载 LED（GPIO9）、ADC 按键（ADC3 通道）

## 仓库结构

```
openhammons-hi3861-experiments/
├── README.md                          # 本文件
├── LICENSE                            # MIT
├── .gitignore
├── report/
│   └── 实验报告.md                     # 4 个实验合并报告（约 30 KB）
├── Lab2/                              # 实验二：环境感知与交互控制
│   ├── key_demo/                      #   LED 闪烁 + ADC 按键 + 消抖
│   └── README.md
├── Lab3/                              # 实验三：Wi-Fi 和网络编程
│   ├── udp_server/                    #   Hi3861 UDP 服务器
│   ├── tcp_client/                    #   Hi3861 TCP 客户端
│   ├── pc_tool/                       #   PC 端 TCP&UDP Debug 工具说明
│   └── README.md
├── Lab4/                              # 实验四：MQTT 协议编程
│   ├── key_demo/                      #   LED + ADC 按键（基础）
│   ├── mqtt_local/                    #   Paho MQTT + 本地 mosquitto
│   ├── smart_home/                    #   智能家居综合：LED+按键+MQTT
│   ├── mosquitto.conf                 #   本地 Broker 配置
│   └── README.md
├── Lab5/                              # 实验五：设备上云
│   ├── mqtt_gas_sensor/
│   │   ├── main.c                     #   基础版：华为云 IoTDA + MQ-2
│   │   ├── main_v2_smart_scene.c      #   优化版：智能场景联动
│   │   ├── gas_sensor.c / .h          #   MQ-2 驱动
│   │   └── BUILD.gn
│   └── README.md
├── config_template.h                  # WiFi / 华为云凭证统一模板
├── docs/
│   └── 环境搭建指南.md                 # WSL2 / Ubuntu 编译环境
└── tools/
    └── web_control_panel.py           # 可选：Flask + MQTT 可视化面板
```

## 编译方法

需要 OpenHarmony Hi3861 SDK（参考 `docs/环境搭建指南.md`）：

```bash
# 在 SDK 根目录
hb build -f                       # 全量编译
hb build -f --target mqtt_gas_sensor   # 单独编译某个模块
```

编译产物在 `out/hispark_pegasus/wifiiot_app.bin`，使用 HiBurn 烧录（115200 bps）。

## WiFi / 华为云凭证

使用前请修改对应 `.c` 文件顶部配置区：

```c
#define WIFI_SSID     "YourSSID"
#define WIFI_PASSWORD "YourPassword"
#define HOST_ADDR     "YOUR_HUAWEI_CLOUD_MQTT_BROKER"
#define DEVICE_ID     "your_device_id"
#define MQTT_PASSWORD "your_hmac_sha256_password"
```

也可以使用 `config_template.h` 集中管理。

## 引用

```bibtex
@misc{openhammons_hi3861_2026,
  author = {sjkncs},
  title  = {OpenHarmony Hi3861 轻量系统开发实验},
  year   = {2026},
  url    = {https://github.com/sjkncs/openhammons-hi3861-experiments}
}
```

## License

MIT — see [`LICENSE`](./LICENSE).

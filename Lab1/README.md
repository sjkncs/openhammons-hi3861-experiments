# Lab1 实验代码

> 课程导论与开发环境搭建

## 实验要求

### 任务一：开发环境搭建及Hello程序改造

- 将Hello程序的启动方式从`SYS_RUN`改为`APP_FEATURE_INIT`
- 在输出中显示学号和姓名

### 任务二：消息队列与时间管理

- 集成消息队列（MessageQueue）与延时（Delay）样例
- 实现带时间戳的消息延迟分析
- 创建多个消息发送者和消息
- 在发送者线程中发送消息前添加时间戳记录
- 在接收者线程中接收消息后计算时间差

## 目录结构

```
Lab1/
├── BUILD.gn                    # 编译配置文件
├── README.md                   # 本文件
│
├── task1_hello/               # 任务一
│   └── hello_world.c          # Hello程序（APP_FEATURE_INIT启动）
│
└── task2_msgqueue/            # 任务二
    └── msg_queue_demo.c       # 消息队列示例
```

## 编译说明

### 方法一：复制到SDK

```bash
# 1. 将代码复制到Hi3861 SDK
cp task1_hello/hello_world.c ~/hi3861_sdk/app/my_first_app/hello_world.c
cp task2_msgqueue/msg_queue_demo.c ~/hi3861_sdk/app/my_msgqueue/msg_queue_demo.c

# 2. 修改学号和姓名
# 编辑 hello_world.c 中的 student_id 和 student_name

# 3. 编译
cd ~/hi3861_sdk
hb build -f
```

### 方法二：完整编译

将整个`Lab1`目录复制到SDK的`applications/sample/wifi-iot/app/`目录下：

```bash
# 假设SDK目录结构
# applications/sample/wifi-iot/app/
#     ├── my_first_app/
#     │   ├── hello_world.c
#     │   └── BUILD.gn
#     └── my_msgqueue/
#         ├── msg_queue_demo.c
#         └── BUILD.gn
```

## 预期输出

### 任务一输出

```
=============================================
       OpenHarmony Hi3861 实验报告
=============================================
学号: 2023315113
姓名: 宋阳霆
---------------------------------------------
Welcome to OpenHarmony World!
Hi3861 设备初始化成功
=============================================
```

### 任务二输出

```
=============================================
   Lab1 任务二：消息队列与时间管理实验
=============================================
发送者数量: 3
每发送者消息数: 5
总消息数: 15
---------------------------------------------
[INFO] 消息队列创建成功 (队列深度: 16)
[Sender0] 线程启动 (ID: 0x20001000)
[Sender1] 线程启动 (ID: 0x20002000)
[Sender2] 线程启动 (ID: 0x20003000)
[Receiver] 线程启动 (ID: 0x20004000)
[Sender0] 发送: Msg#0 from Sender0 [时间戳: 0]
[Receiver] 接收: Msg#0 from Sender0
           -> 发送时间: 0, 接收时间: 5
           -> 延迟: 5 us
           -> 延迟等级: 正常
...
=============================================
           消息队列统计信息
=============================================
发送消息数: 15
接收消息数: 15
平均延迟: xxx us
=============================================
```

## 实验原理

### 启动方式对比

| 启动方式 | 说明 | 适用场景 |
|----------|------|----------|
| SYS_RUN | 内核刚启动就跑，系统服务未就绪 | 简单程序 |
| APP_FEATURE_INIT | 应用层规范入口，系统环境准备就绪后执行 | 正式应用 |

### 消息队列

- **MessageQueue**：提供先进先出(FIFO)的消息传递机制
- **用途**：实现线程间的通信，解耦&异步
- **示例**：传感器线程往队列塞数据，网络线程从队列取数据上云


## 资料

- [OpenHarmony官方文档](https://www.openharmony.cn/)
- [润和HiHope Pegasus文档](https://gitee.com/hihope_iot/HiHope_Pegasus_Doc)
- [课程实验指导书](https://ohiot.p.cs-lab.top/)

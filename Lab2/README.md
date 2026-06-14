# Lab2 - 实验二：环境感知与交互控制

> 课程对应：环境感知与交互控制实验

## 实验目的

1. 掌握 Hi3861 GPIO 外设的编程方法（LED 输出 / ADC 按键输入）
2. 理解 OpenHarmony LiteOS-M 任务模型
3. 掌握 IOT GPIO / HI ADC 驱动调用
4. 掌握软件消抖（两次采样比较）的编程方法
5. 学会按键 → LED 的简单交互

## 实验环境

| 项目 | 说明 |
|:-----|:-----|
| 开发板 | HiSpark Pegasus (Hi3861) |
| LED | 板载 LED（GPIO9，低电平点亮） |
| 按键 | ADC 按键（ADC3 通道，配套开发板） |
| 编译 | Ubuntu + hb/scons |
| 烧录 | HiBurn 115200 bps |

## 代码说明

### 4.1 LED 闪烁

`key_demo.c` 同时实现了 LED 闪烁与 ADC 按键检测。LED 任务循环 300 ms 翻转 GPIO9 电平。

### 4.2 ADC 按键消抖

```c
static int IsKeyPressed(void) {
    unsigned int val1 = ReadAdcKey();
    usleep(10000);                 // 10ms 后再次采样
    unsigned int val2 = ReadAdcKey();
    if (val1 < 2000 && val2 < 2000) return 1;
    return 0;
}
```

### 4.3 "按键 → LED" 交互

KEY1（ADC < 400）点亮 LED；KEY2（ADC < 800）熄灭 LED。

## 文件清单

| 文件 | 说明 |
|:-----|:-----|
| `key_demo/key_demo.c` | LED + ADC 按键 + 消抖主程序 |
| `key_demo/BUILD.gn` | 编译配置 |

## 编译

```bash
# 将 key_demo/ 目录复制到 Hi3861 SDK
cp -r key_demo ~/hi3861_lab/applications/sample/wifi-iot/app/lab2_key_demo/

# SDK 根目录编译
cd ~/hi3861_lab
hb build -f
```

## 实验结果

```
[KEY] ADC Key Demo started
[KEY] Pressed! ADC=120, Voltage=52.73mV
[KEY] -> KEY1 (Short press)
[LED] Set to: ON
```

## 心得要点（详细见报告"心得总结"第一节）

- ADC 一次采样会被毛刺触发（实测 50 次里 5~8 次）
- 软件消抖"两次采样 + 10 ms 间隔"几乎消除误触发
- 50 ms 扫描一次，按键一次能采到 30~40 个连续低值

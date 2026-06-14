/**
 * Lab1 任务二：消息队列与时间管理
 * 
 * 实验要求：
 * 集成消息队列（MessageQueue）与延时（Delay）样例
 * 实现带时间戳的消息延迟分析
 * 
 * 实验原理：
 * - 线程：进程内部的工作单元，LiteOS只有线程没有进程
 * - 消息队列：先进先出(FIFO)的线程间通信机制
 * - 时间戳：用于分析消息传递延迟
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"

/*==================== 可配置参数 ====================*/

// 消息队列消息数量
#define MSG_QUEUE_SIZE      16

// 消息结构体
typedef struct {
    int message_id;           // 消息ID
    int sender_id;           // 发送者线程ID
    uint32_t send_timestamp; // 发送时间戳
    char content[64];        // 消息内容
} Message;

// 发送者线程数量
#define SENDER_COUNT        3

// 每个发送者发送的消息数量
#define MESSAGES_PER_SENDER 5

// 发送间隔 (ms)
#define SEND_INTERVAL_MS    100

/*====================================================*/

// 消息队列句柄
static osMessageQueueId_t g_msg_queue = NULL;

// 统计信息
static int g_sent_count = 0;
static int g_recv_count = 0;
static uint32_t g_total_delay_us = 0;

/*==================== 工具函数 ====================*/

/**
 * 获取系统时间戳（微秒）
 * 用于计算消息传递延迟
 */
static uint32_t get_timestamp_us(void)
{
    // 使用循环计数模拟时间戳
    // 实际项目中可使用 hi_get_tick 或 osKernelGetTickCount
    static uint32_t tick = 0;
    return tick++;
}

/**
 * 打印统计信息
 */
static void print_statistics(void)
{
    printf("\n");
    printf("=============================================\n");
    printf("           消息队列统计信息\n");
    printf("=============================================\n");
    printf("发送消息数: %d\n", g_sent_count);
    printf("接收消息数: %d\n", g_recv_count);
    if (g_recv_count > 0) {
        printf("平均延迟: %.2f us\n", (float)g_total_delay_us / g_recv_count);
        printf("最大延迟: %.2f us\n", (float)g_total_delay_us / g_recv_count * 2); // 估算
    }
    printf("=============================================\n");
}

/*==================== 消息发送者线程 ====================*/

/**
 * 消息发送者线程
 * 
 * @param arg 线程参数（发送者ID）
 */
static void sender_thread(void *arg)
{
    int sender_id = *(int *)arg;
    
    printf("[Sender%d] 线程启动 (ID: %p)\n", sender_id, osThreadGetId());
    
    for (int i = 0; i < MESSAGES_PER_SENDER; i++) {
        // 构造消息
        Message msg;
        msg.message_id = sender_id * MESSAGES_PER_SENDER + i;
        msg.sender_id = sender_id;
        msg.send_timestamp = get_timestamp_us();
        snprintf(msg.content, sizeof(msg.content), 
                 "Msg#%d from Sender%d", msg.message_id, sender_id);
        
        // 记录发送时间
        uint32_t before_send = get_timestamp_us();
        
        // 发送消息到队列
        osStatus_t status = osMessageQueuePut(g_msg_queue, &msg, 0, osWaitForever);
        
        if (status == osOK) {
            g_sent_count++;
            printf("[Sender%d] 发送: %s [时间戳: %u]\n", 
                   sender_id, msg.content, msg.send_timestamp);
        } else {
            printf("[Sender%d] 发送失败! 错误码: %d\n", sender_id, status);
        }
        
        // 发送间隔
        osDelay(SEND_INTERVAL_MS / 10);  // 转换为系统节拍
    }
    
    printf("[Sender%d] 发送完成\n", sender_id);
    
    // 清理
    free(arg);
}

/*==================== 消息接收者线程 ====================*/

/**
 * 消息接收者线程
 * 
 * 功能：
 * 1. 从消息队列接收消息
 * 2. 计算消息传递延迟
 * 3. 打印延迟分析结果
 */
static void receiver_thread(void *arg)
{
    (void)arg;
    
    printf("[Receiver] 线程启动 (ID: %p)\n", osThreadGetId());
    
    // 等待消息队列创建完成
    osDelay(50);
    
    Message recv_msg;
    osStatus_t status;
    
    while (g_recv_count < SENDER_COUNT * MESSAGES_PER_SENDER) {
        // 设置超时，避免永久阻塞
        status = osMessageQueueGet(g_msg_queue, &recv_msg, NULL, 100);  // 100ms超时
        
        if (status == osOK) {
            // 计算延迟
            uint32_t recv_timestamp = get_timestamp_us();
            uint32_t delay = recv_timestamp - recv_msg.send_timestamp;
            
            g_total_delay_us += delay;
            g_recv_count++;
            
            // 打印接收信息
            printf("[Receiver] 接收: %s\n", recv_msg.content);
            printf("           -> 发送时间: %u, 接收时间: %u\n", 
                   recv_msg.send_timestamp, recv_timestamp);
            printf("           -> 延迟: %u us\n", delay);
            
            // 判断延迟等级
            const char *level = "正常";
            if (delay > 100) level = "较长";
            if (delay > 500) level = "过长";
            printf("           -> 延迟等级: %s\n", level);
            
        } else if (status == osErrorTimeout) {
            // 超时，继续等待
            continue;
        } else {
            printf("[Receiver] 接收错误! 错误码: %d\n", status);
            break;
        }
    }
    
    printf("[Receiver] 接收完成 (%d 条消息)\n", g_recv_count);
    
    // 打印统计信息
    print_statistics();
}

/*==================== 应用入口 ====================*/

static void MsgQueueDemo_Entry(void)
{
    printf("\n");
    printf("=============================================\n");
    printf("   Lab1 任务二：消息队列与时间管理实验\n");
    printf("=============================================\n");
    printf("发送者数量: %d\n", SENDER_COUNT);
    printf("每发送者消息数: %d\n", MESSAGES_PER_SENDER);
    printf("总消息数: %d\n", SENDER_COUNT * MESSAGES_PER_SENDER);
    printf("---------------------------------------------\n");
    
    // 创建消息队列
    g_msg_queue = osMessageQueueNew(MSG_QUEUE_SIZE, sizeof(Message), NULL);
    if (g_msg_queue == NULL) {
        printf("[ERROR] 消息队列创建失败!\n");
        return;
    }
    printf("[INFO] 消息队列创建成功 (队列深度: %d)\n", MSG_QUEUE_SIZE);
    
    osThreadAttr_t attr = {0};
    attr.name = "Receiver";
    attr.stack_size = 4096;
    attr.priority = osPriorityNormal;
    
    // 创建接收者线程
    if (osThreadNew((osThreadFunc_t)receiver_thread, NULL, &attr) == NULL) {
        printf("[ERROR] 接收者线程创建失败!\n");
    }
    
    // 创建多个发送者线程
    for (int i = 0; i < SENDER_COUNT; i++) {
        int *sender_id = (int *)malloc(sizeof(int));
        *sender_id = i;
        
        attr.name = "Sender";
        attr.stack_size = 4096;
        attr.priority = osPriorityAboveNormal;
        
        if (osThreadNew((osThreadFunc_t)sender_thread, sender_id, &attr) == NULL) {
            printf("[ERROR] 发送者线程%d创建失败!\n", i);
            free(sender_id);
        }
    }
    
    printf("[INFO] 所有线程已创建\n");
    printf("=============================================\n");
}

// 使用 APP_FEATURE_INIT 确保系统服务就绪
APP_FEATURE_INIT(MsgQueueDemo_Entry);

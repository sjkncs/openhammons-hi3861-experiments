/**
 * Lab1 任务一：Hello程序改造
 * 
 * 实验要求：
 * 1. 将Hello程序的启动方式从SYS_RUN改为APP_FEATURE_INIT
 * 2. 在输出中显示学号和姓名
 * 
 * 启动方式说明：
 * - SYS_RUN：内核刚启动就跑，此时很多系统服务未就绪
 * - APP_FEATURE_INIT：应用层规范入口，确保系统环境准备就绪后再执行
 */

#include <stdio.h>
#include "ohos_init.h"
#include "ohos_types.h"

/**
 * HelloWorld - 主函数
 * 
 * 功能：打印学号、姓名和欢迎信息
 */
void HelloWorld(void)
{
    // ========== 请修改为您的实际信息 ==========
    const char *student_id = "2023315113";    // 替换为您的学号
    const char *student_name = "宋阳霆";         // 替换为您的姓名
    // ==========================================
    
    printf("\n");
    printf("=============================================\n");
    printf("       OpenHarmony Hi3861 实验报告\n");
    printf("=============================================\n");
    printf("学号: %s\n", student_id);
    printf("姓名: %s\n", student_name);
    printf("---------------------------------------------\n");
    printf("Welcome to OpenHarmony World!\n");
    printf("Hi3861 设备初始化成功\n");
    printf("=============================================\n");
    printf("\n");
}

// 使用 APP_FEATURE_INIT 启动方式
// 相比 SYS_RUN，APP_FEATURE_INIT 确保系统服务已就绪
APP_FEATURE_INIT(HelloWorld);

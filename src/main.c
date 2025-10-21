/**
 * @file main.c
 * @brief 18轴舵机控制系统 - 主程序
 * @date 2025-10-21
 * @version 1.0.0
 * 
 * 系统架构：
 * - Core 0: 通信任务、运动控制任务
 * - Core 1: PWM任务（高优先级）
 * - 基于FreeRTOS双核调度
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"

// FreeRTOS (使用Pico SDK内置)
#include <FreeRTOS.h>
#include <task.h>

// 配置
#include "config/config.h"
#include "config/pinout.h"

// 模块
#include "pwm/pwm_driver.h"
#include "servo/servo_control.h"
#include "communication/usb_handler.h"
#include "storage/param_manager.h"
#include "utils/error_handler.h"

// 任务
#include "tasks/task_communication.h"
#include "tasks/task_motion.h"
#include "tasks/task_pwm.h"

// 系统初始化标志
static bool g_system_initialized = false;

/**
 * @brief 系统初始化
 * @return true 成功, false 失败
 */
bool system_init(void) {
    // 1. 初始化错误处理
    error_handler_init();
    
    // 2. 初始化PWM系统
    if (!pwm_init_all()) {
        error_set(ERROR_SYSTEM_INIT);
        return false;
    }
    
    // 3. 初始化舵机控制
    if (!servo_control_init()) {
        error_set(ERROR_SYSTEM_INIT);
        return false;
    }
    
    // 4. 初始化参数管理（加载Flash参数）
    if (!param_manager_init()) {
        error_set(ERROR_SYSTEM_INIT);
        // 参数加载失败不是致命错误，使用默认值继续
    }
    
    // 5. 设置所有舵机到中位
    float center_angles[SERVO_COUNT];
    for (int i = 0; i < SERVO_COUNT; i++) {
        center_angles[i] = 90.0f;  // 中位角度
    }
    servo_set_all_angles(center_angles);
    
    // 6. 设置系统状态
    system_set_state(SYSTEM_STATE_IDLE);
    
    g_system_initialized = true;
    return true;
}

/**
 * @brief Core 1 入口函数
 */
void core1_entry(void) {
    // Core 1 运行FreeRTOS调度器
    // PWM任务会自动绑定到Core 1
    vTaskStartScheduler();
    
    // 不应该到达这里
    while (1) {
        tight_loop_contents();
    }
}

/**
 * @brief 主函数
 */
int main(void) {
    // 初始化标准库
    stdio_init_all();
    
    // 延迟等待USB稳定
    sleep_ms(1000);
    
    printf("\n\n");
    printf("========================================\n");
    printf("  18-Channel Servo Controller\n");
    printf("  Version: %d.%d.%d\n", SYSTEM_VERSION_MAJOR, SYSTEM_VERSION_MINOR, SYSTEM_VERSION_PATCH);
    printf("  Platform: RP2350 @ 150MHz\n");
    printf("  FreeRTOS Dual-Core\n");
    printf("========================================\n\n");
    
    // 系统初始化
    printf("Initializing system...\n");
    if (!system_init()) {
        printf("ERROR: System initialization failed!\n");
        error_set(ERROR_SYSTEM_INIT);
        
        // 进入错误状态，LED快速闪烁
        while (1) {
            gpio_put(PIN_LED_BUILTIN, 1);
            sleep_ms(100);
            gpio_put(PIN_LED_BUILTIN, 0);
            sleep_ms(100);
        }
    }
    printf("System initialized successfully.\n\n");
    
    // 创建任务（Core 0）
    printf("Creating tasks...\n");
    
    if (task_communication_create() != pdPASS) {
        printf("ERROR: Failed to create communication task!\n");
        error_set(ERROR_SYSTEM_TASK);
    }
    
    if (task_motion_create() != pdPASS) {
        printf("ERROR: Failed to create motion task!\n");
        error_set(ERROR_SYSTEM_TASK);
    }
    
    if (task_pwm_create() != pdPASS) {
        printf("ERROR: Failed to create PWM task!\n");
        error_set(ERROR_SYSTEM_TASK);
    }
    
    printf("Tasks created successfully.\n");
    printf("Starting FreeRTOS scheduler...\n\n");
    
    // 启动Core 1
    multicore_launch_core1(core1_entry);
    
    // 延迟一下，确保Core 1启动
    sleep_ms(100);
    
    // 在Core 0启动调度器
    vTaskStartScheduler();
    
    // 不应该到达这里
    printf("ERROR: Scheduler returned!\n");
    while (1) {
        tight_loop_contents();
    }
    
    return 0;
}

/**
 * @brief FreeRTOS malloc失败钩子
 */
void vApplicationMallocFailedHook(void) {
    error_set(ERROR_SYSTEM_MEMORY);
    printf("ERROR: FreeRTOS malloc failed!\n");
    
    // 进入死循环
    while (1) {
        gpio_put(PIN_LED_BUILTIN, 1);
        sleep_ms(50);
        gpio_put(PIN_LED_BUILTIN, 0);
        sleep_ms(50);
    }
}

/**
 * @brief FreeRTOS 栈溢出钩子
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    (void)xTask;
    error_set(ERROR_SYSTEM_MEMORY);
    printf("ERROR: Stack overflow in task: %s\n", pcTaskName);
    
    // 进入死循环
    while (1) {
        gpio_put(PIN_LED_BUILTIN, 1);
        sleep_ms(50);
        gpio_put(PIN_LED_BUILTIN, 0);
        sleep_ms(50);
    }
}

/**
 * @brief FreeRTOS 空闲钩子
 */
void vApplicationIdleHook(void) {
    // 空闲任务，可以在这里做低优先级工作
    // 例如：看门狗喂狗、低功耗模式等
}

/**
 * @brief FreeRTOS Tick钩子
 */
void vApplicationTickHook(void) {
    // 每个Tick调用一次
    // 可以用于定时任务
}
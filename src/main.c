/**
 * @file main.c
 * @brief 18轴舵机控制系统 - QP/C主程序
 * @date 2025-10-23
 * @version 2.0.0-QPC
 * 
 * 系统架构：
 * - QP/C事件驱动框架
 * - QV协作式内核
 * - 3个主动对象：Communication, Motion, System
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "qpc.h"

// 配置
#include "config/config.h"
#include "config/pinout.h"

// 事件
#include "events/events.h"
#include "events/event_types.h"

// 主动对象
#include "ao/ao_communication.h"
#include "ao/ao_motion.h"
#include "ao/ao_system.h"

// 底层模块
#include "pwm/pwm_driver.h"
#include "servo/servo_control.h"
#include "servo/servo_manager.h"
#include "storage/param_manager.h"
#include "utils/error_handler.h"

// ==================== 事件池定义 ====================

// 小事件池（用于简单事件）
static QF_MPOOL_EL(QEvt) small_pool_sto[20];

// 中等事件池（用于单轴移动事件）
static QF_MPOOL_EL(MoveSingleEvt) medium_pool_sto[10];

// 大事件池（用于运动控制事件 - MotionStartEvt是最大的）
static QF_MPOOL_EL(MotionStartEvt) large_pool_sto[5];

// ==================== 系统初始化 ====================

/**
 * @brief 硬件和底层模块初始化
 */
static bool hardware_init(void) {
    printf("\n[INIT] Hardware initialization...\n");
    
    // 1. 初始化错误处理
    printf("[INIT] Error handler...\n");
    error_handler_init();
    
    // 2. 初始化PWM系统
    printf("[INIT] PWM system...\n");
    if (!pwm_init_all()) {
        printf("[ERROR] PWM init failed!\n");
        return false;
    }
    
    // 3. 初始化舵机控制
    printf("[INIT] Servo control...\n");
    if (!servo_control_init()) {
        printf("[ERROR] Servo init failed!\n");
        return false;
    }
    
    // 4. 初始化舵机管理器（支持180度+360度）
    printf("[INIT] Servo manager...\n");
    if (!servo_manager_init()) {
        printf("[ERROR] Servo manager init failed!\n");
        return false;
    }
    
    // 5. 初始化参数管理（加载Flash参数）
    printf("[INIT] Param manager...\n");
    if (!param_manager_init()) {
        printf("[WARNING] Param load failed, using defaults.\n");
    }
    
    // 6. 设置所有舵机到中位
    printf("[INIT] Setting servos to center...\n");
    float center_angles[SERVO_COUNT];
    for (int i = 0; i < SERVO_COUNT; i++) {
        center_angles[i] = 90.0f;
    }
    servo_set_all_angles(center_angles);
    
    printf("[INIT] Hardware init complete!\n\n");
    return true;
}

// ==================== 主函数 ====================

int main(void) {
    // ========== 早期初始化 ==========
    // LED作为早期调试指示
    gpio_init(PIN_LED_BUILTIN);
    gpio_set_dir(PIN_LED_BUILTIN, GPIO_OUT);
    gpio_put(PIN_LED_BUILTIN, 1);  // 点亮
    
    // 初始化USB CDC
    stdio_init_all();
    sleep_ms(2000);  // 等待USB枚举
    
    // LED闪烁2次表示USB就绪
    for (int i = 0; i < 2; i++) {
        gpio_put(PIN_LED_BUILTIN, 0);
        sleep_ms(100);
        gpio_put(PIN_LED_BUILTIN, 1);
        sleep_ms(100);
    }
    
    // ========== 打印启动信息 ==========
    printf("\n\n");
    printf("========================================\n");
    printf("  18-Channel Servo Controller\n");
    printf("  Version: %d.%d.%d-QPC\n", SYSTEM_VERSION_MAJOR, SYSTEM_VERSION_MINOR, SYSTEM_VERSION_PATCH);
    printf("  Platform: RP2350 @ 150MHz\n");
    printf("  Framework: QP/C %s (QV Kernel)\n", QP_VERSION_STR);
    printf("========================================\n\n");
    
    // ========== 硬件初始化 ==========
    if (!hardware_init()) {
        printf("[CRITICAL] Hardware initialization failed!\n");
        
        // LED快速闪烁表示错误
        while (1) {
            gpio_put(PIN_LED_BUILTIN, 1);
            sleep_ms(100);
            gpio_put(PIN_LED_BUILTIN, 0);
            sleep_ms(100);
        }
    }
    
    // ========== QP/C初始化 ==========
    printf("[QP] Initializing QP/C framework...\n");
    QF_init();  // 初始化QP框架
    
    // 初始化事件池
    printf("[QP] Initializing event pools...\n");
    QF_poolInit(small_pool_sto, sizeof(small_pool_sto), sizeof(QEvt));
    QF_poolInit(medium_pool_sto, sizeof(medium_pool_sto), sizeof(MoveSingleEvt));
    QF_poolInit(large_pool_sto, sizeof(large_pool_sto), sizeof(MotionStartEvt));
    
    // 打印事件池信息用于调试
    printf("[QP] Event pool sizes: QEvt=%u, MoveSingleEvt=%u, MotionStartEvt=%u\n",
           sizeof(QEvt), sizeof(MoveSingleEvt), sizeof(MotionStartEvt));
    
    // ========== 创建主动对象 ==========
    printf("[QP] Constructing Active Objects...\n");
    AO_Communication_ctor();
    AO_Motion_ctor();
    AO_System_ctor();
    
    // ========== 启动主动对象 ==========
    printf("[QP] Starting Active Objects...\n");
    
    // Communication AO事件队列
    static QEvt const *comm_queue_sto[AO_QUEUE_SIZE_COMM];
    QACTIVE_START(AO_Communication,
                  AO_PRIORITY_COMM,         // 优先级
                  comm_queue_sto,           // 事件队列存储
                  Q_DIM(comm_queue_sto),    // 队列长度
                  (void *)0,                // 栈（QV不需要）
                  0U,                       // 栈大小
                  (void *)0);               // 初始化参数
    
    // Motion AO事件队列
    static QEvt const *motion_queue_sto[AO_QUEUE_SIZE_MOTION];
    QACTIVE_START(AO_Motion,
                  AO_PRIORITY_MOTION,
                  motion_queue_sto,
                  Q_DIM(motion_queue_sto),
                  (void *)0,
                  0U,
                  (void *)0);
    
    // System AO事件队列
    static QEvt const *system_queue_sto[AO_QUEUE_SIZE_SYSTEM];
    QACTIVE_START(AO_System,
                  AO_PRIORITY_SYSTEM,
                  system_queue_sto,
                  Q_DIM(system_queue_sto),
                  (void *)0,
                  0U,
                  (void *)0);
    
    printf("[QP] All Active Objects started successfully!\n");
    printf("[QP] System ready. Starting event loop...\n");
    printf("========================================\n\n");
    
    // LED闪烁3次表示即将进入QP事件循环
    for (int i = 0; i < 3; i++) {
        gpio_put(PIN_LED_BUILTIN, 0);
        sleep_ms(200);
        gpio_put(PIN_LED_BUILTIN, 1);
        sleep_ms(200);
    }
    
    sleep_ms(100);
    
    // ========== 运行QP事件循环 ==========
    printf("[QP] >>> Entering QF_run() event loop <<<\n");
    printf("If LED starts blinking slowly, QP is running!\n\n");
    sleep_ms(100);
    
    return QF_run();  // 运行QP事件循环（永不返回）
}

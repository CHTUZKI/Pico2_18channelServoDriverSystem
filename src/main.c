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
#include "utils/usb_bridge.h"  // USB桥接器（Core1独占串口）

// GPIO监控器
#include "monitor/gpio_monitor.h"

// 自动测试
#include "test/auto_test.h"

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
    LOG_INFO("[INIT] Starting hardware...\n");
    sleep_ms(50);
    
    // 1. 初始化错误处理
    error_handler_init();
    
    // 2. 初始化PWM系统
    if (!pwm_init_all()) {
        LOG_ERROR("[ERROR] PWM failed!\n");
        return false;
    }
    
    // 3. 初始化舵机控制
    if (!servo_control_init()) {
        LOG_ERROR("[ERROR] Servo failed!\n");
        return false;
    }
    
    // 4. 初始化舵机管理器（支持180度+360度）
    if (!servo_manager_init()) {
        LOG_ERROR("[ERROR] Manager failed!\n");
        return false;
    }
    
    // 5. 初始化参数管理（加载Flash参数）
    param_manager_init();
    
    // 6. 上电位置设置（总是尝试从Flash恢复，安全优先）
    if (param_manager_load_positions()) {
        LOG_INFO("[INIT] Restored positions from Flash\n");
    } else {
        // Flash中无有效位置，设置到中位
        LOG_INFO("[INIT] No saved positions, setting to center\n");
        float center_angles[SERVO_COUNT];
        for (int i = 0; i < SERVO_COUNT; i++) {
            center_angles[i] = 90.0f;
        }
        servo_set_all_angles(center_angles);
    }
    
    LOG_INFO("[INIT] Hardware OK!\n");
    sleep_ms(50);
    return true;
}

// ==================== 主函数 ====================

int main(void) {
    // ========== 早期初始化 ==========
    // LED作为早期调试指示
    gpio_init(PIN_LED_BUILTIN);
    gpio_set_dir(PIN_LED_BUILTIN, GPIO_OUT);
    gpio_put(PIN_LED_BUILTIN, 1);  // 点亮
    
    // ========== 初始化USB（必须在Core 0早期完成）==========
    // USB枚举必须在启动后2秒内完成，否则Windows会放弃
    stdio_init_all();
    sleep_ms(2000);  // 等待USB枚举完成
    
    // LED闪烁2次表示USB就绪
    for (int i = 0; i < 2; i++) {
        gpio_put(PIN_LED_BUILTIN, 0);
        sleep_ms(100);
        gpio_put(PIN_LED_BUILTIN, 1);
        sleep_ms(100);
    }
    
    // ========== 初始化USB桥接器（Core1接管USB操作）==========
    // ⚠️ 不要在这里printf！Core 1还没启动
    // LED闪烁3次表示即将启动Core 1
    for (int i = 0; i < 3; i++) {
        gpio_put(PIN_LED_BUILTIN, 0);
        sleep_ms(150);
        gpio_put(PIN_LED_BUILTIN, 1);
        sleep_ms(150);
    }
    
    usb_bridge_init();
    usb_bridge_start_core1();  // 启动Core1
    sleep_ms(1500);  // 等待Core 1完全启动
    
    // ========== 从这里开始，所有日志通过Core1打印 ==========
    // 注意：每次LOG_INFO后稍微延迟，给Core 1时间处理
    
    LOG_INFO("\n========== System Starting ==========\n");
    sleep_ms(50);
    
    LOG_INFO("18-Channel Servo Controller v%d.%d.%d\n", 
             SYSTEM_VERSION_MAJOR, SYSTEM_VERSION_MINOR, SYSTEM_VERSION_PATCH);
    sleep_ms(50);
    
    LOG_INFO("Build: %s %s\n", __DATE__, __TIME__);
    sleep_ms(50);
    
    LOG_INFO("Platform: RP2350 @ 150MHz\n");
    sleep_ms(50);
    
    LOG_INFO("Framework: QP/C (QV Kernel)\n");
    sleep_ms(50);
    
    LOG_INFO("=====================================\n\n");
    sleep_ms(50);
    
    // ========== 硬件初始化 ==========
    if (!hardware_init()) {
        LOG_CRITICAL("[CRITICAL] Hardware initialization failed!\n");
        
        // LED快速闪烁表示错误
        while (1) {
            gpio_put(PIN_LED_BUILTIN, 1);
            sleep_ms(100);
            gpio_put(PIN_LED_BUILTIN, 0);
            sleep_ms(100);
        }
    }
    
    // ========== QP/C初始化 ==========
    LOG_INFO("[QP] Initializing framework...\n");
    sleep_ms(50);
    QF_init();  // 初始化QP框架
    
    // 初始化事件池
    LOG_INFO("[QP] Event pools...\n");
    sleep_ms(50);
    QF_poolInit(small_pool_sto, sizeof(small_pool_sto), sizeof(QEvt));
    QF_poolInit(medium_pool_sto, sizeof(medium_pool_sto), sizeof(MoveSingleEvt));
    QF_poolInit(large_pool_sto, sizeof(large_pool_sto), sizeof(MotionStartEvt));
    
    // ========== 创建主动对象 ==========
    LOG_INFO("[QP] Creating Active Objects...\n");
    sleep_ms(50);
    AO_Communication_ctor();
    AO_Motion_ctor();
    AO_System_ctor();
    
    // ========== 启动主动对象 ==========
    LOG_INFO("[QP] Starting Active Objects...\n");
    sleep_ms(50);
    
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
    
    LOG_INFO("[QP] All AOs started!\n");
    sleep_ms(50);
    
    // ========== 启动自动测试 ==========
#if ENABLE_AUTO_TEST
    LOG_INFO("[TEST] Auto test enabled\n");
    sleep_ms(50);
    auto_test_start();
#endif
    
    LOG_INFO("[QP] System ready!\n");
    sleep_ms(50);
    
    LOG_INFO("Entering event loop...\n\n");
    sleep_ms(100);
    
    // 等待所有日志打印完成
    usb_bridge_flush(500);
    sleep_ms(200);
    
    return QF_run();  // 运行QP事件循环（永不返回）
}

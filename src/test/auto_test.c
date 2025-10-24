/**
 * @file auto_test.c
 * @brief 自动测试模式实现（基于QP/C事件驱动）
 * @author AI Assistant
 * @date 2025-10-23
 * 
 * 实现方式：监听MOTION_COMPLETE_SIG信号来触发下一步，
 *          避免使用硬件定时器，不会与QP/C冲突
 */

#include "test/auto_test.h"
#include "events/events.h"
#include "events/event_types.h"
#include "ao/ao_motion.h"
#include "ao/ao_system.h"
#include "config/config.h"
#include "servo/servo_control.h"
#include "utils/usb_bridge.h"
#include "qpc.h"
#include "pico/stdlib.h"
#include <stdio.h>

// 测试配置
#define TEST_CYCLES         100      // 测试循环次数
#define TEST_DURATION_MS    5000    // 每次运动持续时间（毫秒）

// 测试状态
typedef enum {
    TEST_STATE_DISABLED,    // 未启动
    TEST_STATE_INIT,        // 初始化（使能舵机）
    TEST_STATE_TO_0,        // 移动到0度
    TEST_STATE_TO_180,      // 移动到180度
    TEST_STATE_COMPLETE     // 测试完成
} test_state_t;

// 测试上下文
static struct {
    bool running;
    test_state_t state;
    uint32_t cycle_count;
    uint32_t total_cycles;
} test_ctx = {
    .running = false,
    .state = TEST_STATE_DISABLED,
    .cycle_count = 0,
    .total_cycles = TEST_CYCLES
};

// 前向声明
static void test_send_motion_command(float target_angle, uint16_t duration_ms);

/**
 * @brief 发送运动命令到Motion AO
 */
static void test_send_motion_command(float target_angle, uint16_t duration_ms) {
    // 分配MotionStartEvt事件
    MotionStartEvt *evt = Q_NEW(MotionStartEvt, MOTION_START_SIG);
    if (evt == NULL) {
        LOG_INFO("[AUTO-TEST] ERROR: Event allocation failed!\n");
        return;
    }
    
    // 填充事件数据
    evt->axis_count = SERVO_COUNT;
    evt->duration_ms = duration_ms;
    
    // 所有舵机设置相同角度和ID
    for (int i = 0; i < SERVO_COUNT; i++) {
        evt->axis_ids[i] = i;
        evt->target_positions[i] = target_angle;
    }
    
    // 发送事件到Motion AO
    QACTIVE_POST(AO_Motion, (QEvt *)evt, 0);
}

/**
 * @brief 处理运动完成事件（由外部调用）
 */
void auto_test_on_motion_complete(void) {
    if (!test_ctx.running) {
        return;  // 测试未运行，忽略
    }
    
    switch (test_ctx.state) {
        case TEST_STATE_TO_0:
            // 0度完成，移动到180度
            test_ctx.state = TEST_STATE_TO_180;
            LOG_INFO("[AUTO-TEST] Cycle %lu/%lu - Moving to 180deg\n", 
                   test_ctx.cycle_count, test_ctx.total_cycles);
            test_send_motion_command(180.0f, TEST_DURATION_MS);
            break;
            
        case TEST_STATE_TO_180:
            // 180度完成，检查是否继续循环
            if (test_ctx.cycle_count < test_ctx.total_cycles) {
                // 开始下一次循环
                test_ctx.cycle_count++;
                test_ctx.state = TEST_STATE_TO_0;
                LOG_INFO("[AUTO-TEST] Cycle %lu/%lu - Moving to 0deg\n", 
                       test_ctx.cycle_count, test_ctx.total_cycles);
                test_send_motion_command(0.0f, TEST_DURATION_MS);
            } else {
                // 测试完成，回到90度安全位置
                test_ctx.state = TEST_STATE_COMPLETE;
                LOG_INFO("[AUTO-TEST] Returning to 90deg (safe position)\n");
                test_send_motion_command(90.0f, TEST_DURATION_MS);
            }
            break;
            
        case TEST_STATE_COMPLETE:
            // 回到90度完成，测试结束
            test_ctx.running = false;
            LOG_INFO("\n========================================\n");
            LOG_INFO("[AUTO-TEST] Test completed!\n");
            LOG_INFO("[AUTO-TEST] Total cycles: %lu\n", test_ctx.total_cycles);
            LOG_INFO("========================================\n\n");
            break;
            
        default:
            break;
    }
}

/**
 * @brief 启动自动测试
 */
void auto_test_start(void) {
    LOG_INFO("\n========================================\n");
    LOG_INFO("[AUTO-TEST] Cycle test mode\n");
    LOG_INFO("[AUTO-TEST] Total cycles: %lu\n", TEST_CYCLES);
    LOG_INFO("[AUTO-TEST] Sequence: 0deg <-> 180deg\n");
    LOG_INFO("[AUTO-TEST] Duration: %ums per move\n", TEST_DURATION_MS);
    LOG_INFO("========================================\n\n");
    
    // 使能所有舵机
    LOG_INFO("[AUTO-TEST] Enabling all servos...\n");
    for (int i = 0; i < SERVO_COUNT; i++) {
        servo_enable(i, true);
    }
    LOG_INFO("[AUTO-TEST] All servos enabled\n\n");
    
    // 初始化测试上下文
    test_ctx.running = true;
    test_ctx.state = TEST_STATE_TO_0;
    test_ctx.cycle_count = 1;
    
    // 开始第一次运动：移动到0度
    LOG_INFO("[AUTO-TEST] Cycle 1/%lu - Moving to 0deg\n", TEST_CYCLES);
    test_send_motion_command(0.0f, TEST_DURATION_MS);
}

/**
 * @brief 停止自动测试
 */
void auto_test_stop(void) {
    if (!test_ctx.running) {
        return;
    }
    
    test_ctx.running = false;
    test_ctx.state = TEST_STATE_DISABLED;
    
    LOG_INFO("\n[AUTO-TEST] Test stopped\n");
}

/**
 * @brief 检查测试是否正在运行
 */
bool auto_test_is_running(void) {
    return test_ctx.running;
}

/**
 * @brief 获取当前测试循环次数
 */
uint32_t auto_test_get_cycle_count(void) {
    return test_ctx.cycle_count;
}

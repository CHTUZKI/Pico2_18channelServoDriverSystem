/**
 * @file auto_test.c
 * @brief 自动测试模式实现
 * @author AI Assistant
 * @date 2025-10-23
 */

#include "test/auto_test.h"
#include "events/events.h"
#include "events/event_types.h"
#include "ao/ao_motion.h"
#include "config/config.h"
#include "qpc.h"
#include "pico/stdlib.h"
#include <stdio.h>

// 测试配置
#define TEST_CYCLES         20      // 测试循环次数
#define TEST_DURATION_MS    2000    // 每次运动持续时间（毫秒）
#define TEST_DELAY_MS       500     // 每次运动之间的延迟（毫秒）

// 测试状态
typedef enum {
    TEST_STATE_IDLE,        // 空闲
    TEST_STATE_TO_0,        // 移动到0度
    TEST_STATE_TO_180,      // 移动到180度
    TEST_STATE_TO_90,       // 回到90度
    TEST_STATE_DELAY,       // 延迟等待
    TEST_STATE_COMPLETE     // 测试完成
} test_state_t;

// 测试上下文
static struct {
    bool running;
    test_state_t state;
    uint32_t cycle_count;
    uint32_t total_cycles;
    alarm_id_t timer_id;
} test_ctx = {
    .running = false,
    .state = TEST_STATE_IDLE,
    .cycle_count = 0,
    .total_cycles = TEST_CYCLES,
    .timer_id = 0
};

// 前向声明
static void test_send_motion_command(float target_angle, uint16_t duration_ms);
static int64_t test_timer_callback(alarm_id_t id, void *user_data);
static void test_next_step(void);

/**
 * @brief 发送运动命令到Motion AO
 */
static void test_send_motion_command(float target_angle, uint16_t duration_ms) {
    printf("[AUTO-TEST] Motion cmd: angle=%.1f deg, duration=%ums\n", 
           target_angle, duration_ms);
    
    // 分配MotionStartEvt事件
    MotionStartEvt *evt = Q_NEW(MotionStartEvt, MOTION_START_SIG);
    if (evt == NULL) {
        printf("[AUTO-TEST] ERROR: Event allocation failed!\n");
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
 * @brief 定时器回调函数
 */
static int64_t test_timer_callback(alarm_id_t id, void *user_data) {
    (void)id;
    (void)user_data;
    
    // 继续下一步
    test_next_step();
    
    return 0;  // 不重复
}

/**
 * @brief 执行下一步测试
 */
static void test_next_step(void) {
    if (!test_ctx.running) {
        return;
    }
    
    switch (test_ctx.state) {
        case TEST_STATE_IDLE:
            // 开始新的循环
            test_ctx.cycle_count++;
            
            if (test_ctx.cycle_count > test_ctx.total_cycles) {
                // 测试完成
                test_ctx.state = TEST_STATE_COMPLETE;
                test_ctx.running = false;
                printf("\n========================================\n");
                printf("[AUTO-TEST] Test complete! Total cycles: %lu\n", 
                       test_ctx.cycle_count - 1);
                printf("========================================\n\n");
                return;
            }
            
            printf("\n--- Cycle %lu/%lu ---\n", 
                   test_ctx.cycle_count, test_ctx.total_cycles);
            
            // 移动到0度
            test_ctx.state = TEST_STATE_TO_0;
            test_send_motion_command(0.0f, TEST_DURATION_MS);
            
            // 设置定时器等待运动完成
            test_ctx.timer_id = add_alarm_in_ms(
                TEST_DURATION_MS + TEST_DELAY_MS,
                test_timer_callback,
                NULL,
                false
            );
            break;
            
        case TEST_STATE_TO_0:
            // 移动到180度
            test_ctx.state = TEST_STATE_TO_180;
            test_send_motion_command(180.0f, TEST_DURATION_MS);
            
            // 设置定时器等待运动完成
            test_ctx.timer_id = add_alarm_in_ms(
                TEST_DURATION_MS + TEST_DELAY_MS,
                test_timer_callback,
                NULL,
                false
            );
            break;
            
        case TEST_STATE_TO_180:
            // 回到90度
            test_ctx.state = TEST_STATE_TO_90;
            test_send_motion_command(90.0f, TEST_DURATION_MS);
            
            // 设置定时器等待运动完成
            test_ctx.timer_id = add_alarm_in_ms(
                TEST_DURATION_MS + TEST_DELAY_MS,
                test_timer_callback,
                NULL,
                false
            );
            break;
            
        case TEST_STATE_TO_90:
            // 当前循环完成，回到IDLE开始下一次循环
            test_ctx.state = TEST_STATE_IDLE;
            
            // 短暂延迟后开始下一次循环
            test_ctx.timer_id = add_alarm_in_ms(
                1000,  // 1秒延迟
                test_timer_callback,
                NULL,
                false
            );
            break;
            
        default:
            break;
    }
}

/**
 * @brief 启动自动测试
 */
void auto_test_start(void) {
    if (test_ctx.running) {
        printf("[AUTO-TEST] Test already running\n");
        return;
    }
    
    printf("\n========================================\n");
    printf("[AUTO-TEST] Auto test started\n");
    printf("[AUTO-TEST] Total cycles: %lu\n", test_ctx.total_cycles);
    printf("[AUTO-TEST] Motion duration: %ums\n", TEST_DURATION_MS);
    printf("[AUTO-TEST] Sequence: 90deg -> 0deg -> 180deg -> 90deg\n");
    printf("========================================\n\n");
    
    // 初始化测试上下文
    test_ctx.running = true;
    test_ctx.state = TEST_STATE_IDLE;
    test_ctx.cycle_count = 0;
    
    // 延迟3秒后开始测试（给系统启动时间）
    test_ctx.timer_id = add_alarm_in_ms(
        3000,
        test_timer_callback,
        NULL,
        false
    );
    
    printf("[AUTO-TEST] Starting in 3 seconds...\n\n");
}

/**
 * @brief 停止自动测试
 */
void auto_test_stop(void) {
    if (!test_ctx.running) {
        return;
    }
    
    // 取消定时器
    if (test_ctx.timer_id != 0) {
        cancel_alarm(test_ctx.timer_id);
        test_ctx.timer_id = 0;
    }
    
    test_ctx.running = false;
    test_ctx.state = TEST_STATE_IDLE;
    
    printf("\n[AUTO-TEST] Test stopped\n");
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

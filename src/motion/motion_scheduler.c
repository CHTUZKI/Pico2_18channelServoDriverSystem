/**
 * @file motion_scheduler.c
 * @brief 运动调度器实现
 * @date 2025-10-25
 */

#include "motion/motion_scheduler.h"
#include "utils/usb_bridge.h"
#include "config/config.h"
#include "pico/stdlib.h"

// ==================== 调试宏 ====================
#if DEBUG_SCHEDULER
    #define SCHED_DEBUG(...) usb_bridge_printf(__VA_ARGS__)
#else
    #define SCHED_DEBUG(...) ((void)0)
#endif

// ==================== 全局数据 ====================

// 全局运动缓冲区
static motion_buffer_t g_motion_buffer;

// 运动执行回调函数
static motion_execute_callback_t g_execute_callback = NULL;

// ==================== 初始化函数 ====================

void motion_scheduler_init(void) {
    motion_buffer_init(&g_motion_buffer);
    g_execute_callback = NULL;
    SCHED_DEBUG("[SCHEDULER] Initialized\n");
}

void motion_scheduler_set_callback(motion_execute_callback_t callback) {
    g_execute_callback = callback;
    SCHED_DEBUG("[SCHEDULER] Callback registered\n");
}

// ==================== 缓冲区管理 ====================

motion_buffer_t* motion_scheduler_get_buffer(void) {
    return &g_motion_buffer;
}

bool motion_scheduler_add_block(const motion_block_t *block) {
    if (block == NULL) {
        return false;
    }
    return motion_buffer_add(&g_motion_buffer, block);
}

bool motion_scheduler_start(void) {
    return motion_buffer_start(&g_motion_buffer);
}

void motion_scheduler_stop(void) {
    motion_buffer_stop(&g_motion_buffer);
}

void motion_scheduler_pause(void) {
    motion_buffer_pause(&g_motion_buffer);
}

void motion_scheduler_resume(void) {
    motion_buffer_resume(&g_motion_buffer);
}

void motion_scheduler_clear(void) {
    motion_buffer_clear(&g_motion_buffer);
}

bool motion_scheduler_is_running(void) {
    return motion_buffer_is_running(&g_motion_buffer);
}

bool motion_scheduler_is_paused(void) {
    return motion_buffer_is_paused(&g_motion_buffer);
}

uint8_t motion_scheduler_get_count(void) {
    return g_motion_buffer.count;
}

uint8_t motion_scheduler_get_available(void) {
    return motion_buffer_available(&g_motion_buffer);
}

// ==================== 调度器核心 ====================

void motion_scheduler_update(void) {
    // 检查缓冲区是否正在运行
    if (!motion_buffer_is_running(&g_motion_buffer)) {
        return;
    }
    
    // 检查是否暂停
    if (g_motion_buffer.paused) {
        return;
    }
    
    // 检查缓冲区是否为空
    if (motion_buffer_is_empty(&g_motion_buffer)) {
        // 缓冲区已空，停止执行
        motion_buffer_stop(&g_motion_buffer);
        SCHED_DEBUG("[SCHEDULER] All blocks executed, stopped\n");
        return;
    }
    
    // 获取当前时间（相对于开始时间）
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    uint32_t elapsed_ms = current_time - g_motion_buffer.start_time;
    
    // 获取下一个待执行的指令
    motion_block_t *block = motion_buffer_peek(&g_motion_buffer);
    if (block == NULL) {
        return;
    }
    
    // 检查是否到达执行时间
    if (elapsed_ms >= block->timestamp_ms) {
        SCHED_DEBUG("[SCHEDULER] >>> t=%dms S%d->%ddeg v=%d a=%d\n",
                   (int)block->timestamp_ms, block->servo_id,
                   (int)block->target_angle, (int)block->velocity,
                   (int)block->acceleration);
        
        // 通过回调执行运动指令
        if (g_execute_callback != NULL) {
            float decel = (block->deceleration > 0.0f) ? block->deceleration : block->acceleration;
            
            g_execute_callback(
                block->servo_id,
                block->target_angle,
                block->velocity,
                block->acceleration,
                decel
            );
        }
        
        // 从缓冲区移除已执行的指令
        motion_buffer_remove(&g_motion_buffer);
        
        SCHED_DEBUG("[SCHEDULER] Remaining: %d blocks\n", g_motion_buffer.count);
    }
}


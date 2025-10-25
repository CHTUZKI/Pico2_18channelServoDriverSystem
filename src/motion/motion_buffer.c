/**
 * @file motion_buffer.c
 * @brief 运动指令缓冲区管理实现
 * @date 2025-10-26
 */

#include "motion/motion_buffer.h"
#include "pico/stdlib.h"
#include "config/config.h"
#include "utils/usb_bridge.h"
#include <string.h>
#include <stdio.h>

// ==================== 调试宏 ====================
#if DEBUG_BUFFER
    #define BUFFER_DEBUG(...) usb_bridge_printf(__VA_ARGS__)
#else
    #define BUFFER_DEBUG(...) ((void)0)
#endif

// ==================== 缓冲区管理实现 ====================

void motion_buffer_init(motion_buffer_t *buffer) {
    if (buffer == NULL) {
        return;
    }
    
    memset(buffer, 0, sizeof(motion_buffer_t));
    buffer->head = 0;
    buffer->tail = 0;
    buffer->count = 0;
    buffer->running = false;
    buffer->paused = false;
    buffer->start_time = 0;
}

bool motion_buffer_add(motion_buffer_t *buffer, const motion_block_t *block) {
    if (buffer == NULL || block == NULL) {
        return false;
    }
    
    // 检查缓冲区是否已满
    if (buffer->count >= MOTION_BUFFER_SIZE) {
        BUFFER_DEBUG("[BUFFER] Add failed: buffer full\n");
        return false;
    }
    
    // 添加到head位置
    buffer->blocks[buffer->head] = *block;
    buffer->blocks[buffer->head].valid = true;
    
    // 更新head指针（环形）
    buffer->head = (buffer->head + 1) % MOTION_BUFFER_SIZE;
    buffer->count++;
    
    BUFFER_DEBUG("[BUFFER] Added: t=%dms S%d->%d deg (count=%d)\n",
                 (int)block->timestamp_ms, block->servo_id, 
                 (int)block->target_angle, buffer->count);
    
    return true;
}

motion_block_t* motion_buffer_peek(motion_buffer_t *buffer) {
    if (buffer == NULL || buffer->count == 0) {
        return NULL;
    }
    
    return &buffer->blocks[buffer->tail];
}

bool motion_buffer_remove(motion_buffer_t *buffer) {
    if (buffer == NULL || buffer->count == 0) {
        return false;
    }
    
    // 标记为无效
    buffer->blocks[buffer->tail].valid = false;
    
    // 更新tail指针（环形）
    buffer->tail = (buffer->tail + 1) % MOTION_BUFFER_SIZE;
    buffer->count--;
    
    return true;
}

void motion_buffer_clear(motion_buffer_t *buffer) {
    if (buffer == NULL) {
        return;
    }
    
    int old_count = buffer->count;
    
    buffer->head = 0;
    buffer->tail = 0;
    buffer->count = 0;
    buffer->running = false;
    buffer->paused = false;
    
    // 清空所有块
    for (int i = 0; i < MOTION_BUFFER_SIZE; i++) {
        buffer->blocks[i].valid = false;
    }
    
    BUFFER_DEBUG("[BUFFER] Cleared (%d blocks removed)\n", old_count);
}

bool motion_buffer_start(motion_buffer_t *buffer) {
    if (buffer == NULL || buffer->count == 0) {
        return false;
    }
    
    buffer->running = true;
    buffer->paused = false;
    buffer->start_time = to_ms_since_boot(get_absolute_time());
    
    BUFFER_DEBUG("[BUFFER] Started execution with %d blocks\n", buffer->count);
    
    return true;
}

void motion_buffer_stop(motion_buffer_t *buffer) {
    if (buffer == NULL) {
        return;
    }
    
    buffer->running = false;
    buffer->paused = false;
}

void motion_buffer_pause(motion_buffer_t *buffer) {
    if (buffer == NULL) {
        return;
    }
    
    buffer->paused = true;
}

void motion_buffer_resume(motion_buffer_t *buffer) {
    if (buffer == NULL) {
        return;
    }
    
    buffer->paused = false;
}

uint8_t motion_buffer_available(const motion_buffer_t *buffer) {
    if (buffer == NULL) {
        return 0;
    }
    
    return MOTION_BUFFER_SIZE - buffer->count;
}

bool motion_buffer_is_empty(const motion_buffer_t *buffer) {
    if (buffer == NULL) {
        return true;
    }
    
    return buffer->count == 0;
}

bool motion_buffer_is_full(const motion_buffer_t *buffer) {
    if (buffer == NULL) {
        return false;
    }
    
    return buffer->count >= MOTION_BUFFER_SIZE;
}

bool motion_buffer_is_running(const motion_buffer_t *buffer) {
    if (buffer == NULL) {
        return false;
    }
    
    return buffer->running;
}

void motion_buffer_get_status(const motion_buffer_t *buffer, char *buf, size_t size) {
    if (buffer == NULL || buf == NULL || size == 0) {
        return;
    }
    
    snprintf(buf, size, 
             "Buffer: count=%d/%d, head=%d, tail=%d, running=%d, paused=%d",
             buffer->count, MOTION_BUFFER_SIZE,
             buffer->head, buffer->tail,
             buffer->running, buffer->paused);
}


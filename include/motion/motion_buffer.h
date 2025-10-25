/**
 * @file motion_buffer.h
 * @brief 运动指令缓冲区管理
 * @description 实现运动指令的缓冲、调度和执行，支持时间戳驱动的自主运动规划
 * @date 2025-10-26
 */

#ifndef MOTION_BUFFER_H
#define MOTION_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include "config/config.h"

// ==================== 配置参数 ====================

#define MOTION_BUFFER_SIZE      32      // 运动缓冲区大小（支持32条指令）
#define SCHEDULER_TICK_MS       10      // 调度器检查间隔（10ms）

// ==================== 运动块结构 ====================

/**
 * @brief 单个运动指令块（类似CNC的block）
 * @description 包含一个舵机在特定时间的目标状态和运动参数
 */
typedef struct {
    uint32_t timestamp_ms;       // 执行时间戳（从start开始计时，单位ms）
    uint8_t servo_id;            // 舵机ID (0-17)
    float target_angle;          // 目标角度（度）
    float velocity;              // 速度（度/秒）
    float acceleration;          // 加速度（度/秒²）
    float deceleration;          // 减速度（度/秒²，0表示与加速度相同）
    bool valid;                  // 是否有效
} motion_block_t;

/**
 * @brief 运动缓冲区（环形队列）
 * @description 管理待执行的运动指令，支持流式添加和按时间调度执行
 */
typedef struct {
    motion_block_t blocks[MOTION_BUFFER_SIZE];  // 缓冲区数组
    uint8_t head;                // 写入位置（生产者索引）
    uint8_t tail;                // 读取位置（消费者索引）
    uint8_t count;               // 当前指令数量
    bool running;                // 是否正在执行
    uint32_t start_time;         // 开始执行的时间戳（绝对时间）
    bool paused;                 // 是否暂停
} motion_buffer_t;

// ==================== 缓冲区管理函数 ====================

/**
 * @brief 初始化运动缓冲区
 * @param buffer 缓冲区指针
 */
void motion_buffer_init(motion_buffer_t *buffer);

/**
 * @brief 添加运动指令到缓冲区
 * @param buffer 缓冲区指针
 * @param block 运动指令块
 * @return true 添加成功, false 缓冲区已满
 */
bool motion_buffer_add(motion_buffer_t *buffer, const motion_block_t *block);

/**
 * @brief 获取下一个待执行的运动指令（不移除）
 * @param buffer 缓冲区指针
 * @return 运动指令块指针，如果缓冲区为空返回NULL
 */
motion_block_t* motion_buffer_peek(motion_buffer_t *buffer);

/**
 * @brief 移除已执行的运动指令
 * @param buffer 缓冲区指针
 * @return true 移除成功, false 缓冲区为空
 */
bool motion_buffer_remove(motion_buffer_t *buffer);

/**
 * @brief 清空缓冲区
 * @param buffer 缓冲区指针
 */
void motion_buffer_clear(motion_buffer_t *buffer);

/**
 * @brief 开始执行缓冲区中的指令
 * @param buffer 缓冲区指针
 * @return true 开始成功, false 缓冲区为空
 */
bool motion_buffer_start(motion_buffer_t *buffer);

/**
 * @brief 停止执行
 * @param buffer 缓冲区指针
 */
void motion_buffer_stop(motion_buffer_t *buffer);

/**
 * @brief 暂停执行
 * @param buffer 缓冲区指针
 */
void motion_buffer_pause(motion_buffer_t *buffer);

/**
 * @brief 恢复执行
 * @param buffer 缓冲区指针
 */
void motion_buffer_resume(motion_buffer_t *buffer);

/**
 * @brief 获取缓冲区可用空间
 * @param buffer 缓冲区指针
 * @return 可用空间数量
 */
uint8_t motion_buffer_available(const motion_buffer_t *buffer);

/**
 * @brief 检查缓冲区是否为空
 * @param buffer 缓冲区指针
 * @return true 为空, false 不为空
 */
bool motion_buffer_is_empty(const motion_buffer_t *buffer);

/**
 * @brief 检查缓冲区是否已满
 * @param buffer 缓冲区指针
 * @return true 已满, false 未满
 */
bool motion_buffer_is_full(const motion_buffer_t *buffer);

/**
 * @brief 检查是否正在执行
 * @param buffer 缓冲区指针
 * @return true 正在执行, false 已停止
 */
bool motion_buffer_is_running(const motion_buffer_t *buffer);

/**
 * @brief 获取缓冲区状态信息（用于调试）
 * @param buffer 缓冲区指针
 * @param buf 字符串缓冲区
 * @param size 缓冲区大小
 */
void motion_buffer_get_status(const motion_buffer_t *buffer, char *buf, size_t size);

#endif // MOTION_BUFFER_H


/**
 * @file motion_scheduler.h
 * @brief 运动调度器 - 基于时间戳的自主运动执行
 * @date 2025-10-25
 * 
 * 职责：
 * - 管理全局运动缓冲区
 * - 根据时间戳自动调度执行运动指令
 * - 提供缓冲区状态查询接口
 */

#ifndef MOTION_SCHEDULER_H
#define MOTION_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>
#include "motion/motion_buffer.h"

/**
 * @brief 运动执行回调函数类型
 * @param servo_id 舵机ID
 * @param target_angle 目标角度
 * @param velocity 速度
 * @param acceleration 加速度
 * @param deceleration 减速度
 */
typedef void (*motion_execute_callback_t)(
    uint8_t servo_id,
    float target_angle,
    float velocity,
    float acceleration,
    float deceleration
);

/**
 * @brief 初始化运动调度器
 */
void motion_scheduler_init(void);

/**
 * @brief 设置运动执行回调函数
 * @param callback 回调函数指针
 */
void motion_scheduler_set_callback(motion_execute_callback_t callback);

/**
 * @brief 更新调度器（定期调用）
 * @description 检查缓冲区，执行到时的运动指令
 */
void motion_scheduler_update(void);

/**
 * @brief 获取全局运动缓冲区指针
 * @return 缓冲区指针
 */
motion_buffer_t* motion_scheduler_get_buffer(void);

/**
 * @brief 添加运动指令到缓冲区
 * @param block 运动指令块
 * @return true 成功, false 失败（缓冲区满）
 */
bool motion_scheduler_add_block(const motion_block_t *block);

/**
 * @brief 启动调度器执行
 * @return true 成功, false 失败
 */
bool motion_scheduler_start(void);

/**
 * @brief 停止调度器
 */
void motion_scheduler_stop(void);

/**
 * @brief 暂停调度器
 */
void motion_scheduler_pause(void);

/**
 * @brief 恢复调度器
 */
void motion_scheduler_resume(void);

/**
 * @brief 清空缓冲区
 */
void motion_scheduler_clear(void);

/**
 * @brief 检查调度器是否正在运行
 * @return true 运行中, false 已停止
 */
bool motion_scheduler_is_running(void);

/**
 * @brief 检查调度器是否暂停
 * @return true 暂停中, false 未暂停
 */
bool motion_scheduler_is_paused(void);

/**
 * @brief 获取缓冲区中的指令数量
 * @return 指令数量
 */
uint8_t motion_scheduler_get_count(void);

/**
 * @brief 获取缓冲区可用空间
 * @return 可用空间数量
 */
uint8_t motion_scheduler_get_available(void);

#endif // MOTION_SCHEDULER_H


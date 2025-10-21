/**
 * @file interpolation.h
 * @brief 运动插值算法
 * @date 2025-10-21
 */

#ifndef INTERPOLATION_H
#define INTERPOLATION_H

#include <stdint.h>
#include <stdbool.h>
#include "config/config.h"

/**
 * @brief 插值类型
 */
typedef enum {
    INTERP_TYPE_LINEAR = 0,     // 线性插值
    INTERP_TYPE_S_CURVE,        // S曲线插值
    INTERP_TYPE_TRAPEZOID       // 梯形曲线
} interp_type_t;

/**
 * @brief 运动状态
 */
typedef enum {
    MOTION_STATE_IDLE = 0,      // 空闲
    MOTION_STATE_MOVING,        // 运动中
    MOTION_STATE_REACHED        // 到达目标
} motion_state_t;

/**
 * @brief 单轴插值器
 */
typedef struct {
    float start_pos;            // 起始位置
    float target_pos;           // 目标位置
    float current_pos;          // 当前位置
    uint32_t start_time;        // 起始时间（ms）
    uint32_t duration;          // 运动时间（ms）
    uint32_t elapsed_time;      // 已过时间（ms）
    interp_type_t type;         // 插值类型
    motion_state_t state;       // 运动状态
} interpolator_t;

/**
 * @brief 多轴插值器
 */
typedef struct {
    interpolator_t axes[SERVO_COUNT];   // 18个轴的插值器
    bool synchronized;                  // 是否同步运动
    uint32_t total_duration;            // 总运动时间
} multi_axis_interpolator_t;

/**
 * @brief 初始化插值器
 * @param interp 插值器指针
 */
void interpolator_init(interpolator_t *interp);

/**
 * @brief 初始化多轴插值器
 * @param multi_interp 多轴插值器指针
 */
void multi_interpolator_init(multi_axis_interpolator_t *multi_interp);

/**
 * @brief 设置单轴运动参数
 * @param interp 插值器指针
 * @param start_pos 起始位置
 * @param target_pos 目标位置
 * @param duration 运动时间（ms）
 * @param type 插值类型
 */
void interpolator_set_motion(interpolator_t *interp, float start_pos, float target_pos,
                              uint32_t duration, interp_type_t type);

/**
 * @brief 更新插值器（每个插值周期调用）
 * @param interp 插值器指针
 * @param delta_time 时间增量（ms）
 * @return 当前位置
 */
float interpolator_update(interpolator_t *interp, uint32_t delta_time);

/**
 * @brief 线性插值计算
 * @param start 起始值
 * @param end 结束值
 * @param ratio 插值比例（0.0-1.0）
 * @return 插值结果
 */
float interpolate_linear(float start, float end, float ratio);

/**
 * @brief S曲线插值计算
 * @param start 起始值
 * @param end 结束值
 * @param ratio 插值比例（0.0-1.0）
 * @return 插值结果
 */
float interpolate_s_curve(float start, float end, float ratio);

/**
 * @brief 检查是否到达目标
 * @param interp 插值器指针
 * @return true 已到达, false 运动中
 */
bool interpolator_is_reached(const interpolator_t *interp);

/**
 * @brief 获取运动状态
 * @param interp 插值器指针
 * @return 运动状态
 */
motion_state_t interpolator_get_state(const interpolator_t *interp);

/**
 * @brief 停止运动
 * @param interp 插值器指针
 */
void interpolator_stop(interpolator_t *interp);

/**
 * @brief 设置多轴同步运动
 * @param multi_interp 多轴插值器指针
 * @param start_positions 起始位置数组
 * @param target_positions 目标位置数组
 * @param duration 运动时间（ms）
 * @param type 插值类型
 */
void multi_interpolator_set_motion(multi_axis_interpolator_t *multi_interp,
                                    const float start_positions[SERVO_COUNT],
                                    const float target_positions[SERVO_COUNT],
                                    uint32_t duration, interp_type_t type);

/**
 * @brief 更新多轴插值器
 * @param multi_interp 多轴插值器指针
 * @param delta_time 时间增量（ms）
 * @param output_positions 输出位置数组
 */
void multi_interpolator_update(multi_axis_interpolator_t *multi_interp,
                                uint32_t delta_time,
                                float output_positions[SERVO_COUNT]);

/**
 * @brief 检查所有轴是否到达
 * @param multi_interp 多轴插值器指针
 * @return true 全部到达, false 运动中
 */
bool multi_interpolator_all_reached(const multi_axis_interpolator_t *multi_interp);

#endif // INTERPOLATION_H


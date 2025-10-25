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
    INTERP_TYPE_TRAPEZOID       // 梯形速度曲线
} interp_type_t;

/**
 * @brief 运动参数结构（用于梯形速度曲线）
 */
typedef struct {
    float max_velocity;      // 最大速度 (度/秒)
    float acceleration;      // 加速度 (度/秒²)
    float deceleration;      // 减速度 (度/秒²) 如果为0则使用acceleration
} motion_params_t;

/**
 * @brief 轨迹点结构
 */
#define MAX_TRAJECTORY_POINTS 50  // 最多支持50个轨迹点

typedef struct {
    float position;              // 目标位置 (度)
    motion_params_t params;      // 运动参数
    uint32_t dwell_time_ms;      // 到达后停留时间（毫秒）
} trajectory_point_t;

/**
 * @brief 轨迹队列结构
 */
typedef struct {
    trajectory_point_t points[MAX_TRAJECTORY_POINTS];  // 轨迹点数组
    uint8_t count;                // 当前点数
    uint8_t current_index;        // 当前执行到第几个点
    uint32_t dwell_start_time;    // 停留开始时间
    bool loop;                    // 是否循环执行
    bool running;                 // 是否正在执行
} trajectory_queue_t;

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
    // 基础参数
    float start_pos;            // 起始位置
    float target_pos;           // 目标位置
    float current_pos;          // 当前位置
    uint32_t start_time;        // 起始时间（ms）
    uint32_t duration;          // 运动时间（ms）
    uint32_t elapsed_time;      // 已过时间（ms）
    interp_type_t type;         // 插值类型
    motion_state_t state;       // 运动状态
    
    // 梯形速度曲线参数
    motion_params_t motion_params;  // 运动参数（速度、加速度）
    float distance;             // 移动距离（带符号）
    float t_accel;              // 加速阶段时间（秒）
    float t_const;              // 匀速阶段时间（秒）
    float t_decel;              // 减速阶段时间（秒）
    float v_max_actual;         // 实际达到的最大速度（度/秒）
    bool use_trapezoid;         // 是否使用梯形速度曲线
    
    // 轨迹规划
    trajectory_queue_t* trajectory;  // 轨迹队列指针（NULL表示不使用轨迹）
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

// ==================== 梯形速度曲线函数 ====================

/**
 * @brief 设置梯形速度运动
 * @param interp 插值器指针
 * @param start_pos 起始位置
 * @param target_pos 目标位置
 * @param params 运动参数（速度、加速度）
 */
void interpolator_set_trapezoid_motion(interpolator_t *interp, 
                                       float start_pos, 
                                       float target_pos,
                                       const motion_params_t *params);

/**
 * @brief 梯形速度插值计算
 * @param start 起始值
 * @param end 结束值
 * @param t_current 当前时间（秒）
 * @param distance 移动距离（带符号）
 * @param t_accel 加速时间
 * @param t_const 匀速时间
 * @param t_decel 减速时间
 * @param v_max 最大速度
 * @return 插值结果
 */
float interpolate_trapezoid(float start, float end, float t_current,
                            float distance, float t_accel, float t_const, 
                            float t_decel, float v_max);

// ==================== 轨迹规划函数 ====================

/**
 * @brief 初始化轨迹队列
 * @param traj 轨迹队列指针
 */
void trajectory_init(trajectory_queue_t* traj);

/**
 * @brief 添加轨迹点
 * @param traj 轨迹队列指针
 * @param position 目标位置
 * @param params 运动参数
 * @param dwell_time_ms 停留时间（毫秒）
 * @return true 添加成功, false 队列已满
 */
bool trajectory_add_point(trajectory_queue_t* traj, 
                         float position,
                         const motion_params_t *params,
                         uint32_t dwell_time_ms);

/**
 * @brief 清空轨迹
 * @param traj 轨迹队列指针
 */
void trajectory_clear(trajectory_queue_t* traj);

/**
 * @brief 开始执行轨迹
 * @param traj 轨迹队列指针
 * @param loop 是否循环执行
 * @return true 开始成功, false 轨迹为空
 */
bool trajectory_start(trajectory_queue_t* traj, bool loop);

/**
 * @brief 停止执行轨迹
 * @param traj 轨迹队列指针
 */
void trajectory_stop(trajectory_queue_t* traj);

/**
 * @brief 获取轨迹当前点数
 * @param traj 轨迹队列指针
 * @return 轨迹点数量
 */
uint8_t trajectory_get_count(const trajectory_queue_t* traj);

/**
 * @brief 检查轨迹是否正在执行
 * @param traj 轨迹队列指针
 * @return true 正在执行, false 已停止
 */
bool trajectory_is_running(const trajectory_queue_t* traj);

#endif // INTERPOLATION_H


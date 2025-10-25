/**
 * @file interpolation.c
 * @brief 运动插值算法实现
 * @date 2025-10-21
 */

#include "motion/interpolation.h"
#include "utils/usb_bridge.h"
#include "pico/stdlib.h"
#include <math.h>
#include <string.h>

// 辅助宏：限制值在范围内
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

// 前向声明
static bool trajectory_auto_execute(interpolator_t *interp, uint32_t current_time);

void interpolator_init(interpolator_t *interp) {
    memset(interp, 0, sizeof(interpolator_t));
    interp->state = MOTION_STATE_IDLE;
    interp->type = INTERP_TYPE_LINEAR;
}

void multi_interpolator_init(multi_axis_interpolator_t *multi_interp) {
    memset(multi_interp, 0, sizeof(multi_axis_interpolator_t));
    
    for (int i = 0; i < SERVO_COUNT; i++) {
        interpolator_init(&multi_interp->axes[i]);
    }
    
    multi_interp->synchronized = false;
}

void interpolator_set_motion(interpolator_t *interp, float start_pos, float target_pos,
                              uint32_t duration, interp_type_t type) {
    interp->start_pos = start_pos;
    interp->target_pos = target_pos;
    interp->current_pos = start_pos;
    interp->start_time = to_ms_since_boot(get_absolute_time());
    interp->duration = duration;
    interp->elapsed_time = 0;
    interp->type = type;
    interp->state = MOTION_STATE_MOVING;
}

float interpolate_linear(float start, float end, float ratio) {
    // 线性插值：P = P0 + (P1 - P0) * t
    ratio = CLAMP(ratio, 0.0f, 1.0f);
    return start + (end - start) * ratio;
}

float interpolate_s_curve(float start, float end, float ratio) {
    // S曲线插值：使用平滑函数 3t² - 2t³
    ratio = CLAMP(ratio, 0.0f, 1.0f);
    
    // 平滑插值系数：smoothstep函数
    float smooth_ratio = ratio * ratio * (3.0f - 2.0f * ratio);
    
    return start + (end - start) * smooth_ratio;
}

float interpolator_update(interpolator_t *interp, uint32_t delta_time) {
    // 轨迹自动执行
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    trajectory_auto_execute(interp, current_time);
    
    if (interp->state != MOTION_STATE_MOVING) {
        return interp->current_pos;
    }
    
    // 更新已过时间
    interp->elapsed_time += delta_time;
    
    // 计算插值比例
    float ratio;
    if (interp->duration > 0) {
        ratio = (float)interp->elapsed_time / (float)interp->duration;
    } else {
        ratio = 1.0f;  // 立即到达
    }
    
    // 检查是否到达
    if (ratio >= 1.0f) {
        interp->current_pos = interp->target_pos;
        interp->state = MOTION_STATE_REACHED;
        
        #if DEBUG_MOTION_SUMMARY
        usb_bridge_printf("[MOTION] Motion COMPLETE: final_pos=%d deg\n", (int)interp->current_pos);
        #endif
        
        return interp->current_pos;
    }
    
    #if DEBUG_MOTION_PROGRESS
    // 在关键进度点输出（25%, 50%, 75%）
    static uint8_t last_progress_milestone = 0;
    uint8_t current_milestone = (uint8_t)(ratio * 100);
    
    if ((current_milestone >= 25 && last_progress_milestone < 25) ||
        (current_milestone >= 50 && last_progress_milestone < 50) ||
        (current_milestone >= 75 && last_progress_milestone < 75)) {
        int pos_int = (int)interp->current_pos;
        usb_bridge_printf("[MOTION] Progress: %d%% pos=%ddeg elapsed=%dms\n",
                         (current_milestone/25)*25, pos_int, (int)interp->elapsed_time);
        last_progress_milestone = current_milestone;
    }
    
    // 运动完成时重置
    if (ratio < 0.1f) {
        last_progress_milestone = 0;
    }
    #endif
    
    // 根据插值类型计算当前位置
    switch (interp->type) {
        case INTERP_TYPE_LINEAR:
            interp->current_pos = interpolate_linear(interp->start_pos, interp->target_pos, ratio);
            break;
            
        case INTERP_TYPE_S_CURVE:
            interp->current_pos = interpolate_s_curve(interp->start_pos, interp->target_pos, ratio);
            break;
            
        case INTERP_TYPE_TRAPEZOID:
            if (interp->use_trapezoid) {
                // 使用梯形速度曲线插值
                float t_current_sec = (float)interp->elapsed_time / 1000.0f;
                interp->current_pos = interpolate_trapezoid(
                    interp->start_pos,
                    interp->target_pos,
                    t_current_sec,
                    interp->distance,
                    interp->t_accel,
                    interp->t_const,
                    interp->t_decel,
                    interp->v_max_actual
                );
            } else {
                // 回退到线性插值
                interp->current_pos = interpolate_linear(interp->start_pos, interp->target_pos, ratio);
            }
            break;
            
        default:
            interp->current_pos = interpolate_linear(interp->start_pos, interp->target_pos, ratio);
            break;
    }
    
    return interp->current_pos;
}

bool interpolator_is_reached(const interpolator_t *interp) {
    return interp->state == MOTION_STATE_REACHED;
}

motion_state_t interpolator_get_state(const interpolator_t *interp) {
    return interp->state;
}

void interpolator_stop(interpolator_t *interp) {
    interp->target_pos = interp->current_pos;
    interp->state = MOTION_STATE_IDLE;
}

void multi_interpolator_set_motion(multi_axis_interpolator_t *multi_interp,
                                    const float start_positions[SERVO_COUNT],
                                    const float target_positions[SERVO_COUNT],
                                    uint32_t duration, interp_type_t type) {
    multi_interp->synchronized = true;
    multi_interp->total_duration = duration;
    
    for (int i = 0; i < SERVO_COUNT; i++) {
        interpolator_set_motion(&multi_interp->axes[i],
                                start_positions[i],
                                target_positions[i],
                                duration,
                                type);
    }
}

void multi_interpolator_update(multi_axis_interpolator_t *multi_interp,
                                uint32_t delta_time,
                                float output_positions[SERVO_COUNT]) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        output_positions[i] = interpolator_update(&multi_interp->axes[i], delta_time);
    }
}

bool multi_interpolator_all_reached(const multi_axis_interpolator_t *multi_interp) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (!interpolator_is_reached(&multi_interp->axes[i])) {
            return false;
        }
    }
    return true;
}

// ==================== 梯形速度曲线实现 ====================

/**
 * @brief 计算梯形速度曲线参数
 * @param distance 移动距离（绝对值）
 * @param max_velocity 最大速度（度/秒）
 * @param acceleration 加速度（度/秒²）
 * @param deceleration 减速度（度/秒²）
 * @param t_accel 输出：加速时间（秒）
 * @param t_const 输出：匀速时间（秒）
 * @param t_decel 输出：减速时间（秒）
 * @param v_max_actual 输出：实际最大速度（度/秒）
 */
static void calculate_trapezoid_profile(float distance, 
                                        float max_velocity,
                                        float acceleration,
                                        float deceleration,
                                        float *t_accel,
                                        float *t_const,
                                        float *t_decel,
                                        float *v_max_actual) {
    // 确保参数有效
    if (distance <= 0.0f || max_velocity <= 0.0f || 
        acceleration <= 0.0f || deceleration <= 0.0f) {
        *t_accel = 0.0f;
        *t_const = 0.0f;
        *t_decel = 0.0f;
        *v_max_actual = 0.0f;
        return;
    }
    
    // 计算达到最大速度所需的加速/减速距离
    float d_accel = (max_velocity * max_velocity) / (2.0f * acceleration);
    float d_decel = (max_velocity * max_velocity) / (2.0f * deceleration);
    
    // 判断是否能够达到最大速度
    if (d_accel + d_decel <= distance) {
        // 标准梯形：能够达到最大速度
        *v_max_actual = max_velocity;
        *t_accel = max_velocity / acceleration;
        *t_decel = max_velocity / deceleration;
        *t_const = (distance - d_accel - d_decel) / max_velocity;
    } else {
        // 三角形：无法达到最大速度
        // 求解：d_accel + d_decel = distance
        // v² / (2a) + v² / (2d) = distance
        // v² * (1/(2a) + 1/(2d)) = distance
        // v² = distance / (1/(2a) + 1/(2d))
        // v = sqrt(distance / (1/(2a) + 1/(2d)))
        
        float inv_2a = 1.0f / (2.0f * acceleration);
        float inv_2d = 1.0f / (2.0f * deceleration);
        *v_max_actual = sqrtf(distance / (inv_2a + inv_2d));
        *t_accel = *v_max_actual / acceleration;
        *t_decel = *v_max_actual / deceleration;
        *t_const = 0.0f;  // 无匀速阶段
    }
}

void interpolator_set_trapezoid_motion(interpolator_t *interp, 
                                       float start_pos, 
                                       float target_pos,
                                       const motion_params_t *params) {
    // 基本参数
    interp->start_pos = start_pos;
    interp->target_pos = target_pos;
    interp->current_pos = start_pos;
    interp->start_time = to_ms_since_boot(get_absolute_time());
    interp->elapsed_time = 0;
    interp->type = INTERP_TYPE_TRAPEZOID;
    interp->state = MOTION_STATE_MOVING;
    interp->use_trapezoid = true;
    
    // 运动参数
    interp->motion_params = *params;
    
    // 如果减速度为0，使用加速度
    if (interp->motion_params.deceleration <= 0.0f) {
        interp->motion_params.deceleration = interp->motion_params.acceleration;
    }
    
    // 计算移动距离（带符号）
    interp->distance = target_pos - start_pos;
    float abs_distance = fabsf(interp->distance);
    
    // 计算梯形速度曲线参数
    calculate_trapezoid_profile(abs_distance,
                                params->max_velocity,
                                params->acceleration,
                                interp->motion_params.deceleration,
                                &interp->t_accel,
                                &interp->t_const,
                                &interp->t_decel,
                                &interp->v_max_actual);
    
    // 计算总时间（秒）并转换为毫秒
    float total_time_sec = interp->t_accel + interp->t_const + interp->t_decel;
    interp->duration = (uint32_t)(total_time_sec * 1000.0f);
    
    #if DEBUG_MOTION_SUMMARY
    // 运动曲线摘要
    int t_a = (int)(interp->t_accel * 1000);
    int t_c = (int)(interp->t_const * 1000);
    int t_d = (int)(interp->t_decel * 1000);
    usb_bridge_printf("[MOTION] Trapezoid profile: accel=%dms const=%dms decel=%dms total=%dms\n",
                     t_a, t_c, t_d, (int)interp->duration);
    #endif
}

float interpolate_trapezoid(float start, float end, float t_current,
                            float distance, float t_accel, float t_const, 
                            float t_decel, float v_max) {
    // 总时间
    float t_total = t_accel + t_const + t_decel;
    
    // 限制时间范围
    if (t_current <= 0.0f) {
        return start;
    }
    if (t_current >= t_total) {
        return end;
    }
    
    float abs_distance = fabsf(distance);
    float s = 0.0f;  // 移动的距离
    
    // 判断当前处于哪个阶段
    static uint8_t last_phase = 0xFF;
    uint8_t current_phase;
    
    if (t_current < t_accel) {
        // 阶段1：加速
        current_phase = 1;
        // s(t) = 0.5 * a * t²
        float accel = v_max / t_accel;
        s = 0.5f * accel * t_current * t_current;
        
        #if DEBUG_MOTION_PROGRESS
        if (last_phase != 1) {
            int accel_int = (int)(accel * 10);
            usb_bridge_printf("[MOTION-PHASE] >>> ACCEL: a=%d.%d deg/s^2, target_v=%d deg/s\n",
                             accel_int/10, accel_int%10, (int)v_max);
            last_phase = 1;
        }
        #endif
    }
    else if (t_current < t_accel + t_const) {
        // 阶段2：匀速
        current_phase = 2;
        // s(t) = s_accel + v_max * (t - t_accel)
        float accel = v_max / t_accel;
        float s_accel = 0.5f * accel * t_accel * t_accel;
        float dt = t_current - t_accel;
        s = s_accel + v_max * dt;
        
        #if DEBUG_MOTION_PROGRESS
        if (last_phase != 2) {
            usb_bridge_printf("[MOTION-PHASE] >>> CONST: v=%d deg/s (cruising)\n", (int)v_max);
            last_phase = 2;
        }
        #endif
    }
    else {
        // 阶段3：减速
        current_phase = 3;
        // s(t) = s_accel + s_const + v_max * t' - 0.5 * d * t'²
        float accel = v_max / t_accel;
        float decel = v_max / t_decel;
        float s_accel = 0.5f * accel * t_accel * t_accel;
        float s_const = v_max * t_const;
        float dt = t_current - t_accel - t_const;
        s = s_accel + s_const + v_max * dt - 0.5f * decel * dt * dt;
        
        #if DEBUG_MOTION_PROGRESS
        if (last_phase != 3) {
            int decel_int = (int)(decel * 10);
            usb_bridge_printf("[MOTION-PHASE] >>> DECEL: d=%d.%d deg/s^2\n",
                             decel_int/10, decel_int%10);
            last_phase = 3;
        }
        #endif
    }
    
    // 运动结束时重置阶段标记
    if (t_current < 0.02f) {  // 接近开始时
        last_phase = 0xFF;
    }
    
    // 计算位置（考虑方向）
    float ratio = s / abs_distance;
    ratio = CLAMP(ratio, 0.0f, 1.0f);
    
    return start + distance * ratio;
}

// ==================== 轨迹规划实现 ====================

void trajectory_init(trajectory_queue_t* traj) {
    memset(traj, 0, sizeof(trajectory_queue_t));
    traj->count = 0;
    traj->current_index = 0;
    traj->loop = false;
    traj->running = false;
}

bool trajectory_add_point(trajectory_queue_t* traj, 
                         float position,
                         const motion_params_t *params,
                         uint32_t dwell_time_ms) {
    // 检查队列是否已满
    if (traj->count >= MAX_TRAJECTORY_POINTS) {
        return false;
    }
    
    // 添加新点
    trajectory_point_t *point = &traj->points[traj->count];
    point->position = position;
    point->params = *params;
    point->dwell_time_ms = dwell_time_ms;
    
    traj->count++;
    return true;
}

void trajectory_clear(trajectory_queue_t* traj) {
    traj->count = 0;
    traj->current_index = 0;
    traj->running = false;
    traj->dwell_start_time = 0;
}

bool trajectory_start(trajectory_queue_t* traj, bool loop) {
    // 检查轨迹是否为空
    if (traj->count == 0) {
        return false;
    }
    
    // 开始执行
    traj->running = true;
    traj->loop = loop;
    traj->current_index = 0;
    traj->dwell_start_time = 0;
    
    return true;
}

void trajectory_stop(trajectory_queue_t* traj) {
    traj->running = false;
    traj->dwell_start_time = 0;
}

uint8_t trajectory_get_count(const trajectory_queue_t* traj) {
    return traj->count;
}

bool trajectory_is_running(const trajectory_queue_t* traj) {
    return traj->running;
}

/**
 * @brief 轨迹自动执行 - 在插值器更新中调用
 * @param interp 插值器指针
 * @param current_time 当前时间（毫秒）
 * @return true 正在执行轨迹, false 无轨迹或轨迹结束
 */
static bool trajectory_auto_execute(interpolator_t *interp, uint32_t current_time) {
    // 检查是否有轨迹
    if (interp->trajectory == NULL || !interp->trajectory->running) {
        return false;
    }
    
    trajectory_queue_t *traj = interp->trajectory;
    
    // 如果当前正在运动，返回true
    if (interp->state == MOTION_STATE_MOVING) {
        return true;
    }
    
    // 如果刚到达，检查是否需要停留
    if (interp->state == MOTION_STATE_REACHED) {
        trajectory_point_t *current_point = &traj->points[traj->current_index];
        
        // 检查是否需要停留
        if (current_point->dwell_time_ms > 0) {
            // 如果刚到达，记录停留开始时间
            if (traj->dwell_start_time == 0) {
                traj->dwell_start_time = current_time;
                return true;
            }
            
            // 检查停留时间是否结束
            uint32_t elapsed = current_time - traj->dwell_start_time;
            if (elapsed < current_point->dwell_time_ms) {
                return true;  // 还在停留中
            }
            
            // 停留结束，准备下一个点
            traj->dwell_start_time = 0;
        }
        
        // 移动到下一个点
        traj->current_index++;
        
        // 检查是否到达最后一个点
        if (traj->current_index >= traj->count) {
            if (traj->loop) {
                // 循环：回到第一个点
                traj->current_index = 0;
            } else {
                // 不循环：轨迹结束
                traj->running = false;
                return false;
            }
        }
    }
    
    // 开始下一个点的运动
    trajectory_point_t *next_point = &traj->points[traj->current_index];
    interpolator_set_trapezoid_motion(interp,
                                      interp->current_pos,
                                      next_point->position,
                                      &next_point->params);
    
    return true;
}


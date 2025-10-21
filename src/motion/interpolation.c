/**
 * @file interpolation.c
 * @brief 运动插值算法实现
 * @date 2025-10-21
 */

#include "motion/interpolation.h"
#include "pico/stdlib.h"
#include <math.h>
#include <string.h>

// 辅助宏：限制值在范围内
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

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
        return interp->current_pos;
    }
    
    // 根据插值类型计算当前位置
    switch (interp->type) {
        case INTERP_TYPE_LINEAR:
            interp->current_pos = interpolate_linear(interp->start_pos, interp->target_pos, ratio);
            break;
            
        case INTERP_TYPE_S_CURVE:
            interp->current_pos = interpolate_s_curve(interp->start_pos, interp->target_pos, ratio);
            break;
            
        case INTERP_TYPE_TRAPEZOID:
            // TODO: 实现梯形曲线
            interp->current_pos = interpolate_linear(interp->start_pos, interp->target_pos, ratio);
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


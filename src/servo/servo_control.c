/**
 * @file servo_control.c
 * @brief 舵机控制层实现
 * @date 2025-10-21
 */

#include "servo/servo_control.h"
#include "pwm/pwm_driver.h"
#include "utils/error_handler.h"
#include "config/config.h"
#include <string.h>
#include <math.h>
#include <stdio.h>

// ==================== 调试宏 ====================
#if DEBUG_SERVO
    #define SERVO_DEBUG(...) printf(__VA_ARGS__)
#else
    #define SERVO_DEBUG(...) ((void)0)
#endif

// 舵机参数数组
static servo_t g_servos[SERVO_COUNT];

// 默认校准参数
static const servo_calibration_t g_default_calibration = {
    .min_pulse_us = SERVO_MIN_PULSE_US,
    .max_pulse_us = SERVO_MAX_PULSE_US,
    .offset_us = 0,
    .reverse = false
};

bool servo_control_init(void) {
    // 初始化所有舵机参数
    for (int i = 0; i < SERVO_COUNT; i++) {
        g_servos[i].id = i;
        g_servos[i].calibration = g_default_calibration;
        g_servos[i].current_angle = 90.0f;  // 中位
        g_servos[i].target_angle = 90.0f;
        g_servos[i].current_pulse_us = SERVO_CENTER_PULSE_US;
        g_servos[i].target_pulse_us = SERVO_CENTER_PULSE_US;
        g_servos[i].enabled = false;
    }
    
    return true;
}

uint16_t servo_angle_to_pulse(uint8_t id, float angle) {
    if (id >= SERVO_COUNT) {
        return SERVO_CENTER_PULSE_US;
    }
    
    const servo_calibration_t *cal = &g_servos[id].calibration;
    
    // 限制角度范围
    if (angle < SERVO_MIN_ANGLE) angle = SERVO_MIN_ANGLE;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;
    
    // 方向反转
    if (cal->reverse) {
        angle = SERVO_MAX_ANGLE - angle;
    }
    
    // 线性映射：0-180度 -> min_pulse-max_pulse
    float pulse_range = cal->max_pulse_us - cal->min_pulse_us;
    float angle_range = SERVO_MAX_ANGLE - SERVO_MIN_ANGLE;
    float pulse = cal->min_pulse_us + (angle / angle_range) * pulse_range;
    
    // 应用中位偏移
    pulse += cal->offset_us;
    
    // 限制脉宽范围
    if (pulse < SERVO_MIN_PULSE_US) pulse = SERVO_MIN_PULSE_US;
    if (pulse > SERVO_MAX_PULSE_US) pulse = SERVO_MAX_PULSE_US;
    
    // 调试：打印角度转换过程（避免浮点printf问题）
    static uint32_t angle_debug_count = 0;
    angle_debug_count++;
    if (angle_debug_count % 10 == 0) {
        int angle_int = (int)(angle * 10);
        SERVO_DEBUG("[SERVO] Angle conversion: %d.%d° -> %dμs (range: %d-%d)\n", 
               angle_int / 10, angle_int % 10, (uint16_t)pulse, cal->min_pulse_us, cal->max_pulse_us);
    }
    
    return (uint16_t)pulse;
}

float servo_pulse_to_angle(uint8_t id, uint16_t pulse_us) {
    if (id >= SERVO_COUNT) {
        return 90.0f;
    }
    
    const servo_calibration_t *cal = &g_servos[id].calibration;
    
    // 去除中位偏移
    float pulse = pulse_us - cal->offset_us;
    
    // 线性映射：min_pulse-max_pulse -> 0-180度
    float pulse_range = cal->max_pulse_us - cal->min_pulse_us;
    float angle_range = SERVO_MAX_ANGLE - SERVO_MIN_ANGLE;
    float angle = (pulse - cal->min_pulse_us) / pulse_range * angle_range;
    
    // 方向反转
    if (cal->reverse) {
        angle = SERVO_MAX_ANGLE - angle;
    }
    
    // 限制角度范围
    if (angle < SERVO_MIN_ANGLE) angle = SERVO_MIN_ANGLE;
    if (angle > SERVO_MAX_ANGLE) angle = SERVO_MAX_ANGLE;
    
    return angle;
}

bool servo_check_angle_limit(uint8_t id, float angle) {
    if (id >= SERVO_COUNT) {
        return false;
    }
    
    // 检查角度范围
    if (angle < SERVO_MIN_ANGLE || angle > SERVO_MAX_ANGLE) {
        error_set(ERROR_SERVO_ANGLE);
        return false;
    }
    
    // 转换为脉宽并检查限位
    uint16_t pulse = servo_angle_to_pulse(id, angle);
    const servo_calibration_t *cal = &g_servos[id].calibration;
    
    if (pulse < cal->min_pulse_us || pulse > cal->max_pulse_us) {
        error_set(ERROR_SERVO_LIMIT);
        return false;
    }
    
    return true;
}

bool servo_set_angle(uint8_t id, float angle) {
    if (id >= SERVO_COUNT) {
        error_set(ERROR_CMD_ID);
        return false;
    }
    
    // 检查限位
    if (!servo_check_angle_limit(id, angle)) {
        return false;
    }
    
    // 转换为脉宽
    uint16_t pulse_us = servo_angle_to_pulse(id, angle);
    
    // 更新目标值
    g_servos[id].target_angle = angle;
    g_servos[id].target_pulse_us = pulse_us;
    
    // 直接更新当前值（插值在运动控制模块处理）
    g_servos[id].current_angle = angle;
    g_servos[id].current_pulse_us = pulse_us;
    
    // 更新PWM输出
    return pwm_set_pulse(id, pulse_us);
}

bool servo_set_all_angles(const float angles[SERVO_COUNT]) {
    uint16_t pulses[SERVO_COUNT];
    
    // 先检查所有角度是否有效
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (!servo_check_angle_limit(i, angles[i])) {
            return false;
        }
        pulses[i] = servo_angle_to_pulse(i, angles[i]);
    }
    
    // 批量更新
    for (int i = 0; i < SERVO_COUNT; i++) {
        g_servos[i].target_angle = angles[i];
        g_servos[i].target_pulse_us = pulses[i];
        g_servos[i].current_angle = angles[i];
        g_servos[i].current_pulse_us = pulses[i];
    }
    
    // 批量更新PWM（减少调试输出频率）
    static uint32_t servo_debug_count = 0;
    servo_debug_count++;
    if (servo_debug_count % 10 == 0) {
        SERVO_DEBUG("[SERVO] Setting all angles: ");
        for (int i = 0; i < SERVO_COUNT; i++) {
            int angle_int = (int)(angles[i] * 10);
            printf("%d.%d° ", angle_int / 10, angle_int % 10);
        }
        printf("\n");
    }
    
    return pwm_set_all_pulses(pulses);
}

bool servo_set_pulse(uint8_t id, uint16_t pulse_us) {
    if (id >= SERVO_COUNT) {
        error_set(ERROR_CMD_ID);
        return false;
    }
    
    // 限制脉宽范围
    if (pulse_us < SERVO_MIN_PULSE_US || pulse_us > SERVO_MAX_PULSE_US) {
        error_set(ERROR_SERVO_LIMIT);
        return false;
    }
    
    // 检查校准限位
    const servo_calibration_t *cal = &g_servos[id].calibration;
    if (pulse_us < cal->min_pulse_us || pulse_us > cal->max_pulse_us) {
        error_set(ERROR_SERVO_LIMIT);
        return false;
    }
    
    // 更新参数
    g_servos[id].current_pulse_us = pulse_us;
    g_servos[id].target_pulse_us = pulse_us;
    g_servos[id].current_angle = servo_pulse_to_angle(id, pulse_us);
    g_servos[id].target_angle = g_servos[id].current_angle;
    
    // 更新PWM
    return pwm_set_pulse(id, pulse_us);
}

void servo_enable(uint8_t id, bool enable) {
    if (id == 0xFF) {
        // 使能/禁用所有舵机
        for (int i = 0; i < SERVO_COUNT; i++) {
            g_servos[i].enabled = enable;
        }
        pwm_enable_all(enable);
    } else if (id < SERVO_COUNT) {
        // 单个舵机
        g_servos[id].enabled = enable;
        pwm_enable_channel(id, enable);
    }
}

float servo_get_angle(uint8_t id) {
    if (id >= SERVO_COUNT) {
        return 0.0f;
    }
    return g_servos[id].current_angle;
}

float servo_get_target_angle(uint8_t id) {
    if (id >= SERVO_COUNT) {
        return 0.0f;
    }
    return g_servos[id].target_angle;
}

uint16_t servo_get_pulse(uint8_t id) {
    if (id >= SERVO_COUNT) {
        return 0;
    }
    return g_servos[id].current_pulse_us;
}

bool servo_is_enabled(uint8_t id) {
    if (id >= SERVO_COUNT) {
        return false;
    }
    return g_servos[id].enabled;
}

bool servo_set_calibration(uint8_t id, const servo_calibration_t *calibration) {
    if (id >= SERVO_COUNT || calibration == NULL) {
        return false;
    }
    
    // 验证校准参数
    if (calibration->min_pulse_us < SERVO_MIN_PULSE_US ||
        calibration->max_pulse_us > SERVO_MAX_PULSE_US ||
        calibration->min_pulse_us >= calibration->max_pulse_us) {
        return false;
    }
    
    g_servos[id].calibration = *calibration;
    return true;
}

const servo_calibration_t* servo_get_calibration(uint8_t id) {
    if (id >= SERVO_COUNT) {
        return NULL;
    }
    return &g_servos[id].calibration;
}

const servo_t* servo_get_info(uint8_t id) {
    if (id >= SERVO_COUNT) {
        return NULL;
    }
    return &g_servos[id];
}

void servo_apply_default_calibration(uint8_t id) {
    if (id == 0xFF) {
        // 应用到所有舵机
        for (int i = 0; i < SERVO_COUNT; i++) {
            g_servos[i].calibration = g_default_calibration;
        }
    } else if (id < SERVO_COUNT) {
        g_servos[id].calibration = g_default_calibration;
    }
}


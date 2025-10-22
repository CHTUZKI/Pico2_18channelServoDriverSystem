/**
 * @file servo_360.c
 * @brief 360度连续旋转舵机控制实现
 * @date 2025-10-22
 */

#include "servo/servo_360.h"
#include "pwm/pwm_driver.h"
#include "utils/error_handler.h"
#include <string.h>
#include <math.h>
#include "FreeRTOS.h"
#include "task.h"

// ==================== 内部数据结构 ====================

// 360度舵机数组 (与标准舵机共用18个通道)
static servo_360_t g_servo_360[SERVO_COUNT];

// 360度舵机模式标识位图
static uint32_t g_servo_360_mode_bitmap = 0;

// 加减速曲线类型
static accel_curve_type_t g_curve_type[SERVO_COUNT];

// 默认校准参数
static const servo_360_calibration_t g_default_360_calib = {
    .neutral_pulse_us = SERVO_360_NEUTRAL_PULSE_US,
    .min_pulse_us = SERVO_360_MIN_PULSE_US,
    .max_pulse_us = SERVO_360_MAX_PULSE_US,
    .deadzone_us = SERVO_360_DEADZONE_US,
    .reverse = false,
    .calibrated = false
};

// ==================== 内部辅助函数 ====================

/**
 * @brief 应用加速度曲线
 */
static float apply_accel_curve(float t, accel_curve_type_t curve) {
    switch (curve) {
        case ACCEL_CURVE_LINEAR:
            return t;
        
        case ACCEL_CURVE_S_CURVE:
            // S曲线 (平滑)
            if (t < 0.5f) {
                return 2.0f * t * t;
            } else {
                return 1.0f - 2.0f * (1.0f - t) * (1.0f - t);
            }
        
        case ACCEL_CURVE_EXPONENTIAL:
            // 指数曲线
            return (expf(t) - 1.0f) / (expf(1.0f) - 1.0f);
        
        default:
            return t;
    }
}

/**
 * @brief 速度转换为脉宽 (带死区补偿)
 */
static uint16_t speed_to_pulse(uint8_t id, int8_t speed) {
    if (id >= SERVO_COUNT) {
        return SERVO_CENTER_PULSE_US;
    }
    
    const servo_360_calibration_t *calib = &g_servo_360[id].calib;
    
    // 限制速度范围
    if (speed > 100) speed = 100;
    if (speed < -100) speed = -100;
    
    // 最小速度阈值 (避免抖动)
    if (abs(speed) < SERVO_360_MIN_SPEED_THRESHOLD) {
        return calib->neutral_pulse_us;
    }
    
    // 方向反转
    if (calib->reverse) {
        speed = -speed;
    }
    
    // 计算基础脉宽
    int16_t pulse_offset = (speed * (SERVO_360_MAX_PULSE_US - SERVO_360_MIN_PULSE_US)) / 200;
    int16_t pulse = calib->neutral_pulse_us + pulse_offset;
    
    // 死区补偿
    if (speed > 0 && pulse < calib->neutral_pulse_us + calib->deadzone_us) {
        pulse = calib->neutral_pulse_us + calib->deadzone_us;
    } else if (speed < 0 && pulse > calib->neutral_pulse_us - calib->deadzone_us) {
        pulse = calib->neutral_pulse_us - calib->deadzone_us;
    }
    
    // PWM限幅
    if (pulse < calib->min_pulse_us) pulse = calib->min_pulse_us;
    if (pulse > calib->max_pulse_us) pulse = calib->max_pulse_us;
    
    return (uint16_t)pulse;
}

/**
 * @brief 检查是否需要方向切换延迟
 */
static bool needs_direction_change_delay(int8_t current, int8_t target) {
    return (current > 0 && target < 0) || (current < 0 && target > 0);
}

// ==================== 公共函数实现 ====================

bool servo_360_init(void) {
    // 初始化所有360度舵机参数
    for (int i = 0; i < SERVO_COUNT; i++) {
        g_servo_360[i].id = i;
        g_servo_360[i].calib = g_default_360_calib;
        g_servo_360[i].current_speed = 0;
        g_servo_360[i].target_speed = 0;
        g_servo_360[i].current_pulse_us = SERVO_360_NEUTRAL_PULSE_US;
        g_servo_360[i].accel_rate = SERVO_360_DEFAULT_ACCEL;
        g_servo_360[i].decel_rate = SERVO_360_DEFAULT_DECEL;
        g_servo_360[i].last_update_ms = 0;
        g_servo_360[i].enabled = false;
        g_servo_360[i].soft_stopping = false;
        g_servo_360[i].last_cmd_ms = 0;
        g_servo_360[i].estimated_position = 0.0f;
        g_servo_360[i].speed_to_dps = 0.0f;
        
        g_curve_type[i] = ACCEL_CURVE_LINEAR;
    }
    
    g_servo_360_mode_bitmap = 0;
    return true;
}

bool servo_360_enable_mode(uint8_t id) {
    if (id >= SERVO_COUNT) {
        return false;
    }
    
    // 设置为360度模式
    g_servo_360_mode_bitmap |= (1 << id);
    g_servo_360[id].enabled = true;
    
    return true;
}

bool servo_360_is_mode(uint8_t id) {
    if (id >= SERVO_COUNT) {
        return false;
    }
    return (g_servo_360_mode_bitmap & (1 << id)) != 0;
}

bool servo_360_set_speed(uint8_t id, int8_t speed) {
    if (id >= SERVO_COUNT || !servo_360_is_mode(id)) {
        error_set(ERROR_CMD_ID);
        return false;
    }
    
    servo_360_t *servo = &g_servo_360[id];
    
    // 检查是否需要方向切换延迟
    if (needs_direction_change_delay(servo->current_speed, speed) && 
        SERVO_360_DIRECTION_CHANGE_DELAY_MS > 0) {
        // 先减速到停止
        servo->target_speed = 0;
        servo_360_update(id);
        
        // 等待停止
        vTaskDelay(pdMS_TO_TICKS(SERVO_360_DIRECTION_CHANGE_DELAY_MS));
    }
    
    // 设置目标速度
    servo->target_speed = speed;
    servo->last_cmd_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
    servo->soft_stopping = false;
    
    return true;
}

bool servo_360_set_speeds(const uint8_t *ids, const int8_t *speeds, uint8_t count) {
    if (ids == NULL || speeds == NULL) {
        return false;
    }
    
    for (uint8_t i = 0; i < count; i++) {
        if (!servo_360_set_speed(ids[i], speeds[i])) {
            return false;
        }
    }
    
    return true;
}

bool servo_360_stop(uint8_t id) {
    if (id == 0xFF) {
        // 停止所有360度舵机
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_360_is_mode(i)) {
                g_servo_360[i].current_speed = 0;
                g_servo_360[i].target_speed = 0;
                g_servo_360[i].soft_stopping = false;
                uint16_t pulse = speed_to_pulse(i, 0);
                pwm_set_pulse(i, pulse);
            }
        }
        return true;
    }
    
    if (id >= SERVO_COUNT || !servo_360_is_mode(id)) {
        return false;
    }
    
    servo_360_t *servo = &g_servo_360[id];
    servo->current_speed = 0;
    servo->target_speed = 0;
    servo->soft_stopping = false;
    
    uint16_t pulse = speed_to_pulse(id, 0);
    return pwm_set_pulse(id, pulse);
}

bool servo_360_soft_stop(uint8_t id) {
    if (id == 0xFF) {
        // 软停止所有360度舵机
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_360_is_mode(i)) {
                g_servo_360[i].target_speed = 0;
                g_servo_360[i].soft_stopping = true;
            }
        }
        return true;
    }
    
    if (id >= SERVO_COUNT || !servo_360_is_mode(id)) {
        return false;
    }
    
    g_servo_360[id].target_speed = 0;
    g_servo_360[id].soft_stopping = true;
    
    return true;
}

void servo_360_set_acceleration(uint8_t id, uint8_t accel_rate) {
    if (id >= SERVO_COUNT) return;
    if (accel_rate < 1) accel_rate = 1;
    if (accel_rate > 100) accel_rate = 100;
    g_servo_360[id].accel_rate = accel_rate;
}

void servo_360_set_deceleration(uint8_t id, uint8_t decel_rate) {
    if (id >= SERVO_COUNT) return;
    if (decel_rate < 1) decel_rate = 1;
    if (decel_rate > 100) decel_rate = 100;
    g_servo_360[id].decel_rate = decel_rate;
}

void servo_360_set_curve_type(uint8_t id, accel_curve_type_t curve_type) {
    if (id >= SERVO_COUNT) return;
    g_curve_type[id] = curve_type;
}

bool servo_360_calibrate_neutral(uint8_t id, uint32_t timeout_ms) {
    if (id >= SERVO_COUNT || !servo_360_is_mode(id)) {
        return false;
    }
    
    // 二分查找最佳中点
    uint16_t low = SERVO_360_MIN_PULSE_US;
    uint16_t high = SERVO_360_MAX_PULSE_US;
    uint16_t best_neutral = SERVO_CENTER_PULSE_US;
    
    uint32_t start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    while ((high - low) > 10 && 
           (xTaskGetTickCount() * portTICK_PERIOD_MS - start_time) < timeout_ms) {
        uint16_t mid = (low + high) / 2;
        
        // 尝试这个脉宽
        pwm_set_pulse(id, mid);
        vTaskDelay(pdMS_TO_TICKS(100));  // 等待稳定
        
        // 这里需要外部反馈判断是否在转动
        // 简化实现：假设1500μs附近是中点
        if (mid < SERVO_CENTER_PULSE_US) {
            low = mid;
        } else {
            high = mid;
        }
        
        best_neutral = mid;
    }
    
    // 保存校准结果
    g_servo_360[id].calib.neutral_pulse_us = best_neutral;
    g_servo_360[id].calib.calibrated = true;
    
    return true;
}

bool servo_360_set_calibration(uint8_t id, const servo_360_calibration_t *calib) {
    if (id >= SERVO_COUNT || calib == NULL) {
        return false;
    }
    
    // 验证参数
    if (calib->min_pulse_us >= calib->max_pulse_us ||
        calib->neutral_pulse_us < calib->min_pulse_us ||
        calib->neutral_pulse_us > calib->max_pulse_us) {
        return false;
    }
    
    g_servo_360[id].calib = *calib;
    return true;
}

const servo_360_calibration_t* servo_360_get_calibration(uint8_t id) {
    if (id >= SERVO_COUNT) {
        return NULL;
    }
    return &g_servo_360[id].calib;
}

void servo_360_apply_default_calibration(uint8_t id) {
    if (id == 0xFF) {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_360_is_mode(i)) {
                g_servo_360[i].calib = g_default_360_calib;
            }
        }
    } else if (id < SERVO_COUNT) {
        g_servo_360[id].calib = g_default_360_calib;
    }
}

void servo_360_update(uint8_t id) {
    if (id >= SERVO_COUNT || !servo_360_is_mode(id)) {
        return;
    }
    
    servo_360_t *servo = &g_servo_360[id];
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // 超时保护
    if (SERVO_360_TIMEOUT_MS > 0 && 
        servo->last_cmd_ms > 0 &&
        (now - servo->last_cmd_ms) > SERVO_360_TIMEOUT_MS) {
        servo->target_speed = 0;
        servo->current_speed = 0;
        uint16_t pulse = speed_to_pulse(id, 0);
        pwm_set_pulse(id, pulse);
        return;
    }
    
    // 计算时间差
    uint32_t dt = now - servo->last_update_ms;
    if (dt < 20) return;  // 20ms更新周期
    
    servo->last_update_ms = now;
    
    // 软停止处理 (指数衰减)
    if (servo->soft_stopping) {
        servo->current_speed = (int8_t)(servo->current_speed * SERVO_360_SOFT_STOP_FACTOR);
        if (abs(servo->current_speed) < 2) {
            servo->current_speed = 0;
            servo->soft_stopping = false;
        }
    } else {
        // 加减速处理
        if (servo->current_speed != servo->target_speed) {
            int8_t delta;
            
            if (servo->current_speed < servo->target_speed) {
                // 加速
                delta = (servo->accel_rate * dt) / 1000;
                if (delta < 1) delta = 1;
                servo->current_speed += delta;
                if (servo->current_speed > servo->target_speed) {
                    servo->current_speed = servo->target_speed;
                }
            } else {
                // 减速
                delta = (servo->decel_rate * dt) / 1000;
                if (delta < 1) delta = 1;
                servo->current_speed -= delta;
                if (servo->current_speed < servo->target_speed) {
                    servo->current_speed = servo->target_speed;
                }
            }
        }
    }
    
    // 更新PWM
    uint16_t pulse = speed_to_pulse(id, servo->current_speed);
    servo->current_pulse_us = pulse;
    pwm_set_pulse(id, pulse);
    
    // 位置估算
    if (servo->speed_to_dps != 0.0f) {
        float dps = servo->current_speed * servo->speed_to_dps;
        float dt_sec = dt / 1000.0f;
        servo->estimated_position += dps * dt_sec;
        
        // 归一化到0-360度
        while (servo->estimated_position >= 360.0f) {
            servo->estimated_position -= 360.0f;
        }
        while (servo->estimated_position < 0.0f) {
            servo->estimated_position += 360.0f;
        }
    }
}

void servo_360_update_all(void) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (servo_360_is_mode(i)) {
            servo_360_update(i);
        }
    }
}

int8_t servo_360_get_speed(uint8_t id) {
    if (id >= SERVO_COUNT) return 0;
    return g_servo_360[id].current_speed;
}

int8_t servo_360_get_target_speed(uint8_t id) {
    if (id >= SERVO_COUNT) return 0;
    return g_servo_360[id].target_speed;
}

bool servo_360_is_enabled(uint8_t id) {
    if (id >= SERVO_COUNT) return false;
    return g_servo_360[id].enabled;
}

bool servo_360_is_moving(uint8_t id) {
    if (id >= SERVO_COUNT) return false;
    return g_servo_360[id].current_speed != 0;
}

const servo_360_t* servo_360_get_info(uint8_t id) {
    if (id >= SERVO_COUNT) return NULL;
    return &g_servo_360[id];
}

void servo_360_enable_position_estimate(uint8_t id, float speed_to_dps) {
    if (id >= SERVO_COUNT) return;
    g_servo_360[id].speed_to_dps = speed_to_dps;
}

float servo_360_get_estimated_position(uint8_t id) {
    if (id >= SERVO_COUNT) return 0.0f;
    return g_servo_360[id].estimated_position;
}

void servo_360_reset_position(uint8_t id, float position) {
    if (id >= SERVO_COUNT) return;
    g_servo_360[id].estimated_position = position;
}

bool servo_360_goto_position(uint8_t id, float target_degrees, float tolerance) {
    if (id >= SERVO_COUNT || !servo_360_is_mode(id)) {
        return false;
    }
    
    servo_360_t *servo = &g_servo_360[id];
    
    // 计算位置误差
    float error = target_degrees - servo->estimated_position;
    
    // 选择最短路径
    if (error > 180.0f) error -= 360.0f;
    if (error < -180.0f) error += 360.0f;
    
    // 检查是否到达
    if (fabsf(error) < tolerance) {
        servo_360_stop(id);
        return true;
    }
    
    // 简单P控制
    int8_t speed = (int8_t)(error * 0.5f);  // Kp = 0.5
    if (speed > 100) speed = 100;
    if (speed < -100) speed = -100;
    
    servo_360_set_speed(id, speed);
    return false;
}

void servo_360_enable(uint8_t id, bool enable) {
    if (id == 0xFF) {
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (servo_360_is_mode(i)) {
                g_servo_360[i].enabled = enable;
                if (!enable) {
                    servo_360_stop(i);
                }
            }
        }
    } else if (id < SERVO_COUNT && servo_360_is_mode(id)) {
        g_servo_360[id].enabled = enable;
        if (!enable) {
            servo_360_stop(id);
        }
    }
}


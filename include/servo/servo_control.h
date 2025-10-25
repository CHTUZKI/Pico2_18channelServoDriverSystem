/**
 * @file servo_control.h
 * @brief 舵机控制层接口
 * @date 2025-10-21
 */

#ifndef SERVO_CONTROL_H
#define SERVO_CONTROL_H

#include <stdint.h>
#include <stdbool.h>
#include "config/config.h"
#include "motion/interpolation.h"  // 用于梯形速度和轨迹规划

/**
 * @brief 舵机校准参数
 */
typedef struct {
    uint16_t min_pulse_us;      // 最小脉宽限位（μs）
    uint16_t max_pulse_us;      // 最大脉宽限位（μs）
    int16_t offset_us;          // 中位偏移（μs）
    bool reverse;               // 方向反转标志
} servo_calibration_t;

/**
 * @brief 舵机参数结构体
 */
typedef struct {
    uint8_t id;                     // 舵机ID
    servo_calibration_t calibration;// 校准参数
    float current_angle;            // 当前角度
    float target_angle;             // 目标角度
    uint16_t current_pulse_us;      // 当前脉宽
    uint16_t target_pulse_us;       // 目标脉宽
    bool enabled;                   // 使能标志
} servo_t;

/**
 * @brief 初始化舵机控制模块
 * @return true 成功, false 失败
 */
bool servo_control_init(void);

/**
 * @brief 设置舵机角度（单个）
 * @param id 舵机ID（0-17）
 * @param angle 目标角度（0-180度）
 * @return true 成功, false 失败
 */
bool servo_set_angle(uint8_t id, float angle);

/**
 * @brief 批量设置所有舵机角度
 * @param angles 角度数组（18个元素）
 * @return true 成功, false 失败
 */
bool servo_set_all_angles(const float angles[SERVO_COUNT]);

/**
 * @brief 设置舵机脉宽（直接控制）
 * @param id 舵机ID（0-17）
 * @param pulse_us 脉宽（500-2500μs）
 * @return true 成功, false 失败
 */
bool servo_set_pulse(uint8_t id, uint16_t pulse_us);

/**
 * @brief 角度转换为脉宽
 * @param id 舵机ID
 * @param angle 角度（0-180度）
 * @return 脉宽（μs）
 */
uint16_t servo_angle_to_pulse(uint8_t id, float angle);

/**
 * @brief 脉宽转换为角度
 * @param id 舵机ID
 * @param pulse_us 脉宽（μs）
 * @return 角度（度）
 */
float servo_pulse_to_angle(uint8_t id, uint16_t pulse_us);

/**
 * @brief 使能/禁用舵机
 * @param id 舵机ID（0-17，0xFF=全部）
 * @param enable true 使能, false 禁用
 */
void servo_enable(uint8_t id, bool enable);

/**
 * @brief 获取舵机当前角度
 * @param id 舵机ID
 * @return 当前角度
 */
float servo_get_angle(uint8_t id);

/**
 * @brief 获取舵机目标角度
 * @param id 舵机ID
 * @return 目标角度
 */
float servo_get_target_angle(uint8_t id);

/**
 * @brief 获取舵机当前脉宽
 * @param id 舵机ID
 * @return 当前脉宽（μs）
 */
uint16_t servo_get_pulse(uint8_t id);

/**
 * @brief 检查舵机是否使能
 * @param id 舵机ID
 * @return true 使能, false 禁用
 */
bool servo_is_enabled(uint8_t id);

/**
 * @brief 设置舵机校准参数
 * @param id 舵机ID
 * @param calibration 校准参数
 * @return true 成功, false 失败
 */
bool servo_set_calibration(uint8_t id, const servo_calibration_t *calibration);

/**
 * @brief 获取舵机校准参数
 * @param id 舵机ID
 * @return 校准参数指针，NULL表示无效ID
 */
const servo_calibration_t* servo_get_calibration(uint8_t id);

/**
 * @brief 获取舵机信息
 * @param id 舵机ID
 * @return 舵机信息指针，NULL表示无效ID
 */
const servo_t* servo_get_info(uint8_t id);

/**
 * @brief 检查角度是否在限位范围内
 * @param id 舵机ID
 * @param angle 角度
 * @return true 在范围内, false 超限
 */
bool servo_check_angle_limit(uint8_t id, float angle);

/**
 * @brief 应用默认校准参数
 * @param id 舵机ID（0-17，0xFF=全部）
 */
void servo_apply_default_calibration(uint8_t id);

/**
 * @brief 梯形速度运动
 * @param id 舵机ID
 * @param angle 目标角度
 * @param params 运动参数（速度、加速度）
 * @return true 成功, false 失败
 */
bool servo_move_trapezoid(uint8_t id, float angle, const motion_params_t *params);

/**
 * @brief 设置轨迹队列
 * @param id 舵机ID
 * @param trajectory 轨迹队列指针
 * @return true 成功, false 失败
 */
bool servo_set_trajectory(uint8_t id, trajectory_queue_t *trajectory);

#endif // SERVO_CONTROL_H


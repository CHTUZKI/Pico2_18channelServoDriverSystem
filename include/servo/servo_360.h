/**
 * @file servo_360.h
 * @brief 360度连续旋转舵机控制接口
 * @date 2025-10-22
 * 
 * 支持功能：
 * - 速度控制 (-100% 到 +100%)
 * - 加减速控制 (平滑启停)
 * - 死区补偿 (解决中点不转问题)
 * - 中点校准 (自适应每个舵机)
 * - 软停止 (指数衰减)
 * - 方向切换保护
 * - 超时安全保护
 */

#ifndef SERVO_360_H
#define SERVO_360_H

#include <stdint.h>
#include <stdbool.h>
#include "config/config.h"

/**
 * @brief 360度舵机校准参数
 */
typedef struct {
    uint16_t neutral_pulse_us;      // 实际停止点脉宽 (μs)
    uint16_t min_pulse_us;          // 最小脉宽 (满速反转)
    uint16_t max_pulse_us;          // 最大脉宽 (满速正转)
    uint16_t deadzone_us;           // 死区范围 (±μs)
    bool reverse;                   // 方向反转标志
    bool calibrated;                // 是否已校准
} servo_360_calibration_t;

/**
 * @brief 360度舵机运行状态
 */
typedef struct {
    uint8_t id;                     // 舵机ID
    servo_360_calibration_t calib;  // 校准参数
    
    // 速度控制
    int8_t current_speed;           // 当前速度 (-100 到 +100)
    int8_t target_speed;            // 目标速度
    uint16_t current_pulse_us;      // 当前脉宽
    
    // 加减速控制
    uint8_t accel_rate;             // 加速度 (%/s)
    uint8_t decel_rate;             // 减速度 (%/s)
    uint32_t last_update_ms;        // 上次更新时间
    
    // 状态标志
    bool enabled;                   // 使能标志
    bool soft_stopping;             // 软停止中
    uint32_t last_cmd_ms;           // 上次命令时间
    
    // 位置估算 (可选)
    float estimated_position;       // 估算角度 (度)
    float speed_to_dps;             // 速度→度/秒转换系数
} servo_360_t;

/**
 * @brief 加减速曲线类型
 */
typedef enum {
    ACCEL_CURVE_LINEAR,             // 线性加速
    ACCEL_CURVE_S_CURVE,            // S曲线加速
    ACCEL_CURVE_EXPONENTIAL         // 指数加速
} accel_curve_type_t;

// ==================== 初始化与配置 ====================

/**
 * @brief 初始化360度舵机控制模块
 * @return true 成功, false 失败
 */
bool servo_360_init(void);

/**
 * @brief 设置舵机为360度模式
 * @param id 舵机ID (0-17)
 * @return true 成功, false 失败
 */
bool servo_360_enable_mode(uint8_t id);

/**
 * @brief 检查舵机是否为360度模式
 * @param id 舵机ID
 * @return true 是360度模式, false 不是
 */
bool servo_360_is_mode(uint8_t id);

// ==================== 速度控制 ====================

/**
 * @brief 设置舵机速度
 * @param id 舵机ID (0-17)
 * @param speed 速度 (-100到+100, 0=停止)
 *              负值=反转, 正值=正转
 * @return true 成功, false 失败
 */
bool servo_360_set_speed(uint8_t id, int8_t speed);

/**
 * @brief 批量设置多个舵机速度
 * @param ids 舵机ID数组
 * @param speeds 速度数组
 * @param count 舵机数量
 * @return true 成功, false 失败
 */
bool servo_360_set_speeds(const uint8_t *ids, const int8_t *speeds, uint8_t count);

/**
 * @brief 停止舵机 (立即停止)
 * @param id 舵机ID (0-17, 0xFF=全部)
 * @return true 成功, false 失败
 */
bool servo_360_stop(uint8_t id);

/**
 * @brief 软停止舵机 (平滑减速到停止)
 * @param id 舵机ID (0-17, 0xFF=全部)
 * @return true 成功, false 失败
 */
bool servo_360_soft_stop(uint8_t id);

// ==================== 加减速配置 ====================

/**
 * @brief 设置加速度
 * @param id 舵机ID
 * @param accel_rate 加速度 (%/s, 1-100)
 */
void servo_360_set_acceleration(uint8_t id, uint8_t accel_rate);

/**
 * @brief 设置减速度
 * @param id 舵机ID
 * @param decel_rate 减速度 (%/s, 1-100)
 */
void servo_360_set_deceleration(uint8_t id, uint8_t decel_rate);

/**
 * @brief 设置加减速曲线类型
 * @param id 舵机ID
 * @param curve_type 曲线类型
 */
void servo_360_set_curve_type(uint8_t id, accel_curve_type_t curve_type);

// ==================== 校准功能 ====================

/**
 * @brief 自动校准舵机中点
 * @param id 舵机ID
 * @param timeout_ms 超时时间 (毫秒)
 * @return true 校准成功, false 失败
 */
bool servo_360_calibrate_neutral(uint8_t id, uint32_t timeout_ms);

/**
 * @brief 手动设置校准参数
 * @param id 舵机ID
 * @param calib 校准参数
 * @return true 成功, false 失败
 */
bool servo_360_set_calibration(uint8_t id, const servo_360_calibration_t *calib);

/**
 * @brief 获取校准参数
 * @param id 舵机ID
 * @return 校准参数指针, NULL表示失败
 */
const servo_360_calibration_t* servo_360_get_calibration(uint8_t id);

/**
 * @brief 应用默认校准参数
 * @param id 舵机ID (0-17, 0xFF=全部)
 */
void servo_360_apply_default_calibration(uint8_t id);

// ==================== 更新函数 ====================

/**
 * @brief 更新单个舵机状态 (加减速、超时检测等)
 * @param id 舵机ID
 * @note 应在FreeRTOS任务中定期调用 (建议20ms)
 */
void servo_360_update(uint8_t id);

/**
 * @brief 更新所有360度舵机状态
 * @note 应在FreeRTOS任务中定期调用 (建议20ms)
 */
void servo_360_update_all(void);

// ==================== 状态查询 ====================

/**
 * @brief 获取当前速度
 * @param id 舵机ID
 * @return 当前速度 (-100 到 +100)
 */
int8_t servo_360_get_speed(uint8_t id);

/**
 * @brief 获取目标速度
 * @param id 舵机ID
 * @return 目标速度
 */
int8_t servo_360_get_target_speed(uint8_t id);

/**
 * @brief 检查舵机是否使能
 * @param id 舵机ID
 * @return true 使能, false 禁用
 */
bool servo_360_is_enabled(uint8_t id);

/**
 * @brief 检查舵机是否正在运动
 * @param id 舵机ID
 * @return true 运动中, false 静止
 */
bool servo_360_is_moving(uint8_t id);

/**
 * @brief 获取舵机信息
 * @param id 舵机ID
 * @return 舵机信息指针, NULL表示失败
 */
const servo_360_t* servo_360_get_info(uint8_t id);

// ==================== 位置估算 (可选功能) ====================

/**
 * @brief 启用位置估算
 * @param id 舵机ID
 * @param speed_to_dps 速度→度/秒转换系数 (需实测)
 */
void servo_360_enable_position_estimate(uint8_t id, float speed_to_dps);

/**
 * @brief 获取估算位置
 * @param id 舵机ID
 * @return 估算角度 (0-360度)
 */
float servo_360_get_estimated_position(uint8_t id);

/**
 * @brief 重置估算位置
 * @param id 舵机ID
 * @param position 新位置 (度)
 */
void servo_360_reset_position(uint8_t id, float position);

/**
 * @brief 简单位置控制 (基于估算)
 * @param id 舵机ID
 * @param target_degrees 目标角度 (0-360度)
 * @param tolerance 容差 (度)
 * @return true 到达目标, false 未到达
 * @note 这是粗略控制，精度受估算误差影响
 */
bool servo_360_goto_position(uint8_t id, float target_degrees, float tolerance);

// ==================== 使能控制 ====================

/**
 * @brief 使能/禁用舵机
 * @param id 舵机ID (0-17, 0xFF=全部)
 * @param enable true 使能, false 禁用
 */
void servo_360_enable(uint8_t id, bool enable);

#endif // SERVO_360_H


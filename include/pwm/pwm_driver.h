/**
 * @file pwm_driver.h
 * @brief 18路PWM驱动（硬件PWM实现）
 * @date 2025-10-21
 * 
 * 实现方案：
 * - 使用RP2350的24个硬件PWM通道
 * - 18路全部使用硬件PWM（8个PWM切片 × 2通道 + 2个额外切片）
 * - 50Hz频率，500-2500μs脉宽范围，0.5μs分辨率
 */

#ifndef PWM_DRIVER_H
#define PWM_DRIVER_H

#include <stdint.h>
#include <stdbool.h>
#include "config/config.h"

/**
 * @brief PWM通道结构体
 */
typedef struct {
    uint8_t gpio;               // GPIO引脚
    uint16_t pulse_us;          // 当前脉宽（μs）
    bool enabled;               // 使能标志
    uint8_t slice;              // PWM切片号
    uint8_t channel;            // PWM通道（A=0, B=1）
} pwm_channel_t;

/**
 * @brief 初始化18路PWM系统
 * @return true 成功, false 失败
 */
bool pwm_init_all(void);

/**
 * @brief 设置指定通道的脉宽
 * @param channel 通道号（0-17）
 * @param pulse_us 脉宽（μs，范围500-2500）
 * @return true 成功, false 失败
 */
bool pwm_set_pulse(uint8_t channel, uint16_t pulse_us);

/**
 * @brief 批量设置所有通道的脉宽
 * @param pulse_us_array 脉宽数组（18个元素）
 * @return true 成功, false 失败
 */
bool pwm_set_all_pulses(const uint16_t pulse_us_array[SERVO_COUNT]);

/**
 * @brief 使能指定通道
 * @param channel 通道号（0-17）
 * @param enable true 使能, false 禁用
 */
void pwm_enable_channel(uint8_t channel, bool enable);

/**
 * @brief 使能所有通道
 * @param enable true 使能, false 禁用
 */
void pwm_enable_all(bool enable);

/**
 * @brief 获取指定通道的当前脉宽
 * @param channel 通道号（0-17）
 * @return 当前脉宽（μs）
 */
uint16_t pwm_get_pulse(uint8_t channel);

/**
 * @brief 检查通道是否使能
 * @param channel 通道号（0-17）
 * @return true 使能, false 禁用
 */
bool pwm_is_enabled(uint8_t channel);

/**
 * @brief 紧急停止所有PWM输出
 */
void pwm_emergency_stop(void);

/**
 * @brief 获取PWM通道信息
 * @param channel 通道号（0-17）
 * @return 通道信息指针，NULL表示无效通道
 */
const pwm_channel_t* pwm_get_channel_info(uint8_t channel);

#endif // PWM_DRIVER_H


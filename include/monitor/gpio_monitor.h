/**
 * @file gpio_monitor.h
 * @brief GPIO监控器头文件
 * @author AI Assistant
 * @date 2025-01-23
 */

#ifndef GPIO_MONITOR_H
#define GPIO_MONITOR_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 启动GPIO监控器（使用Core1）
 * 
 * 监控所有舵机PWM引脚（GPIO0-17）的状态变化
 * 通过GPIO中断捕获上升沿和下降沿，计算脉宽
 */
void gpio_monitor_start(void);

/**
 * @brief 停止GPIO监控器
 */
void gpio_monitor_stop(void);

/**
 * @brief 获取指定引脚的脉宽（微秒）
 * @param pin GPIO引脚号
 * @return 脉宽（微秒），0表示无有效脉冲
 */
uint32_t gpio_monitor_get_pulse_width(uint8_t pin);

/**
 * @brief 获取指定引脚的脉冲计数
 * @param pin GPIO引脚号
 * @return 脉冲计数
 */
uint32_t gpio_monitor_get_pulse_count(uint8_t pin);

#endif // GPIO_MONITOR_H

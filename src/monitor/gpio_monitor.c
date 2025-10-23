/**
 * @file gpio_monitor.c
 * @brief GPIO监控器 - 使用Core1监控PWM输出变化
 * @author AI Assistant
 * @date 2025-01-23
 */

#include "monitor/gpio_monitor.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/irq.h"
#include "hardware/timer.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include <stdio.h>

// 监控的GPIO引脚列表（舵机PWM引脚）
static const uint8_t MONITOR_PINS[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17
};
#define MONITOR_PIN_COUNT (sizeof(MONITOR_PINS) / sizeof(MONITOR_PINS[0]))

// 每个引脚的状态记录
typedef struct {
    bool last_state;
    uint32_t last_rise_time;
    uint32_t last_fall_time;
    uint32_t pulse_width_us;
    uint32_t pulse_count;
    bool has_pulse;
} pin_state_t;

static pin_state_t pin_states[MONITOR_PIN_COUNT];
static volatile bool monitor_enabled = false;
static volatile uint32_t monitor_count = 0;

// GPIO中断处理函数
void gpio_monitor_irq_handler(uint gpio, uint32_t events) {
    if (!monitor_enabled) return;
    
    // 查找是哪个监控引脚
    int pin_index = -1;
    for (int i = 0; i < MONITOR_PIN_COUNT; i++) {
        if (MONITOR_PINS[i] == gpio) {
            pin_index = i;
            break;
        }
    }
    
    if (pin_index == -1) return;
    
    pin_state_t *state = &pin_states[pin_index];
    uint32_t current_time = time_us_32();
    
    if (events & GPIO_IRQ_EDGE_RISE) {
        // 上升沿
        state->last_rise_time = current_time;
        state->last_state = true;
        
        // 如果之前是低电平，计算脉冲宽度
        if (state->last_fall_time > 0) {
            uint32_t pulse_width = state->last_rise_time - state->last_fall_time;
            if (pulse_width > 500 && pulse_width < 2500) {  // 0.5ms - 2.5ms 有效脉宽范围
                state->pulse_width_us = pulse_width;
                state->pulse_count++;
                state->has_pulse = true;
            }
        }
    } else if (events & GPIO_IRQ_EDGE_FALL) {
        // 下降沿
        state->last_fall_time = current_time;
        state->last_state = false;
    }
}

// Core1监控主循环 - 直接读取PWM寄存器
void gpio_monitor_core1_main() {
    printf("[MONITOR] Core1 PWM Monitor started\n");
    printf("[MONITOR] Monitoring PWM slices for GPIO0-17\n");
    
    monitor_enabled = true;
    
    uint32_t last_report_time = 0;
    const uint32_t REPORT_INTERVAL_MS = 2000;  // 2秒报告一次
    
    while (true) {
        uint32_t current_time = time_us_32();
        
        // 定期报告PWM状态
        if (current_time - last_report_time > REPORT_INTERVAL_MS * 1000) {
            printf("\n=== PWM Report #%lu ===\n", monitor_count++);
            
            // 读取PWM寄存器
            for (int i = 0; i < MONITOR_PIN_COUNT; i++) {
                uint gpio = MONITOR_PINS[i];
                uint slice = pwm_gpio_to_slice_num(gpio);
                uint chan = pwm_gpio_to_channel(gpio);
                
                // 读取PWM配置
                uint32_t cc_reg = pwm_hw->slice[slice].cc;
                uint16_t level = (chan == PWM_CHAN_A) ? (cc_reg & 0xFFFF) : (cc_reg >> 16);
                uint16_t wrap = pwm_hw->slice[slice].top;
                bool enabled = (pwm_hw->slice[slice].csr & PWM_CH0_CSR_EN_BITS) != 0;
                
                // 计算脉宽（微秒）
                // PWM频率 = 2MHz, 周期 = 0.5us
                // wrap = 39999 (20ms周期)
                // pulse_width = level * 0.5us
                uint32_t pulse_us = (level + 1) / 2;  // level * 0.5
                
                printf("GPIO%02d S%dC%c: L=%5d W=%5d PW=%4luus %s\n",
                       gpio, slice, chan ? 'B' : 'A',
                       level, wrap, pulse_us,
                       enabled ? "EN" : "DIS");
            }
            
            last_report_time = current_time;
        }
        
        // 短暂休眠
        sleep_ms(100);
    }
}

// 启动GPIO监控器（不配置GPIO，只读取PWM寄存器）
void gpio_monitor_start() {
    printf("[MONITOR] Starting PWM monitor on Core1...\n");
    
    // 不配置GPIO（PWM已经配置好了）
    // 直接启动Core1监控循环
    multicore_launch_core1(gpio_monitor_core1_main);
    
    printf("[MONITOR] Core1 PWM monitor started\n");
}

// 停止监控
void gpio_monitor_stop() {
    monitor_enabled = false;
    printf("[MONITOR] 监控器已停止\n");
}

// 获取指定引脚的脉宽
uint32_t gpio_monitor_get_pulse_width(uint8_t pin) {
    for (int i = 0; i < MONITOR_PIN_COUNT; i++) {
        if (MONITOR_PINS[i] == pin) {
            return pin_states[i].pulse_width_us;
        }
    }
    return 0;
}

// 获取指定引脚的脉冲计数
uint32_t gpio_monitor_get_pulse_count(uint8_t pin) {
    for (int i = 0; i < MONITOR_PIN_COUNT; i++) {
        if (MONITOR_PINS[i] == pin) {
            return pin_states[i].pulse_count;
        }
    }
    return 0;
}

/**
 * @file pwm_driver.c
 * @brief 18路PWM驱动实现（硬件PWM）
 * @date 2025-10-21
 */

#include "pwm/pwm_driver.h"
#include "config/pinout.h"
#include "config/config.h"
#include "utils/error_handler.h"
#include "utils/usb_bridge.h"
#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include <string.h>
#include <stdio.h>

// ==================== 调试宏 ====================
// 使用USB Bridge避免Core 0直接访问USB
#if DEBUG_PWM
    #define PWM_DEBUG(...) usb_bridge_printf(__VA_ARGS__)
#else
    #define PWM_DEBUG(...) ((void)0)
#endif

// PWM通道配置表
static pwm_channel_t g_pwm_channels[SERVO_COUNT];

// 硬件PWM配置
#define HW_PWM_WRAP     40000   // PWM计数器最大值 (对应20ms @ 2MHz时钟)
#define HW_PWM_FREQ     2000000 // PWM时钟频率 2MHz (0.5μs分辨率)

/**
 * @brief 初始化硬件PWM通道
 * @param gpio GPIO引脚
 * @param channel 通道配置
 * @return true 成功
 */
static bool init_hardware_pwm(uint8_t gpio, pwm_channel_t *channel) {
    // 获取PWM切片和通道
    uint slice_num = pwm_gpio_to_slice_num(gpio);
    uint chan = pwm_gpio_to_channel(gpio);
    
    PWM_DEBUG("[PWM] Init GPIO%d -> Slice%d Chan%c\n", gpio, slice_num, chan ? 'B' : 'A');
    
    // 配置GPIO为PWM功能
    gpio_set_function(gpio, GPIO_FUNC_PWM);
    
    // 获取PWM配置
    pwm_config config = pwm_get_default_config();
    
    // 计算分频器: 系统时钟 / 分频器 = 2MHz
    // 假设系统时钟125MHz: 125MHz / 62.5 = 2MHz
    uint32_t sys_clk = clock_get_hz(clk_sys);
    float div = (float)sys_clk / HW_PWM_FREQ;
    pwm_config_set_clkdiv(&config, div);
    
    // 设置PWM周期 (20ms = 40000 个 0.5μs)
    pwm_config_set_wrap(&config, HW_PWM_WRAP - 1);
    
    // 调试：显示PWM配置
    if (gpio == 0) {
        PWM_DEBUG("[PWM] GPIO0 Config: SysClk=%luHz, Div=%.2f, Wrap=%d\n", 
                 sys_clk, div, HW_PWM_WRAP - 1);
    }
    
    // 应用配置
    pwm_init(slice_num, &config, false);  // 先不启动
    
    // 设置初始占空比为0
    pwm_set_chan_level(slice_num, chan, 0);
    
    // 保存配置信息
    channel->slice = slice_num;
    channel->channel = chan;
    
    return true;
}

bool pwm_init_all(void) {
    // 初始化所有通道配置
    for (int i = 0; i < SERVO_COUNT; i++) {
        g_pwm_channels[i].gpio = SERVO_PINS[i];
        g_pwm_channels[i].pulse_us = SERVO_CENTER_PULSE_US;
        g_pwm_channels[i].enabled = false;
        
        // 全部使用硬件PWM
        if (!init_hardware_pwm(SERVO_PINS[i], &g_pwm_channels[i])) {
            error_set(ERROR_SYSTEM_INIT);
            return false;
        }
    }
    
    // 启动所有PWM切片（统一启动，保证同步）
    for (int slice = 0; slice < 8; slice++) {
        pwm_set_enabled(slice, true);
        PWM_DEBUG("[PWM] Slice %d enabled\n", slice);
    }
    
    PWM_DEBUG("[PWM] All PWM slices initialized and enabled\n");
    
    // 测试：GPIO0输出明显的PWM信号 (10% duty cycle)
    pwm_set_chan_level(0, 0, 4000);  // 10% of 40000
    PWM_DEBUG("[PWM] Test: GPIO0 set to 10%% duty cycle (level=4000)\n");
    
    return true;
}

bool pwm_set_pulse(uint8_t channel, uint16_t pulse_us) {
    if (channel >= SERVO_COUNT) {
        return false;
    }
    
    // 限制脉宽范围
    if (pulse_us < SERVO_MIN_PULSE_US) {
        pulse_us = SERVO_MIN_PULSE_US;
    }
    if (pulse_us > SERVO_MAX_PULSE_US) {
        pulse_us = SERVO_MAX_PULSE_US;
    }
    
    g_pwm_channels[channel].pulse_us = pulse_us;
    
    // 转换μs到PWM计数值
    // pulse_us * 2 因为时钟是0.5μs分辨率
    uint16_t level = pulse_us * 2;
    
    uint slice = g_pwm_channels[channel].slice;
    uint chan = g_pwm_channels[channel].channel;
    
    #if DEBUG_PWM
    // 【调试】打印前3个通道的详细信息
    if (channel <= 2) {
        usb_bridge_printf("[PWM] set_pulse: ch=%d, GPIO=%d, pulse=%d, slice=%d, chan=%c, enabled=%d\n",
                          channel, g_pwm_channels[channel].gpio, pulse_us, slice, chan ? 'B' : 'A',
                          g_pwm_channels[channel].enabled);
    }
    #endif
    
    if (g_pwm_channels[channel].enabled) {
        pwm_set_chan_level(slice, chan, level);
        
        // Channel 0 总是输出详细调试信息
        static uint32_t pwm_debug_count = 0;
        pwm_debug_count++;
        if (channel == 0) {
            // 验证PWM slice是否真的使能
            bool slice_enabled = pwm_is_enabled(slice);
            PWM_DEBUG("[PWM] Ch%d: GPIO%d pulse=%dus, level=%d, slice%d=%s, CH_EN=1\n", 
                     channel, g_pwm_channels[channel].gpio, pulse_us, level, slice, 
                     slice_enabled ? "EN" : "DIS");
        } else if (pwm_debug_count % 20 == 0) {
            PWM_DEBUG("[PWM] Channel %d: pulse=%dus, level=%d, enabled=1\n", channel, pulse_us, level);
        }
    } else {
        pwm_set_chan_level(slice, chan, 0);
        // Channel 0 总是输出调试信息，其他通道减少频率
        static uint32_t pwm_debug_count_disabled = 0;
        pwm_debug_count_disabled++;
        if (channel == 0 || pwm_debug_count_disabled % 20 == 0) {
            PWM_DEBUG("[PWM] Channel %d: pulse=%dus, level=0 (SET TO ZERO), CH_EN=0\n", channel, pulse_us);
        }
    }
    
    return true;
}

bool pwm_set_all_pulses(const uint16_t pulse_us_array[SERVO_COUNT]) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (!pwm_set_pulse(i, pulse_us_array[i])) {
            return false;
        }
    }
    return true;
}

void pwm_enable_channel(uint8_t channel, bool enable) {
    if (channel >= SERVO_COUNT) {
        return;
    }
    
    g_pwm_channels[channel].enabled = enable;
    
    // 更新PWM输出
    uint slice = g_pwm_channels[channel].slice;
    uint chan = g_pwm_channels[channel].channel;
    
    // 确保PWM Slice已使能
    if (!pwm_is_enabled(slice)) {
        pwm_set_enabled(slice, true);
        PWM_DEBUG("[PWM] Slice %d was disabled, re-enabling...\n", slice);
    }
    
    if (enable) {
        // 使能：输出当前脉宽
        uint16_t level = g_pwm_channels[channel].pulse_us * 2;
        pwm_set_chan_level(slice, chan, level);
        
        // Channel 0 总是输出调试信息
        if (channel == 0) {
            PWM_DEBUG("[PWM] Enable Ch%d: GPIO%d, pulse=%dus, level=%d, slice%d\n", 
                     channel, g_pwm_channels[channel].gpio, 
                     g_pwm_channels[channel].pulse_us, level, slice);
        }
    } else {
        // 禁用：输出0
        pwm_set_chan_level(slice, chan, 0);
        
        if (channel == 0) {
            PWM_DEBUG("[PWM] Disable Ch%d: GPIO%d, level=0\n", 
                     channel, g_pwm_channels[channel].gpio);
        }
    }
}

void pwm_enable_all(bool enable) {
    for (int i = 0; i < SERVO_COUNT; i++) {
        pwm_enable_channel(i, enable);
    }
}

uint16_t pwm_get_pulse(uint8_t channel) {
    if (channel >= SERVO_COUNT) {
        return 0;
    }
    return g_pwm_channels[channel].pulse_us;
}

bool pwm_is_enabled(uint8_t channel) {
    if (channel >= SERVO_COUNT) {
        return false;
    }
    return g_pwm_channels[channel].enabled;
}

void pwm_emergency_stop(void) {
    // 立即禁用所有PWM输出
    for (int i = 0; i < SERVO_COUNT; i++) {
        uint slice = g_pwm_channels[i].slice;
        uint chan = g_pwm_channels[i].channel;
        pwm_set_chan_level(slice, chan, 0);
        g_pwm_channels[i].enabled = false;
    }
}

const pwm_channel_t* pwm_get_channel_info(uint8_t channel) {
    if (channel >= SERVO_COUNT) {
        return NULL;
    }
    return &g_pwm_channels[channel];
}


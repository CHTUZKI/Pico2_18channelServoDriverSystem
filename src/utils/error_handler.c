/**
 * @file error_handler.c
 * @brief 错误处理和状态管理实现
 * @date 2025-10-21
 */

#include "utils/error_handler.h"
#include "config/config.h"
#include "config/pinout.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

// 错误状态全局变量
static error_status_t g_error_status;

// LED闪烁控制
static uint32_t g_led_last_toggle = 0;
static bool g_led_state = false;

void error_handler_init(void) {
    // 清零错误状态
    memset(&g_error_status, 0, sizeof(error_status_t));
    g_error_status.state = SYSTEM_STATE_IDLE;
    
    // 初始化LED引脚
    gpio_init(PIN_LED_BUILTIN);
    gpio_set_dir(PIN_LED_BUILTIN, GPIO_OUT);
    gpio_put(PIN_LED_BUILTIN, 0);
}

void error_set(error_code_t error) {
    g_error_status.last_error = error;
    g_error_status.error_count++;
    
    // 统计特定错误类型
    if (error >= ERROR_COMM_TIMEOUT && error <= ERROR_COMM_OVERFLOW) {
        g_error_status.comm_error_count++;
    }
    if (error == ERROR_COMM_CRC) {
        g_error_status.crc_error_count++;
    }
    
    // 严重错误时进入错误状态
    if (error == ERROR_EMERGENCY_STOP || 
        error == ERROR_SYSTEM_INIT || 
        error == ERROR_SYSTEM_MEMORY) {
        g_error_status.state = SYSTEM_STATE_ERROR;
    }
}

void error_clear(void) {
    g_error_status.last_error = ERROR_NONE;
    if (g_error_status.state == SYSTEM_STATE_ERROR) {
        g_error_status.state = SYSTEM_STATE_IDLE;
    }
}

error_code_t error_get_last(void) {
    return g_error_status.last_error;
}

const error_status_t* error_get_status(void) {
    return &g_error_status;
}

void system_set_state(system_state_t state) {
    g_error_status.state = state;
}

system_state_t system_get_state(void) {
    return g_error_status.state;
}

void emergency_stop_trigger(void) {
    g_error_status.emergency_stop = true;
    g_error_status.state = SYSTEM_STATE_EMERGENCY_STOP;
    error_set(ERROR_EMERGENCY_STOP);
}

void emergency_stop_clear(void) {
    g_error_status.emergency_stop = false;
    if (g_error_status.state == SYSTEM_STATE_EMERGENCY_STOP) {
        g_error_status.state = SYSTEM_STATE_IDLE;
    }
    error_clear();
}

bool is_emergency_stopped(void) {
    return g_error_status.emergency_stop;
}

void error_led_update(void) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    uint32_t blink_period;
    
    // 根据状态选择闪烁周期
    switch (g_error_status.state) {
        case SYSTEM_STATE_ERROR:
        case SYSTEM_STATE_EMERGENCY_STOP:
            blink_period = LED_BLINK_ERROR_MS;
            break;
            
        case SYSTEM_STATE_MOVING:
            blink_period = LED_BLINK_COMM_MS;
            break;
            
        case SYSTEM_STATE_RUNNING:
        case SYSTEM_STATE_IDLE:
        default:
            blink_period = LED_BLINK_NORMAL_MS;
            break;
    }
    
    // 翻转LED
    if (now - g_led_last_toggle >= blink_period / 2) {
        g_led_state = !g_led_state;
        gpio_put(PIN_LED_BUILTIN, g_led_state);
        g_led_last_toggle = now;
    }
}

// vAssertCalled函数已不再需要（QP/C使用Q_onError替代）


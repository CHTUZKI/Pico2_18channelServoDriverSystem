/**
 * @file param_manager.c
 * @brief 参数管理器实现
 * @date 2025-10-21
 */

#include "storage/param_manager.h"
#include "storage/flash_storage.h"
#include "servo/servo_control.h"
#include "utils/error_handler.h"
#include "config/config.h"
#include <string.h>

// 当前参数缓存
static flash_params_t g_current_params;

bool param_manager_init(void) {
    // 尝试从Flash加载参数
    if (flash_load_params(&g_current_params)) {
        // 加载成功，应用到舵机
        return param_manager_apply_to_servos();
    } else {
        // 加载失败，使用默认参数
        return param_manager_factory_reset();
    }
}

bool param_manager_save(void) {
    // 从舵机读取当前校准参数
    for (int i = 0; i < SERVO_COUNT; i++) {
        const servo_calibration_t *cal = servo_get_calibration(i);
        if (cal != NULL) {
            g_current_params.calibrations[i] = *cal;
        }
    }
    
    // 保存到Flash
    return flash_save_params(&g_current_params);
}

bool param_manager_load(void) {
    // 从Flash加载
    if (!flash_load_params(&g_current_params)) {
        return false;
    }
    
    // 应用到舵机
    return param_manager_apply_to_servos();
}

bool param_manager_factory_reset(void) {
    // 清空参数结构
    memset(&g_current_params, 0, sizeof(flash_params_t));
    
    // 设置默认校准参数
    servo_calibration_t default_cal = {
        .min_pulse_us = SERVO_MIN_PULSE_US,
        .max_pulse_us = SERVO_MAX_PULSE_US,
        .offset_us = 0,
        .reverse = false
    };
    
    for (int i = 0; i < SERVO_COUNT; i++) {
        g_current_params.calibrations[i] = default_cal;
    }
    
    // 应用到舵机
    if (!param_manager_apply_to_servos()) {
        return false;
    }
    
    // 保存到Flash
    return flash_save_params(&g_current_params);
}

bool param_manager_apply_to_servos(void) {
    // 将参数应用到所有舵机
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (!servo_set_calibration(i, &g_current_params.calibrations[i])) {
            return false;
        }
    }
    
    return true;
}


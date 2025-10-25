/**
 * @file param_manager.c
 * @brief 参数管理器实现
 * @date 2025-10-21
 */

#include "storage/param_manager.h"
#include "storage/flash_storage.h"
#include "servo/servo_control.h"
#include "utils/error_handler.h"
#include "utils/usb_bridge.h"
#include "config/config.h"
#include <string.h>
#include <stdio.h>

// 调试宏 - 使用USB Bridge避免Core 0直接访问USB
#if DEBUG_FLASH
    #define FLASH_DEBUG(fmt, ...) usb_bridge_printf("[FLASH] " fmt, ##__VA_ARGS__)
#else
    #define FLASH_DEBUG(fmt, ...)
#endif

// 当前参数缓存
static flash_params_t g_current_params;

bool param_manager_init(void) {
    FLASH_DEBUG("=== Param Manager Init ===\n");
    
    // 尝试从Flash加载参数
    if (flash_load_params(&g_current_params)) {
        FLASH_DEBUG("Flash loaded successfully\n");
        FLASH_DEBUG("Magic: 0x%08X\n", g_current_params.magic);
        FLASH_DEBUG("Version: %d\n", g_current_params.version);
        FLASH_DEBUG("Servo count: %d\n", g_current_params.servo_count);
        FLASH_DEBUG("Checksum: 0x%04X\n", g_current_params.checksum);
        
        // 打印校准参数
        FLASH_DEBUG("\n--- Calibration Params ---\n");
        for (int i = 0; i < SERVO_COUNT; i++) {
            FLASH_DEBUG("Servo%d: Pulse[%d-%d]us, Offset=%d, Reverse=%d\n",
                i,
                g_current_params.calibrations[i].min_pulse_us,
                g_current_params.calibrations[i].max_pulse_us,
                g_current_params.calibrations[i].offset_us,
                g_current_params.calibrations[i].reverse
            );
        }
        
        // 打印保存的位置
        FLASH_DEBUG("\n--- Saved Positions (Valid: %d) ---\n", g_current_params.positions_valid);
        if (g_current_params.positions_valid) {
            for (int i = 0; i < SERVO_COUNT; i++) {
                int angle_int = (int)(g_current_params.saved_positions[i] * 10);
                FLASH_DEBUG("Servo%d: %d.%d deg\n", i, angle_int / 10, angle_int % 10);
            }
        } else {
            FLASH_DEBUG("No valid position data\n");
        }
        
        FLASH_DEBUG("==========================\n\n");
        
        // 加载成功，应用到舵机
        return param_manager_apply_to_servos();
    } else {
        FLASH_DEBUG("Flash load failed, using defaults\n");
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
    FLASH_DEBUG("=== Factory Reset (First Boot) ===\n");
    
    // 清空参数结构
    memset(&g_current_params, 0, sizeof(flash_params_t));
    
    // 设置默认校准参数
    servo_calibration_t default_cal = {
        .min_pulse_us = SERVO_MIN_PULSE_US,
        .max_pulse_us = SERVO_MAX_PULSE_US,
        .offset_us = 0,
        .reverse = false
    };
    
    FLASH_DEBUG("Default calibration: Pulse[%d-%d]us, Offset=0, Reverse=0\n",
        SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
    
    for (int i = 0; i < SERVO_COUNT; i++) {
        g_current_params.calibrations[i] = default_cal;
    }
    
    // 应用到舵机
    if (!param_manager_apply_to_servos()) {
        FLASH_DEBUG("Apply to servos: FAIL\n");
        FLASH_DEBUG("==================================\n\n");
        return false;
    }
    FLASH_DEBUG("Apply to servos: OK\n");
    
    // 保存到Flash
    bool result = flash_save_params(&g_current_params);
    FLASH_DEBUG("Save to Flash: %s\n", result ? "OK" : "FAIL");
    FLASH_DEBUG("==================================\n\n");
    
    return result;
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

bool param_manager_save_positions(void) {
    FLASH_DEBUG("=== Save Positions to Flash ===\n");
    
    // 读取当前所有舵机的位置
    for (int i = 0; i < SERVO_COUNT; i++) {
        g_current_params.saved_positions[i] = servo_get_angle(i);
        int angle_int = (int)(g_current_params.saved_positions[i] * 10);
        FLASH_DEBUG("Servo%d: %d.%d deg\n", i, angle_int / 10, angle_int % 10);
    }
    
    // 标记位置数据有效
    g_current_params.positions_valid = true;
    
    // 保存到Flash
    bool result = flash_save_params(&g_current_params);
    FLASH_DEBUG("Save result: %s\n", result ? "OK" : "FAIL");
    FLASH_DEBUG("===============================\n\n");
    
    return result;
}

bool param_manager_load_positions(void) {
    FLASH_DEBUG("=== Load Positions from Flash ===\n");
    
    // 检查位置数据是否有效
    if (!g_current_params.positions_valid) {
        FLASH_DEBUG("Position data invalid\n");
        FLASH_DEBUG("=================================\n\n");
        return false;
    }
    
    FLASH_DEBUG("Position data valid, applying to servos:\n");
    for (int i = 0; i < SERVO_COUNT; i++) {
        int angle_int = (int)(g_current_params.saved_positions[i] * 10);
        FLASH_DEBUG("Servo%d: %d.%d deg\n", i, angle_int / 10, angle_int % 10);
    }
    
    // 应用位置到舵机
    bool result = servo_set_all_angles(g_current_params.saved_positions);
    FLASH_DEBUG("Apply result: %s\n", result ? "OK" : "FAIL");
    FLASH_DEBUG("=================================\n\n");
    
    return result;
}

bool param_manager_has_saved_positions(void) {
    // 尝试从Flash加载参数
    flash_params_t temp_params;
    if (flash_load_params(&temp_params)) {
        return temp_params.positions_valid;
    }
    return false;
}

bool param_manager_reset(void) {
    FLASH_DEBUG("=== Factory Reset ===\n");
    
    // 恢复出厂设置：清除所有参数，恢复默认值
    
    // 1. 恢复默认校准参数
    for (int i = 0; i < SERVO_COUNT; i++) {
        g_current_params.calibrations[i].min_pulse_us = SERVO_MIN_PULSE_US;
        g_current_params.calibrations[i].max_pulse_us = SERVO_MAX_PULSE_US;
        g_current_params.calibrations[i].offset_us = 0;
        g_current_params.calibrations[i].reverse = false;
        
        // 清除保存的位置
        g_current_params.saved_positions[i] = 90.0f;
    }
    
    // 2. 标记位置数据无效
    g_current_params.positions_valid = false;
    FLASH_DEBUG("Default calibration: Pulse[%d-%d]us, Offset=0, Reverse=0\n",
        SERVO_MIN_PULSE_US, SERVO_MAX_PULSE_US);
    FLASH_DEBUG("Default position: 90.0 deg\n");
    FLASH_DEBUG("Position data marked invalid\n");
    
    // 3. 保存到Flash
    bool flash_ok = flash_save_params(&g_current_params);
    FLASH_DEBUG("Flash save: %s\n", flash_ok ? "OK" : "FAIL");
    
    // 4. 应用默认校准到舵机
    bool apply_ok = param_manager_apply_to_servos();
    FLASH_DEBUG("Params apply: %s\n", apply_ok ? "OK" : "FAIL");
    FLASH_DEBUG("=====================\n\n");
    
    return flash_ok && apply_ok;
}


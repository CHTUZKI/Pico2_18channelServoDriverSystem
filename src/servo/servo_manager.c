/**
 * @file servo_manager.c
 * @brief 舵机统一管理实现
 * @date 2025-10-22
 */

#include "servo/servo_manager.h"
#include "utils/error_handler.h"
#include <stdio.h>
#include <string.h>

// 舵机类型数组
static servo_manager_type_t g_servo_types[SERVO_COUNT];

bool servo_manager_init(void) {
    // 初始化两种舵机系统
    if (!servo_control_init()) {
        return false;
    }
    
    if (!servo_360_init()) {
        return false;
    }
    
    // 默认所有舵机为180度位置舵机
    for (int i = 0; i < SERVO_COUNT; i++) {
        g_servo_types[i] = SERVO_TYPE_POSITION_180;
    }
    
    return true;
}

bool servo_manager_set_type(uint8_t id, servo_manager_type_t type) {
    if (id >= SERVO_COUNT) {
        return false;
    }
    
    g_servo_types[id] = type;
    
    // 根据类型启用对应模式
    if (type == SERVO_TYPE_CONTINUOUS_360) {
        servo_360_enable_mode(id);
    }
    
    return true;
}

servo_manager_type_t servo_manager_get_type(uint8_t id) {
    if (id >= SERVO_COUNT) {
        return SERVO_TYPE_POSITION_180;
    }
    return g_servo_types[id];
}

void servo_manager_enable(uint8_t id, bool enable) {
    if (id == 0xFF) {
        // 使能/禁用所有舵机
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (g_servo_types[i] == SERVO_TYPE_POSITION_180) {
                servo_enable(i, enable);
            } else {
                servo_360_enable(i, enable);
            }
        }
    } else if (id < SERVO_COUNT) {
        // 单个舵机
        if (g_servo_types[id] == SERVO_TYPE_POSITION_180) {
            servo_enable(id, enable);
        } else {
            servo_360_enable(id, enable);
        }
    }
}

bool servo_manager_stop(uint8_t id) {
    if (id == 0xFF) {
        // 停止所有舵机
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (g_servo_types[i] == SERVO_TYPE_POSITION_180) {
                // 180度舵机：停止插值运动
                servo_set_angle(i, servo_get_angle(i));
            } else {
                // 360度舵机：停止旋转
                servo_360_stop(i);
            }
        }
        return true;
    }
    
    if (id >= SERVO_COUNT) {
        return false;
    }
    
    if (g_servo_types[id] == SERVO_TYPE_POSITION_180) {
        return servo_set_angle(id, servo_get_angle(id));
    } else {
        return servo_360_stop(id);
    }
}

void servo_manager_update_all(void) {
    // 更新所有360度舵机 (加减速、超时保护等)
    servo_360_update_all();
    
    // 180度舵机的插值更新在motion任务中处理
}

bool servo_manager_set_angle(uint8_t id, float angle) {
    if (id >= SERVO_COUNT) {
        error_set(ERROR_CMD_ID);
        return false;
    }
    
    // 检查是否为180度舵机
    if (g_servo_types[id] != SERVO_TYPE_POSITION_180) {
        error_set(ERROR_CMD_TYPE);
        return false;
    }
    
    return servo_set_angle(id, angle);
}

bool servo_manager_set_all_angles(const float angles[SERVO_COUNT]) {
    if (angles == NULL) {
        return false;
    }
    
    // 只设置180度舵机
    for (int i = 0; i < SERVO_COUNT; i++) {
        if (g_servo_types[i] == SERVO_TYPE_POSITION_180) {
            if (!servo_set_angle(i, angles[i])) {
                return false;
            }
        }
    }
    
    return true;
}

bool servo_manager_set_speed(uint8_t id, int8_t speed) {
    if (id >= SERVO_COUNT) {
        error_set(ERROR_CMD_ID);
        return false;
    }
    
    // 检查是否为360度舵机
    if (g_servo_types[id] != SERVO_TYPE_CONTINUOUS_360) {
        error_set(ERROR_CMD_TYPE);
        return false;
    }
    
    return servo_360_set_speed(id, speed);
}

bool servo_manager_soft_stop(uint8_t id) {
    if (id == 0xFF) {
        // 软停止所有360度舵机
        for (int i = 0; i < SERVO_COUNT; i++) {
            if (g_servo_types[i] == SERVO_TYPE_CONTINUOUS_360) {
                servo_360_soft_stop(i);
            }
        }
        return true;
    }
    
    if (id >= SERVO_COUNT) {
        return false;
    }
    
    if (g_servo_types[id] != SERVO_TYPE_CONTINUOUS_360) {
        return false;
    }
    
    return servo_360_soft_stop(id);
}

bool servo_manager_get_value(uint8_t id, float *value) {
    if (id >= SERVO_COUNT || value == NULL) {
        return false;
    }
    
    if (g_servo_types[id] == SERVO_TYPE_POSITION_180) {
        *value = servo_get_angle(id);
    } else {
        *value = (float)servo_360_get_speed(id);
    }
    
    return true;
}

bool servo_manager_is_enabled(uint8_t id) {
    if (id >= SERVO_COUNT) {
        return false;
    }
    
    if (g_servo_types[id] == SERVO_TYPE_POSITION_180) {
        return servo_is_enabled(id);
    } else {
        return servo_360_is_enabled(id);
    }
}

void servo_manager_print_status(void) {
    printf("\n========== Servo Status ==========\n");
    printf("ID  Type    Enabled  Value\n");
    printf("--  ------  -------  -----\n");
    
    for (int i = 0; i < SERVO_COUNT; i++) {
        const char *type_str = (g_servo_types[i] == SERVO_TYPE_POSITION_180) ? 
                               "180°" : "360°";
        bool enabled = servo_manager_is_enabled(i);
        float value;
        servo_manager_get_value(i, &value);
        
        printf("%2d  %s  %s  ", i, type_str, enabled ? "Yes" : "No");
        
        if (g_servo_types[i] == SERVO_TYPE_POSITION_180) {
            printf("%.1f°\n", value);
        } else {
            printf("%+d%%\n", (int)value);
        }
    }
    
    printf("==================================\n\n");
}


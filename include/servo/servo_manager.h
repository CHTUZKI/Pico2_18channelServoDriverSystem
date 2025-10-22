/**
 * @file servo_manager.h
 * @brief 舵机统一管理接口
 * @date 2025-10-22
 * 
 * 统一管理180度位置舵机和360度连续旋转舵机
 * 自动识别舵机类型并调用相应的控制函数
 */

#ifndef SERVO_MANAGER_H
#define SERVO_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "servo/servo_control.h"
#include "servo/servo_360.h"

/**
 * @brief 舵机类型
 */
typedef enum {
    SERVO_TYPE_POSITION_180,    // 180度位置舵机 (默认)
    SERVO_TYPE_CONTINUOUS_360   // 360度连续旋转舵机
} servo_manager_type_t;

// ==================== 初始化 ====================

/**
 * @brief 初始化舵机管理器
 * @return true 成功, false 失败
 */
bool servo_manager_init(void);

/**
 * @brief 设置舵机类型
 * @param id 舵机ID (0-17)
 * @param type 舵机类型
 * @return true 成功, false 失败
 */
bool servo_manager_set_type(uint8_t id, servo_manager_type_t type);

/**
 * @brief 获取舵机类型
 * @param id 舵机ID
 * @return 舵机类型
 */
servo_manager_type_t servo_manager_get_type(uint8_t id);

// ==================== 统一控制接口 ====================

/**
 * @brief 统一使能/禁用接口
 * @param id 舵机ID (0-17, 0xFF=全部)
 * @param enable true 使能, false 禁用
 */
void servo_manager_enable(uint8_t id, bool enable);

/**
 * @brief 统一停止接口
 * @param id 舵机ID (0-17, 0xFF=全部)
 * @return true 成功, false 失败
 */
bool servo_manager_stop(uint8_t id);

/**
 * @brief 统一更新接口 (应在FreeRTOS任务中定期调用)
 * @note 对于180度舵机：调用插值更新
 *       对于360度舵机：调用加减速更新
 */
void servo_manager_update_all(void);

// ==================== 位置舵机专用 (自动检查类型) ====================

/**
 * @brief 设置位置舵机角度
 * @param id 舵机ID
 * @param angle 角度 (0-180度)
 * @return true 成功, false 失败 (如果是360度舵机会失败)
 */
bool servo_manager_set_angle(uint8_t id, float angle);

/**
 * @brief 批量设置位置舵机角度
 * @param angles 角度数组 (18个元素)
 * @return true 成功, false 失败
 */
bool servo_manager_set_all_angles(const float angles[SERVO_COUNT]);

// ==================== 360度舵机专用 (自动检查类型) ====================

/**
 * @brief 设置360度舵机速度
 * @param id 舵机ID
 * @param speed 速度 (-100到+100)
 * @return true 成功, false 失败 (如果是180度舵机会失败)
 */
bool servo_manager_set_speed(uint8_t id, int8_t speed);

/**
 * @brief 软停止360度舵机
 * @param id 舵机ID (0-17, 0xFF=全部360度舵机)
 * @return true 成功, false 失败
 */
bool servo_manager_soft_stop(uint8_t id);

// ==================== 信息查询 ====================

/**
 * @brief 获取舵机当前位置/速度
 * @param id 舵机ID
 * @param value 输出值：180度舵机返回角度，360度舵机返回速度
 * @return true 成功, false 失败
 */
bool servo_manager_get_value(uint8_t id, float *value);

/**
 * @brief 检查舵机是否使能
 * @param id 舵机ID
 * @return true 使能, false 禁用
 */
bool servo_manager_is_enabled(uint8_t id);

/**
 * @brief 打印所有舵机状态 (用于调试)
 */
void servo_manager_print_status(void);

#endif // SERVO_MANAGER_H


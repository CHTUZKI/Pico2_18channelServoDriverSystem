/**
 * @file param_manager.h
 * @brief 参数管理器
 * @date 2025-10-21
 */

#ifndef PARAM_MANAGER_H
#define PARAM_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 初始化参数管理器
 * @return true 成功, false 失败
 */
bool param_manager_init(void);

/**
 * @brief 保存所有参数到Flash
 * @return true 成功, false 失败
 */
bool param_manager_save(void);

/**
 * @brief 从Flash加载所有参数
 * @return true 成功, false 失败
 */
bool param_manager_load(void);

/**
 * @brief 应用参数到舵机
 * @return true 成功, false 失败
 */
bool param_manager_apply_to_servos(void);

/**
 * @brief 保存当前所有舵机位置到Flash
 * @return true 成功, false 失败
 */
bool param_manager_save_positions(void);

/**
 * @brief 从Flash加载舵机位置并应用
 * @return true 成功, false 失败（或位置数据无效）
 */
bool param_manager_load_positions(void);

/**
 * @brief 获取Flash中保存的位置是否有效
 * @return true 有效, false 无效
 */
bool param_manager_has_saved_positions(void);

/**
 * @brief 设置起始位置（从上位机设置所有舵机的起始角度）
 * @param positions 18个舵机的角度数组
 * @return true 成功, false 失败
 */
bool param_manager_set_start_positions(const float *positions);

#endif // PARAM_MANAGER_H


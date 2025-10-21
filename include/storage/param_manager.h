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
 * @brief 恢复出厂设置
 * @return true 成功, false 失败
 */
bool param_manager_factory_reset(void);

/**
 * @brief 应用参数到舵机
 * @return true 成功, false 失败
 */
bool param_manager_apply_to_servos(void);

#endif // PARAM_MANAGER_H


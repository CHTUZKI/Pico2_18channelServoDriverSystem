/**
 * @file auto_test.h
 * @brief 自动测试模式 - 上电自动运行测试序列
 * @author AI Assistant
 * @date 2025-10-23
 */

#ifndef AUTO_TEST_H
#define AUTO_TEST_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 启动自动测试模式
 * 
 * 测试序列：
 * 1. 所有18个舵机从90°移动到0°（2秒）
 * 2. 所有18个舵机从0°移动到180°（2秒）
 * 3. 所有18个舵机从180°回到90°（2秒）
 * 4. 重复20次
 */
void auto_test_start(void);

/**
 * @brief 停止自动测试
 */
void auto_test_stop(void);

/**
 * @brief 检查测试是否正在运行
 * @return true 正在运行，false 已停止
 */
bool auto_test_is_running(void);

/**
 * @brief 获取当前测试循环次数
 * @return 当前循环次数
 */
uint32_t auto_test_get_cycle_count(void);

#endif // AUTO_TEST_H

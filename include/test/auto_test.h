/**
 * @file auto_test.h
 * @brief 自动测试模式 - 基于QP/C事件驱动的循环测试
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
 * 1. 所有18个舵机从0°移动到180°（2秒）
 * 2. 所有18个舵机从180°移动到0°（2秒）
 * 3. 重复20次
 * 4. 最后回到90°安全位置
 * 
 * 实现方式：监听MOTION_COMPLETE_SIG信号触发下一步，
 *          避免使用硬件定时器，不会与QP/C冲突
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

/**
 * @brief 运动完成事件回调（由Motion AO调用）
 * 
 * 当Motion AO完成一次运动时调用此函数，
 * 自动测试会根据当前状态触发下一步运动
 */
void auto_test_on_motion_complete(void);

#endif // AUTO_TEST_H

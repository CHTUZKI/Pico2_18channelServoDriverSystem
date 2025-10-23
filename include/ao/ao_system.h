/**
 * @file ao_system.h
 * @brief System主动对象 - 系统监控、LED、错误处理
 * @date 2025-10-23
 */

#ifndef AO_SYSTEM_H
#define AO_SYSTEM_H

#include "qpc.h"

// ==================== System AO声明 ====================

/**
 * @brief System主动对象类
 */
typedef struct {
    QActive super;  // 继承QActive
    
    // 时间事件
    QTimeEvt led_timer;     // LED更新定时器
    
    // 状态数据
    uint32_t error_count;   // 错误计数
    bool emergency_stop;    // 急停标志
} AO_System_t;

// 全局实例声明
extern AO_System_t AO_System_inst;
extern QActive * const AO_System;

// ==================== 公共接口 ====================

/**
 * @brief 构造System AO
 */
void AO_System_ctor(void);

#endif // AO_SYSTEM_H


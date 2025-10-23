/**
 * @file events.h
 * @brief 系统事件信号定义
 * @date 2025-10-23
 */

#ifndef EVENTS_H
#define EVENTS_H

#include "qpc.h"

// ==================== 事件信号枚举 ====================

enum ServoSignals {
    // 系统预留信号（QP/C定义）
    // Q_EMPTY_SIG = 0
    // Q_ENTRY_SIG = 1
    // Q_EXIT_SIG = 2
    // Q_INIT_SIG = 3
    // Q_USER_SIG = 4  // 用户信号起始
    
    // ========== 系统事件 (4-9) ==========
    INIT_COMPLETE_SIG = Q_USER_SIG,     // 4 - 初始化完成
    TIMEOUT_SIG,                         // 5 - 超时事件
    TICK_1MS_SIG,                        // 6 - 1ms系统Tick
    
    // ========== 通信事件 (10-19) ==========
    USB_RX_DATA_SIG = 10,                // 10 - USB接收到数据
    CMD_MOVE_SINGLE_SIG,                 // 11 - 单轴移动命令
    CMD_MOVE_ALL_SIG,                    // 12 - 全轴移动命令
    CMD_GET_SINGLE_SIG,                  // 13 - 查询单轴
    CMD_GET_ALL_SIG,                     // 14 - 查询全轴
    CMD_ENABLE_SIG,                      // 15 - 使能命令
    CMD_DISABLE_SIG,                     // 16 - 禁用命令
    CMD_FLASH_SAVE_SIG,                  // 17 - Flash保存
    CMD_FLASH_LOAD_SIG,                  // 18 - Flash加载
    CMD_PING_SIG,                        // 19 - Ping命令
    
    // ========== 运动控制事件 (20-29) ==========
    MOTION_START_SIG = 20,               // 20 - 开始运动
    MOTION_STOP_SIG,                     // 21 - 停止运动
    MOTION_COMPLETE_SIG,                 // 22 - 运动完成
    INTERP_TICK_SIG,                     // 23 - 插值时钟（20ms）
    
    // ========== 系统监控事件 (30-39) ==========
    ERROR_SIG = 30,                      // 30 - 错误事件
    ESTOP_SIG,                           // 31 - 紧急停止
    LED_UPDATE_SIG,                      // 32 - LED更新
    FLASH_COMPLETE_SIG,                  // 33 - Flash操作完成
    
    // ========== 其他 ==========
    MAX_PUB_SIG,                         // 发布信号最大值
    MAX_SIG                              // 所有信号最大值
};

// ==================== 事件参数检查 ====================

// 确保信号数量不超过限制
#if (MAX_SIG > 100)
    #error "Too many signals defined!"
#endif

#endif // EVENTS_H


/**
 * @file qp_config.h
 * @brief QP/C框架配置
 * @date 2025-10-23
 * 
 * QP/C (Quantum Platform) 配置文件
 * 用于18通道舵机控制系统
 */

#ifndef QP_CONFIG_H
#define QP_CONFIG_H

// ==================== QP/C基本配置 ====================

// 信号大小（2字节足够）
#define Q_SIGNAL_SIZE       2

// 最大主动对象数量（3个AO + 1个预留）
#define QF_MAX_ACTIVE       8

// 时间事件计数器大小（4字节，支持长时间）
#define QF_TIMEEVT_CTR_SIZE 4

// 事件队列计数器大小
#define QF_EQUEUE_CTR_SIZE  1

// 事件大小字段大小
#define QF_EVENT_SIZ_SIZE   2

// 最大Tick频率（只用1个，1ms tick）
#define QF_MAX_TICK_RATE    1

// 事件池数量（用于动态事件分配）
#define QF_MAX_EPOOL        3

// ==================== 架构配置 ====================

// ARM架构版本（Cortex-M33是ARMv8-M，使用ARMv7-M的代码路径）
// RP2350的M33支持所有ARMv7-M特性
#define __ARM_ARCH          7

// ==================== 可选功能 ====================

// 禁用软件跟踪（节省空间）
// #define Q_SPY

// 启用安全检查（调试模式）
#ifndef NDEBUG
// #define Q_UNSAFE  // 禁用双重反向存储检查（调试时不禁用）
#endif

// ==================== QP端口配置 ====================

// 使用BASEPRI进行中断屏蔽（ARMv7-M及以上）
// QF_BASEPRI在qp_port.h中已定义为0x3F

// ==================== 应用特定配置 ====================

// 主动对象名称（便于调试）
#define QP_APP_NAME         "ServoController"
#define QP_APP_VERSION      "1.0.0-QPC"

#endif // QP_CONFIG_H


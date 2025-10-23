/**
 * @file event_types.h
 * @brief 事件数据结构定义
 * @date 2025-10-23
 */

#ifndef EVENT_TYPES_H
#define EVENT_TYPES_H

#include "qpc.h"
#include "config/config.h"

// ==================== 移动命令事件 ====================

/**
 * @brief 单轴移动命令事件
 */
typedef struct {
    QEvt super;                 // 继承QEvt
    uint8_t servo_id;          // 舵机ID
    float angle;               // 目标角度
    uint16_t duration_ms;      // 运动时间(ms)
} MoveSingleEvt;

/**
 * @brief 全轴移动命令事件
 */
typedef struct {
    QEvt super;                     // 继承QEvt
    float angles[SERVO_COUNT];     // 18个舵机的目标角度
    uint16_t duration_ms;          // 运动时间(ms)
} MoveAllEvt;

// ==================== 查询事件 ====================

/**
 * @brief 查询单轴状态事件
 */
typedef struct {
    QEvt super;
    uint8_t servo_id;
} QuerySingleEvt;

// ==================== 配置事件 ====================

/**
 * @brief 使能/禁用命令事件
 */
typedef struct {
    QEvt super;
    uint8_t servo_id;   // 0xFF表示全部
    bool enable;
} EnableEvt;

// ==================== Flash事件 ====================

/**
 * @brief Flash操作事件
 */
typedef struct {
    QEvt super;
    enum {
        FLASH_OP_SAVE,
        FLASH_OP_LOAD,
        FLASH_OP_RESET
    } operation;
} FlashEvt;

// ==================== 错误事件 ====================

/**
 * @brief 错误事件
 */
typedef struct {
    QEvt super;
    uint8_t error_code;     // 错误码
    uint8_t source_ao;      // 来源AO
} ErrorEvt;

// ==================== 运动控制事件 ====================

/**
 * @brief 运动开始事件
 */
typedef struct {
    QEvt super;
    uint8_t axis_count;             // 轴数量
    uint8_t axis_ids[SERVO_COUNT]; // 轴ID数组
    float target_positions[SERVO_COUNT];  // 目标位置
    uint16_t duration_ms;           // 运动时间
} MotionStartEvt;

// ==================== 事件构造辅助宏 ====================

// 静态事件（不需要动态分配）
#define MAKE_STATIC_EVT(sig_) { {(sig_), 0U, 0U} }

#endif // EVENT_TYPES_H


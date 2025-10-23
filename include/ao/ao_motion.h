/**
 * @file ao_motion.h
 * @brief Motion主动对象 - 运动控制和插值
 * @date 2025-10-23
 */

#ifndef AO_MOTION_H
#define AO_MOTION_H

#include "qpc.h"
#include "motion/interpolation.h"

// ==================== Motion AO声明 ====================

/**
 * @brief Motion主动对象类
 */
typedef struct {
    QActive super;  // 继承QActive
    
    // 时间事件
    QTimeEvt interp_timer;  // 插值定时器（20ms）
    
    // 插值器
    multi_axis_interpolator_t interpolator;
    
    // 状态数据
    bool is_moving;         // 是否正在运动
} AO_Motion_t;

// 全局实例声明
extern AO_Motion_t AO_Motion_inst;
extern QActive * const AO_Motion;

// ==================== 公共接口 ====================

/**
 * @brief 构造Motion AO
 */
void AO_Motion_ctor(void);

#endif // AO_MOTION_H


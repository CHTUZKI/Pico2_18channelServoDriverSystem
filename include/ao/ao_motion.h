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

/**
 * @brief 设置单轴梯形速度运动
 * @param axis_id 轴ID（0-17）
 * @param target_pos 目标位置
 * @param params 运动参数
 * @return true 成功, false 失败
 */
bool AO_Motion_set_trapezoid(uint8_t axis_id, float target_pos, const motion_params_t *params);

/**
 * @brief 设置单轴轨迹队列
 * @param axis_id 轴ID（0-17）
 * @param trajectory 轨迹队列指针
 * @return true 成功, false 失败
 */
bool AO_Motion_set_trajectory(uint8_t axis_id, trajectory_queue_t *trajectory);

/**
 * @brief 获取单轴插值器
 * @param axis_id 轴ID（0-17）
 * @return 插值器指针，NULL表示无效ID
 */
interpolator_t* AO_Motion_get_interpolator(uint8_t axis_id);

#endif // AO_MOTION_H


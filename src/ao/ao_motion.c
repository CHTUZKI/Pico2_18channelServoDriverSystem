/**
 * @file ao_motion.c
 * @brief Motion主动对象实现
 * @date 2025-10-23
 */

#define QP_IMPL
#include "qpc.h"
#include "ao/ao_motion.h"
#include "ao/ao_system.h"
#include "events/events.h"
#include "events/event_types.h"
#include "config/config.h"
#include "servo/servo_control.h"
#include "servo/servo_manager.h"
#include <stdio.h>

Q_DEFINE_THIS_FILE

// ==================== Motion AO实例 ====================

AO_Motion_t AO_Motion_inst;
QActive * const AO_Motion = &AO_Motion_inst.super;

// ==================== 状态处理函数声明 ====================

static QState AO_Motion_initial(AO_Motion_t * const me, void const * const par);
static QState AO_Motion_idle(AO_Motion_t * const me, QEvt const * const e);
static QState AO_Motion_moving(AO_Motion_t * const me, QEvt const * const e);

// ==================== 构造函数 ====================

void AO_Motion_ctor(void) {
    AO_Motion_t *me = &AO_Motion_inst;
    
    // 调用QActive构造函数
    QActive_ctor(&me->super, Q_STATE_CAST(&AO_Motion_initial));
    
    // 构造时间事件
    QTimeEvt_ctorX(&me->interp_timer, &me->super, INTERP_TICK_SIG, 0U);
    
    // 初始化插值器
    multi_interpolator_init(&me->interpolator);
    
    // 初始化状态数据
    me->is_moving = false;
}

// ==================== 状态机实现 ====================

/**
 * @brief 初始状态
 */
static QState AO_Motion_initial(AO_Motion_t * const me, void const * const par) {
    (void)par;
    
    printf("[AO-MOTION] Initial state\n");
    
    // 启动插值定时器（20ms周期）
    QTimeEvt_armX(&me->interp_timer, 
                  TIME_EVENT_INTERP_MS,     // 首次触发
                  TIME_EVENT_INTERP_MS);    // 周期
    
    // 转换到idle状态
    return Q_TRAN(&AO_Motion_idle);
}

/**
 * @brief 空闲状态
 */
static QState AO_Motion_idle(AO_Motion_t * const me, QEvt const * const e) {
    QState status;
    
    switch (e->sig) {
        case Q_ENTRY_SIG: {
            printf("[AO-MOTION] Entering IDLE state\n");
            me->is_moving = false;
            status = Q_HANDLED();
            break;
        }
        
        case MOTION_START_SIG: {
            // 开始运动
            MotionStartEvt const *evt = Q_EVT_CAST(MotionStartEvt);
            
            printf("[AO-MOTION] Motion start, duration=%d ms\n", evt->duration_ms);
            
            // 设置插值器
            float start_positions[SERVO_COUNT];
            for (uint8_t i = 0; i < evt->axis_count && i < SERVO_COUNT; i++) {
                start_positions[i] = servo_get_angle(evt->axis_ids[i]);
            }
            
            multi_interpolator_set_motion(&me->interpolator,
                                        start_positions,
                                        evt->target_positions,
                                        evt->duration_ms,
                                        INTERP_TYPE_S_CURVE);  // 使用S曲线
            
            // 转换到moving状态
            status = Q_TRAN(&AO_Motion_moving);
            break;
        }
        
        case INTERP_TICK_SIG: {
            // 空闲状态下忽略插值Tick
            status = Q_HANDLED();
            break;
        }
        
        case ESTOP_SIG: {
            // 空闲状态下收到急停，直接处理
            printf("[AO-MOTION] ESTOP in IDLE (no action needed)\n");
            status = Q_HANDLED();
            break;
        }
        
        default: {
            status = Q_SUPER(&QHsm_top);
            break;
        }
    }
    
    return status;
}

/**
 * @brief 运动状态
 */
static QState AO_Motion_moving(AO_Motion_t * const me, QEvt const * const e) {
    QState status;
    
    switch (e->sig) {
        case Q_ENTRY_SIG: {
            printf("[AO-MOTION] Entering MOVING state\n");
            me->is_moving = true;
            status = Q_HANDLED();
            break;
        }
        
        case Q_EXIT_SIG: {
            me->is_moving = false;
            status = Q_HANDLED();
            break;
        }
        
        case INTERP_TICK_SIG: {
            // 更新插值器（20ms周期）
            float output_positions[SERVO_COUNT];
            multi_interpolator_update(&me->interpolator, 
                                     TIME_EVENT_INTERP_MS, 
                                     output_positions);
            
            // 应用到舵机
            servo_set_all_angles(output_positions);
            
            // 检查是否到达
            if (multi_interpolator_all_reached(&me->interpolator)) {
                printf("[AO-MOTION] Motion complete!\n");
                
                // 发布运动完成事件（其他AO可能需要）
                static QEvt const motion_complete_evt = QEVT_INITIALIZER(MOTION_COMPLETE_SIG);
                QACTIVE_PUBLISH(&motion_complete_evt, AO_Motion);
                
                // 返回idle状态
                status = Q_TRAN(&AO_Motion_idle);
            } else {
                status = Q_HANDLED();
            }
            break;
        }
        
        case MOTION_STOP_SIG:
        case ESTOP_SIG: {
            // 停止运动
            printf("[AO-MOTION] Motion stopped\n");
            
            // 停止插值器
            for (uint8_t i = 0; i < SERVO_COUNT; i++) {
                interpolator_stop(&me->interpolator.axes[i]);
            }
            
            // 返回idle
            status = Q_TRAN(&AO_Motion_idle);
            break;
        }
        
        default: {
            status = Q_SUPER(&QHsm_top);
            break;
        }
    }
    
    return status;
}


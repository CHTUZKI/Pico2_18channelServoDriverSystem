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
#include <math.h>

Q_DEFINE_THIS_FILE

// ==================== 调试宏 ====================
#if DEBUG_MOTION
    #define MOTION_DEBUG(...) printf(__VA_ARGS__)
#else
    #define MOTION_DEBUG(...) ((void)0)
#endif

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
    
    MOTION_DEBUG("[AO-MOTION] Initial state\n");
    
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
            MOTION_DEBUG("[AO-MOTION] >>> Entering IDLE state <<<\n");
            me->is_moving = false;
            status = Q_HANDLED();
            break;
        }
        
        case Q_EXIT_SIG: {
            MOTION_DEBUG("[AO-MOTION] Exiting IDLE state\n");
            status = Q_HANDLED();
            break;
        }
        
        case MOTION_START_SIG: {
            // 开始运动
            MotionStartEvt const *evt = Q_EVT_CAST(MotionStartEvt);
            
            MOTION_DEBUG("[AO-MOTION] Motion start, axis_count=%d, duration=%d ms\n", 
                   evt->axis_count, evt->duration_ms);
            
            // 设置插值器 - 初始化所有舵机的起始位置
            float start_positions[SERVO_COUNT];
            // 先获取所有舵机的当前位置
            for (uint8_t i = 0; i < SERVO_COUNT; i++) {
                start_positions[i] = servo_get_angle(i);
            }
            
            // 调试：打印起始位置（避免浮点printf问题）
            MOTION_DEBUG("[AO-MOTION] Start positions: ");
            for (int i = 0; i < SERVO_COUNT; i++) {
                int angle_int = (int)(start_positions[i] * 10);
                printf("%d.%d ", angle_int / 10, angle_int % 10);
            }
            printf("\n");
            
            // 调试：打印目标位置（避免浮点printf问题）
            MOTION_DEBUG("[AO-MOTION] Target positions: ");
            for (int i = 0; i < SERVO_COUNT; i++) {
                int angle_int = (int)(evt->target_positions[i] * 10);
                printf("%d.%d ", angle_int / 10, angle_int % 10);
            }
            printf("\n");
            
            MOTION_DEBUG("[AO-MOTION] Setting up interpolator for all axes\n");
            
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
            MOTION_DEBUG("[AO-MOTION] ESTOP in IDLE (no action needed)\n");
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
            MOTION_DEBUG("[AO-MOTION] Entering MOVING state\n");
            me->is_moving = true;
            status = Q_HANDLED();
            break;
        }
        
        case Q_EXIT_SIG: {
            MOTION_DEBUG("[AO-MOTION] Exiting MOVING state\n");
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
            
            // 错误检查：防止无效值导致卡死
            bool valid_output = true;
            for (int i = 0; i < SERVO_COUNT; i++) {
                if (output_positions[i] < -180.0f || output_positions[i] > 180.0f || 
                    isnan(output_positions[i]) || isinf(output_positions[i])) {
                    valid_output = false;
                    MOTION_DEBUG("[AO-MOTION] ERROR: Invalid output position[%d] = %.1f\n", i, output_positions[i]);
                    break;
                }
            }
            
            if (!valid_output) {
                MOTION_DEBUG("[AO-MOTION] ERROR: Invalid interpolator output, stopping motion\n");
                status = Q_TRAN(&AO_Motion_idle);
                break;
            }
            
            // 调试：减少输出频率（每10次输出一次）
            static uint32_t debug_count = 0;
            debug_count++;
            if (debug_count % 10 == 0) {
                MOTION_DEBUG("[AO-MOTION] Interpolator output: ");
                for (int i = 0; i < SERVO_COUNT; i++) {
                    int angle_int = (int)(output_positions[i] * 10);
                    printf("%d.%d° ", angle_int / 10, angle_int % 10);
                }
                printf("\n");
            }
            
            // 应用到舵机
            servo_set_all_angles(output_positions);
            
            // 调试：定期打印状态
            static uint32_t tick_count = 0;
            tick_count++;
            if (tick_count % 25 == 0) {  // 每500ms打印一次
                MOTION_DEBUG("[AO-MOTION] TICK #%lu\n", tick_count);
            }
            
            // 检查是否到达
            if (multi_interpolator_all_reached(&me->interpolator)) {
                MOTION_DEBUG("[AO-MOTION] Motion complete! Transitioning to IDLE...\n");
                tick_count = 0;  // 重置计数器
                
                // 暂时注释掉PUBLISH，看是否是问题所在
                // static QEvt const motion_complete_evt = QEVT_INITIALIZER(MOTION_COMPLETE_SIG);
                // QACTIVE_PUBLISH(&motion_complete_evt, AO_Motion);
                
                // 返回idle状态
                status = Q_TRAN(&AO_Motion_idle);
                MOTION_DEBUG("[AO-MOTION] Q_TRAN called\n");
            } else {
                status = Q_HANDLED();
            }
            break;
        }
        
        case MOTION_STOP_SIG:
        case ESTOP_SIG: {
            // 停止运动
            MOTION_DEBUG("[AO-MOTION] Motion stopped\n");
            
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


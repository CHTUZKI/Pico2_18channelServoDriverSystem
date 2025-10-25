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
#include "test/auto_test.h"
#include "utils/usb_bridge.h"
#include "motion/motion_scheduler.h"  // 使用运动调度器（替代motion_buffer）
#include <stdio.h>
#include <math.h>

Q_DEFINE_THIS_FILE

// ==================== 调试宏 ====================
// 使用USB Bridge避免Core 0直接访问USB
#if DEBUG_AO_MOTION
    #define MOTION_DEBUG(...) usb_bridge_printf(__VA_ARGS__)
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

// ==================== 运动执行回调 ====================
/**
 * @brief 调度器回调：执行梯形速度运动
 */
static void motion_execute_trapezoid_callback(uint8_t servo_id, float target_angle, 
                                             float velocity, float acceleration, float deceleration) {
    AO_Motion_set_trapezoid(servo_id, target_angle, velocity, acceleration, deceleration);
    
    // 发送MOTION_START事件触发状态转换
    static MotionStartEvt start_evt;
    start_evt.super.sig = MOTION_START_SIG;
    start_evt.axis_count = 1;
    start_evt.duration_ms = 0;
    start_evt.target_positions[servo_id] = target_angle;
    
    QACTIVE_POST((QActive *)&AO_Motion_inst, (QEvt *)&start_evt, 0);
}

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
    
    // 初始化运动调度器并注册回调
    motion_scheduler_init();
    motion_scheduler_set_callback(motion_execute_trapezoid_callback);
}

// ==================== 状态机实现 ====================

/**
 * @brief 初始状态
 */
static QState AO_Motion_initial(AO_Motion_t * const me, void const * const par) {
    (void)par;
    
    LOG_DEBUG("[AO-MOTION] Initial state\n");
    
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
            LOG_DEBUG("[AO-MOTION] >>> Entering IDLE state <<<\n");
            me->is_moving = false;
            status = Q_HANDLED();
            break;
        }
        
        case Q_EXIT_SIG: {
            LOG_DEBUG("[AO-MOTION] Exiting IDLE state\n");
            status = Q_HANDLED();
            break;
        }
        
        case MOTION_START_SIG: {
            // 开始运动
            MotionStartEvt const *evt = Q_EVT_CAST(MotionStartEvt);
            
            LOG_DEBUG("[AO-MOTION] Motion start, axis_count=%d, duration=%d ms\n", 
                   evt->axis_count, evt->duration_ms);
            
            // 检查是否有舵机已经设置了梯形速度（状态为MOVING且类型为TRAPEZOID）
            bool has_trapezoid = false;
            for (uint8_t i = 0; i < SERVO_COUNT; i++) {
                if (me->interpolator.axes[i].state == MOTION_STATE_MOVING &&
                    me->interpolator.axes[i].type == INTERP_TYPE_TRAPEZOID) {
                    has_trapezoid = true;
                    break;
                }
            }
            
            if (has_trapezoid) {
                // 如果有梯形速度运动，不要覆盖插值器设置
                // 梯形速度的插值器已经在AO_Motion_set_trapezoid中设置好了
                #if DEBUG_MOTION_SUMMARY
                usb_bridge_printf("[MOTION] Trapezoid motion detected, skip interpolator reset\n");
                #endif
                
                // 重要！确保其他没有设置梯形速度的舵机保持IDLE状态
                int moving_count = 0;
                for (uint8_t i = 0; i < SERVO_COUNT; i++) {
                    if (me->interpolator.axes[i].type == INTERP_TYPE_TRAPEZOID &&
                        me->interpolator.axes[i].state == MOTION_STATE_MOVING) {
                        moving_count++;
                    } else {
                        me->interpolator.axes[i].state = MOTION_STATE_IDLE;
                        me->interpolator.axes[i].current_pos = servo_get_angle(i);
                    }
                }
                
                #if DEBUG_MOTION_SUMMARY
                usb_bridge_printf("[MOTION] Active servos: %d (others forced to IDLE)\n", moving_count);
                #endif
            } else {
                // 基于时间的运动，设置插值器
                float start_positions[SERVO_COUNT];
                for (uint8_t i = 0; i < SERVO_COUNT; i++) {
                    start_positions[i] = servo_get_angle(i);
                }
                
                LOG_DEBUG("[AO-MOTION] Setting up interpolator for all axes\n");
                
                multi_interpolator_set_motion(&me->interpolator,
                                            start_positions,
                                            evt->target_positions,
                                            evt->duration_ms,
                                            INTERP_TYPE_S_CURVE);  // 使用S曲线
            }
            
            // 转换到moving状态
            status = Q_TRAN(&AO_Motion_moving);
            break;
        }
        
        case INTERP_TICK_SIG: {
            // 运动缓冲区调度器（每20ms检查一次）
            motion_scheduler_update();
            
            status = Q_HANDLED();
            break;
        }
        
        case ESTOP_SIG: {
            // 空闲状态下收到急停，直接处理
            LOG_DEBUG("[AO-MOTION] ESTOP in IDLE (no action needed)\n");
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
            LOG_DEBUG("[AO-MOTION] Entering MOVING state\n");
            me->is_moving = true;
            status = Q_HANDLED();
            break;
        }
        
        case Q_EXIT_SIG: {
            LOG_DEBUG("[AO-MOTION] Exiting MOVING state\n");
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
                    LOG_DEBUG("[AO-MOTION] ERROR: Invalid output position[%d] = %.1f\n", i, output_positions[i]);
                    break;
                }
            }
            
            if (!valid_output) {
                usb_bridge_printf("[ERROR] Invalid interpolator output, stopping motion\n");
                status = Q_TRAN(&AO_Motion_idle);
                break;
            }
            
            // 只应用正在运动的舵机（优化性能，避免误触其他舵机）
            for (int i = 0; i < SERVO_COUNT; i++) {
                if (me->interpolator.axes[i].state == MOTION_STATE_MOVING) {
                    // 只更新正在运动的舵机
                    servo_set_angle(i, output_positions[i]);
                    
                    #if DEBUG_PWM_SUMMARY
                    // 追踪PWM显著变化
                    static float last_pwm_pos[SERVO_COUNT] = {90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90, 90};
                    float delta = fabsf(output_positions[i] - last_pwm_pos[i]);
                    if (delta > 5.0f) {  // 变化超过5度才输出
                        int pos_int = (int)output_positions[i];
                        int delta_int = (int)delta;
                        usb_bridge_printf("[PWM-CHG] Servo%d: %ddeg (Δ%d)\n", i, pos_int, delta_int);
                        last_pwm_pos[i] = output_positions[i];
                    }
                    #endif
                }
            }
            
            // 调试：定期打印状态
            static uint32_t tick_count = 0;
            tick_count++;
            if (tick_count % 25 == 0) {  // 每500ms打印一次
                LOG_DEBUG("[AO-MOTION] TICK #%lu\n", tick_count);
            }
            
            // 检查是否到达
            if (multi_interpolator_all_reached(&me->interpolator)) {
                LOG_DEBUG("[AO-MOTION] Motion complete! Transitioning to IDLE...\n");
                tick_count = 0;  // 重置计数器
                
                // 通知自动测试（如果正在运行）
                if (auto_test_is_running()) {
                    auto_test_on_motion_complete();
                }
                
                // 返回idle状态
                status = Q_TRAN(&AO_Motion_idle);
                LOG_DEBUG("[AO-MOTION] Q_TRAN called\n");
            } else {
                status = Q_HANDLED();
            }
            break;
        }
        
        case MOTION_STOP_SIG:
        case ESTOP_SIG: {
            // 停止运动
            LOG_DEBUG("[AO-MOTION] Motion stopped\n");
            
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

// ==================== 公共接口实现 ====================

bool AO_Motion_set_trapezoid(uint8_t axis_id, float target_pos, const motion_params_t *params) {
    if (axis_id >= SERVO_COUNT || params == NULL) {
        return false;
    }
    
    AO_Motion_t *me = &AO_Motion_inst;
    interpolator_t *interp = &me->interpolator.axes[axis_id];
    
    // 获取当前位置作为起始位置
    float start_pos = servo_get_angle(axis_id);
    
    // 设置梯形速度运动
    interpolator_set_trapezoid_motion(interp, start_pos, target_pos, params);
    
    #if DEBUG_MOTION_SUMMARY
    // 运动摘要信息（整数格式，避免浮点printf问题）
    int start_int = (int)start_pos;
    int target_int = (int)target_pos;
    int dist_int = (int)(target_pos - start_pos);
    int vel_int = (int)(params->max_velocity * 10);  // ×10保留一位小数
    int accel_int = (int)(params->acceleration * 10);
    usb_bridge_printf("[MOTION] Trapezoid: Axis%d %d->%d deg (d=%d) v=%d.%d a=%d.%d\n",
                     axis_id, start_int, target_int, dist_int,
                     vel_int/10, vel_int%10, accel_int/10, accel_int%10);
    #endif
    
    // 标记为运动中
    me->is_moving = true;
    
    return true;
}

bool AO_Motion_set_trajectory(uint8_t axis_id, trajectory_queue_t *trajectory) {
    if (axis_id >= SERVO_COUNT || trajectory == NULL) {
        return false;
    }
    
    AO_Motion_t *me = &AO_Motion_inst;
    interpolator_t *interp = &me->interpolator.axes[axis_id];
    
    // 绑定轨迹队列到插值器
    interp->trajectory = trajectory;
    
    MOTION_DEBUG("[AO-MOTION] Set trajectory: axis=%d, points=%d\n",
                 axis_id, trajectory->count);
    
    return true;
}

interpolator_t* AO_Motion_get_interpolator(uint8_t axis_id) {
    if (axis_id >= SERVO_COUNT) {
        return NULL;
    }
    
    return &AO_Motion_inst.interpolator.axes[axis_id];
}



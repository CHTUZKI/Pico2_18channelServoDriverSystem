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
#include "servo/servo_360.h"
#include "test/auto_test.h"
#include "utils/usb_bridge.h"
#include "motion/planner.h"  // 使用Look-Ahead运动规划器
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
 * @brief 规划器回调：执行规划好的运动块
 * @param block 规划块（包含优化后的速度参数）
 */
static void planner_execute_block_callback(const plan_block_t *block) {
    if (block == NULL) {
        return;
    }
    
    AO_Motion_t *me = &AO_Motion_inst;
    
    // ========== 360度连续旋转模式 ==========
    if (block->flags.is_continuous) {
        // 360度舵机：速度控制模式
        #if DEBUG_MOTION_SUMMARY
        usb_bridge_printf("[AO-MOTION] Execute 360° block: S%d speed=%d%% (entry=%d%% exit=%d%%)\n",
                         block->servo_id, block->target_speed_pct,
                         block->entry_speed_pct, block->exit_speed_pct);
        #endif
        
        // 直接设置360度舵机速度
        // 注意：这里可以进一步优化，实现速度的平滑过渡
        // 当前版本：直接设置目标速度，由 servo_360 模块处理加减速
        servo_360_set_speed(block->servo_id, block->target_speed_pct);
        
        // 如果有指定持续时间，设置定时器
        // TODO: 可以添加定时器在 duration_ms 后自动停止
        
        return;
    }
    
    // ========== 位置控制模式（180度舵机）==========
    // 使用规划器计算好的进入/退出速度设置梯形运动
    // 注意：这里使用的是规划器优化后的速度参数
    
    interpolator_t *interp = &me->interpolator.axes[block->servo_id];
    
    motion_params_t params = {
        .max_velocity = block->v_max_actual,      // 使用规划后的最大速度
        .acceleration = block->acceleration,
        .deceleration = block->deceleration
    };
    
    // 设置梯形运动（注意：起始位置是当前位置，不是block->start_angle）
    // 因为可能有速度衔接，不一定从0速度开始
    interpolator_set_trapezoid_motion(interp,
                                      block->start_angle,
                                      block->target_angle,
                                      &params);
    
    // 手动设置进入/退出速度（实现速度平滑过渡）
    interp->t_accel = block->t_accel;
    interp->t_const = block->t_const;
    interp->t_decel = block->t_decel;
    interp->v_max_actual = block->v_max_actual;
    interp->duration = block->duration_ms;
    
    #if DEBUG_MOTION_SUMMARY
    usb_bridge_printf("[AO-MOTION] Execute position block: S%d %.1f->%.1f v=%.1f (entry=%.1f exit=%.1f)\n",
                     block->servo_id, block->start_angle, block->target_angle,
                     block->v_max_actual, block->entry_speed, block->exit_speed);
    #endif
    
    // 发送MOTION_START事件触发状态转换
    static MotionStartEvt start_evt;
    start_evt.super.sig = MOTION_START_SIG;
    start_evt.axis_count = 1;
    start_evt.duration_ms = block->duration_ms;
    start_evt.target_positions[block->servo_id] = block->target_angle;
    
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
    
    // 初始化运动规划器并注册回调
    planner_init(NULL);  // 使用全局实例
    planner_set_callback(planner_execute_block_callback);
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
            // 运动规划器更新（每20ms检查一次）
            // 执行：时间戳检查、前瞻规划、运动调度
            planner_update();
            
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



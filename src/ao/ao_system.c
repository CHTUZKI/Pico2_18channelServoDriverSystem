/**
 * @file ao_system.c
 * @brief System主动对象实现
 * @date 2025-10-23
 */

#define QP_IMPL
#include "qpc.h"
#include "ao/ao_system.h"
#include "events/events.h"
#include "events/event_types.h"
#include "config/config.h"
#include "config/pinout.h"
#include "utils/error_handler.h"
#include "storage/param_manager.h"
#include "pwm/pwm_driver.h"
#include "pico/stdlib.h"
#include "utils/usb_bridge.h"
#include <stdio.h>

Q_DEFINE_THIS_FILE

// ==================== 调试宏 ====================
// 使用USB Bridge避免Core 0直接访问USB
#if DEBUG_SYSTEM
    #define SYSTEM_DEBUG(...) usb_bridge_printf(__VA_ARGS__)
#else
    #define SYSTEM_DEBUG(...) ((void)0)
#endif

// ==================== System AO实例 ====================

AO_System_t AO_System_inst;
QActive * const AO_System = &AO_System_inst.super;

// ==================== 状态处理函数声明 ====================

static QState AO_System_initial(AO_System_t * const me, void const * const par);
static QState AO_System_normal(AO_System_t * const me, QEvt const * const e);
static QState AO_System_error(AO_System_t * const me, QEvt const * const e);
static QState AO_System_emergency_stop(AO_System_t * const me, QEvt const * const e);

// ==================== 构造函数 ====================

void AO_System_ctor(void) {
    AO_System_t *me = &AO_System_inst;
    
    // 调用QActive构造函数
    QActive_ctor(&me->super, Q_STATE_CAST(&AO_System_initial));
    
    // 构造时间事件
    QTimeEvt_ctorX(&me->led_timer, &me->super, LED_UPDATE_SIG, 0U);
    
    // 初始化状态数据
    me->error_count = 0;
    me->emergency_stop = false;
}

// ==================== 状态机实现 ====================

/**
 * @brief 初始状态
 */
static QState AO_System_initial(AO_System_t * const me, void const * const par) {
    (void)par;
    
    LOG_INFO("[AO-SYSTEM] Initial state\n");
    
    // 初始化LED为普通GPIO输出（不用PWM）
    gpio_init(PIN_LED_BUILTIN);
    gpio_set_dir(PIN_LED_BUILTIN, GPIO_OUT);
    gpio_put(PIN_LED_BUILTIN, 1);  // 初始点亮
    
    LOG_INFO("[AO-SYSTEM] LED initialized (GPIO mode)\n");
    
    // 启动LED定时器（1秒周期，简单闪烁）
    QTimeEvt_armX(&me->led_timer, 
                  1000,    // 首次触发时间
                  1000);   // 周期：1秒
    
    // 转换到normal状态
    return Q_TRAN(&AO_System_normal);
}

/**
 * @brief 正常状态
 */
static QState AO_System_normal(AO_System_t * const me, QEvt const * const e) {
    QState status;
    
    switch (e->sig) {
        case Q_ENTRY_SIG: {
            LOG_DEBUG("[AO-SYSTEM] Entering NORMAL state\n");
            system_set_state(SYSTEM_STATE_IDLE);
            status = Q_HANDLED();
            break;
        }
        
        case Q_EXIT_SIG: {
            status = Q_HANDLED();
            break;
        }
        
        case LED_UPDATE_SIG: {
            // LED简单闪烁（GPIO翻转）
            static bool led_state = false;
            static uint32_t led_count = 0;
            
            led_count++;
            
            // 翻转LED状态
            led_state = !led_state;
            gpio_put(PIN_LED_BUILTIN, led_state);
            
            // 每10次（10秒）打印一次心跳
            if (led_count % 10 == 0) {
                LOG_DEBUG("[AO-SYSTEM] Heartbeat #%lu\n", led_count);
            }
            status = Q_HANDLED();
            break;
        }
        
        case ERROR_SIG: {
            ErrorEvt const *evt = Q_EVT_CAST(ErrorEvt);
            me->error_count++;
            LOG_DEBUG("[AO-SYSTEM] Error received: code=%d, count=%lu\n", 
                   evt->error_code, me->error_count);
            
            // 根据错误严重程度决定是否转换状态
            if (evt->error_code >= 0xF0) {
                // 严重错误，转到错误状态
                status = Q_TRAN(&AO_System_error);
            } else {
                status = Q_HANDLED();
            }
            break;
        }
        
        case ESTOP_SIG: {
            LOG_DEBUG("[AO-SYSTEM] Emergency stop triggered!\n");
            status = Q_TRAN(&AO_System_emergency_stop);
            break;
        }
        
        case CMD_FLASH_SAVE_SIG: {
            LOG_DEBUG("[AO-SYSTEM] Saving parameters to Flash...\n");
            // 注意：Flash写入会短暂阻塞（<10ms），但这是硬件限制
            // RP2350 Flash写入时必须禁用中断，无法完全非阻塞
            param_manager_save();
            status = Q_HANDLED();
            break;
        }
        
        case CMD_FLASH_LOAD_SIG: {
            LOG_DEBUG("[AO-SYSTEM] Loading parameters from Flash...\n");
            param_manager_load();
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
 * @brief 错误状态
 */
static QState AO_System_error(AO_System_t * const me, QEvt const * const e) {
    QState status;
    
    switch (e->sig) {
        case Q_ENTRY_SIG: {
            LOG_DEBUG("[AO-SYSTEM] Entering ERROR state\n");
            system_set_state(SYSTEM_STATE_ERROR);
            
            // LED快速闪烁（错误指示）
            QTimeEvt_disarm(&me->led_timer);
            QTimeEvt_armX(&me->led_timer, 
                         LED_BLINK_ERROR_MS, 
                         LED_BLINK_ERROR_MS);
            status = Q_HANDLED();
            break;
        }
        
        case Q_EXIT_SIG: {
            // 恢复正常LED闪烁
            QTimeEvt_disarm(&me->led_timer);
            QTimeEvt_armX(&me->led_timer, 
                         TIME_EVENT_LED_MS, 
                         TIME_EVENT_LED_MS);
            status = Q_HANDLED();
            break;
        }
        
        case LED_UPDATE_SIG: {
            // LED快速闪烁（错误状态）
            static bool led_state = false;
            led_state = !led_state;
            gpio_put(PIN_LED_BUILTIN, led_state);
            
            status = Q_HANDLED();
            break;
        }
        
        case INIT_COMPLETE_SIG: {
            // 收到重新初始化完成信号，返回正常状态
            LOG_DEBUG("[AO-SYSTEM] Recovery, returning to NORMAL\n");
            status = Q_TRAN(&AO_System_normal);
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
 * @brief 紧急停止状态
 */
static QState AO_System_emergency_stop(AO_System_t * const me, QEvt const * const e) {
    QState status;
    
    switch (e->sig) {
        case Q_ENTRY_SIG: {
            LOG_DEBUG("[AO-SYSTEM] EMERGENCY STOP!\n");
            system_set_state(SYSTEM_STATE_EMERGENCY_STOP);
            emergency_stop_trigger();
            
            // 禁用所有舵机PWM
            pwm_emergency_stop();
            
            // LED超快闪烁
            QTimeEvt_disarm(&me->led_timer);
            QTimeEvt_armX(&me->led_timer, 50, 50);
            status = Q_HANDLED();
            break;
        }
        
        case Q_EXIT_SIG: {
            emergency_stop_clear();
            status = Q_HANDLED();
            break;
        }
        
        case LED_UPDATE_SIG: {
            // LED超快闪烁（紧急停止状态）
            static bool led_state = false;
            led_state = !led_state;
            gpio_put(PIN_LED_BUILTIN, led_state);
            
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


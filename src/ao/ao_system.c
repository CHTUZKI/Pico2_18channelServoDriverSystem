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
#include "hardware/pwm.h"
#include <stdio.h>

Q_DEFINE_THIS_FILE

// ==================== 调试宏 ====================
#if DEBUG_SYSTEM
    #define SYSTEM_DEBUG(...) printf(__VA_ARGS__)
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
    
    printf("[AO-SYSTEM] Initial state\n");
    
    // 初始化LED为PWM呼吸灯（使用已配置的PWM）
    // 注意：GPIO25在Slice 4，已经在pwm_init_all()中配置好了
    gpio_set_function(PIN_LED_BUILTIN, GPIO_FUNC_PWM);
    uint led_slice = pwm_gpio_to_slice_num(PIN_LED_BUILTIN);
    uint led_chan = pwm_gpio_to_channel(PIN_LED_BUILTIN);
    
    // 不重新配置PWM，直接使用舵机PWM的配置（2MHz, wrap=39999）
    pwm_set_chan_level(led_slice, led_chan, 0);  // 初始亮度0
    
    printf("[AO-SYSTEM] LED breathing mode initialized (PWM Slice %d Chan %c)\n", 
           led_slice, led_chan ? 'B' : 'A');
    
    // 启动LED定时器（50ms周期用于呼吸效果）
    QTimeEvt_armX(&me->led_timer, 
                  50,      // 首次触发时间
                  50);     // 周期：50ms
    
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
            SYSTEM_DEBUG("[AO-SYSTEM] Entering NORMAL state\n");
            system_set_state(SYSTEM_STATE_IDLE);
            status = Q_HANDLED();
            break;
        }
        
        case Q_EXIT_SIG: {
            status = Q_HANDLED();
            break;
        }
        
        case LED_UPDATE_SIG: {
            // LED呼吸效果（PWM调光）
            // 使用舵机PWM的wrap值：39999
            static uint16_t brightness = 0;
            static int16_t direction = 800;  // 亮度变化步长
            static uint32_t led_count = 0;
            
            led_count++;
            
            // 更新亮度
            brightness += direction;
            
            // 反转方向（范围：0-39999）
            if (brightness >= 39999 || (direction > 0 && brightness > 39999 - 800)) {
                brightness = 39999;
                direction = -800;
            } else if ((int16_t)brightness <= 0) {
                brightness = 0;
                direction = 800;
            }
            
            // 设置PWM亮度
            uint led_slice = pwm_gpio_to_slice_num(PIN_LED_BUILTIN);
            uint led_chan = pwm_gpio_to_channel(PIN_LED_BUILTIN);
            pwm_set_chan_level(led_slice, led_chan, brightness);
            
            // 每200次（10秒）打印一次心跳
            if (led_count % 200 == 0) {
                SYSTEM_DEBUG("[AO-SYSTEM] Breathing #%lu, brightness=%u\n", led_count, brightness);
            }
            status = Q_HANDLED();
            break;
        }
        
        case ERROR_SIG: {
            ErrorEvt const *evt = Q_EVT_CAST(ErrorEvt);
            me->error_count++;
            SYSTEM_DEBUG("[AO-SYSTEM] Error received: code=%d, count=%lu\n", 
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
            SYSTEM_DEBUG("[AO-SYSTEM] Emergency stop triggered!\n");
            status = Q_TRAN(&AO_System_emergency_stop);
            break;
        }
        
        case CMD_FLASH_SAVE_SIG: {
            SYSTEM_DEBUG("[AO-SYSTEM] Saving parameters to Flash...\n");
            // 注意：Flash写入会短暂阻塞（<10ms），但这是硬件限制
            // RP2350 Flash写入时必须禁用中断，无法完全非阻塞
            param_manager_save();
            status = Q_HANDLED();
            break;
        }
        
        case CMD_FLASH_LOAD_SIG: {
            SYSTEM_DEBUG("[AO-SYSTEM] Loading parameters from Flash...\n");
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
            SYSTEM_DEBUG("[AO-SYSTEM] Entering ERROR state\n");
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
            // LED快速闪烁（错误状态用传统方式）
            static bool led_state = false;
            led_state = !led_state;
            
            // 临时切换回GPIO模式来闪烁
            gpio_set_function(PIN_LED_BUILTIN, GPIO_FUNC_SIO);
            gpio_set_dir(PIN_LED_BUILTIN, GPIO_OUT);
            gpio_put(PIN_LED_BUILTIN, led_state);
            
            status = Q_HANDLED();
            break;
        }
        
        case INIT_COMPLETE_SIG: {
            // 收到重新初始化完成信号，返回正常状态
            SYSTEM_DEBUG("[AO-SYSTEM] Recovery, returning to NORMAL\n");
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
            SYSTEM_DEBUG("[AO-SYSTEM] EMERGENCY STOP!\n");
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
            // LED快速闪烁（紧急停止状态）
            static bool led_state = false;
            led_state = !led_state;
            
            // 临时切换回GPIO模式来闪烁
            gpio_set_function(PIN_LED_BUILTIN, GPIO_FUNC_SIO);
            gpio_set_dir(PIN_LED_BUILTIN, GPIO_OUT);
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


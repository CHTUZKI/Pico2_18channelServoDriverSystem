/**
 * @file qp_port_init.c
 * @brief QP/C端口初始化和回调实现
 * @date 2025-10-23
 */

#include "qpc.h"
#include "pico/stdlib.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include "config/pinout.h"
#include "events/events.h"
#include <stdio.h>

// 外部主动对象声明（稍后定义）
extern QActive * const AO_Communication;
extern QActive * const AO_Motion;
extern QActive * const AO_System;

// ==================== SysTick配置 ====================

// 调试计数器（全局，避免在中断中使用printf）
static volatile uint32_t g_systick_count = 0;

// SysTick定时器回调
static bool systick_callback(struct repeating_timer *t) {
    (void)t;
    
    // 增加计数器（用于外部监控）
    g_systick_count++;
    
    // QP时间事件Tick（1ms）
    QF_TICK_X(0, (void *)0);
    
    return true;  // 继续触发
}

static struct repeating_timer systick_timer;

// ==================== QP/C回调函数 ====================

/**
 * @brief QP启动回调
 * @note 在QF_run()之前调用，用于启动硬件定时器
 */
void QF_onStartup(void) {
    printf("[QP] QF_onStartup called.\n");
    
    // 配置SysTick为1ms周期
    // 使用Pico SDK的repeating timer
    add_repeating_timer_ms(-1,  // -1表示每1ms触发一次
                          systick_callback,
                          NULL,
                          &systick_timer);
    
    printf("[QP] SysTick configured (1ms period).\n");
}

/**
 * @brief QP清理回调
 */
void QF_onCleanup(void) {
    printf("[QP] QF_onCleanup called.\n");
    cancel_repeating_timer(&systick_timer);
}

/**
 * @brief QV空闲回调
 * @note 当没有事件需要处理时调用
 */
void QV_onIdle(void) {
    // **关键**: QV_onIdle()被调用时中断是禁用的
    // 必须在这里启用中断，否则时间事件不会触发！
    QF_INT_ENABLE();
    
    // 进入低功耗模式等待中断
    // WFI会在任何中断（如SysTick）时自动唤醒
    __asm volatile ("wfi");
}

/**
 * @brief QP错误回调
 * @param module 模块名
 * @param id 错误ID
 * @note 发生严重错误时调用
 */
Q_NORETURN Q_onError(char const * const module, int_t const id) {
    // 禁用中断
    QF_INT_DISABLE();
    
    // 打印错误信息（如果USB可用）
    printf("\n[QP-ERROR] Module: %s, ID: %d\n", module, id);
    
    // 进入死循环，快速闪烁LED
    while (1) {
        gpio_put(PIN_LED_BUILTIN, 1);
        for (volatile int i = 0; i < 500000; i++);
        gpio_put(PIN_LED_BUILTIN, 0);
        for (volatile int i = 0; i < 500000; i++);
    }
}

// ==================== 断言定义 ====================

// 在qp_port.h中定义的Q_onAssert已废弃，使用Q_onError


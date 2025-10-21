/**
 * @file task_pwm.c
 * @brief PWM更新任务实现（Core 1专用）
 * @date 2025-10-21
 */

#include "tasks/task_pwm.h"
#include "pwm/pwm_driver.h"
#include "utils/error_handler.h"
#include "config/config.h"
#include <FreeRTOS.h>
#include <task.h>

// 任务句柄
static TaskHandle_t g_pwm_task_handle = NULL;

void task_pwm_func(void *pvParameters) {
    (void)pvParameters;
    
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(PWM_PERIOD_MS);  // 20ms周期
    
    while (1) {
        // PWM更新（由硬件自动完成，这里主要做监控）
        // 实际的PWM更新在servo_set_angle调用时已经完成
        
        // LED状态更新
        error_led_update();
        
        // 等待下一个周期
        vTaskDelayUntil(&last_wake_time, period);
    }
}

BaseType_t task_pwm_create(void) {
    // 创建任务并绑定到Core 1
    BaseType_t ret = xTaskCreate(
        task_pwm_func,
        "PWMTask",
        TASK_STACK_SIZE_PWM,
        NULL,
        TASK_PRIORITY_PWM,
        &g_pwm_task_handle
    );
    
    // 设置任务亲和性到Core 1
    if (ret == pdPASS) {
        vTaskCoreAffinitySet(g_pwm_task_handle, (1 << 1));  // Core 1
    }
    
    return ret;
}


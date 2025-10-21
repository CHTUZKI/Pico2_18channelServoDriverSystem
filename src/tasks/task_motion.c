/**
 * @file task_motion.c
 * @brief 运动控制任务实现
 * @date 2025-10-21
 */

#include "tasks/task_motion.h"
#include "motion/interpolation.h"
#include "servo/servo_control.h"
#include "utils/error_handler.h"
#include "config/config.h"
#include <FreeRTOS.h>
#include <task.h>

// 任务句柄
static TaskHandle_t g_motion_task_handle = NULL;

// 多轴插值器
static multi_axis_interpolator_t g_interpolator;

void task_motion_func(void *pvParameters) {
    (void)pvParameters;
    
    // 初始化插值器
    multi_interpolator_init(&g_interpolator);
    
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(INTERPOLATION_PERIOD_MS);  // 20ms周期
    
    float output_positions[SERVO_COUNT];
    
    while (1) {
        // 检查紧急停止
        if (is_emergency_stopped()) {
            // 紧急停止状态，不执行插值
            vTaskDelayUntil(&last_wake_time, period);
            continue;
        }
        
        // 更新插值器
        multi_interpolator_update(&g_interpolator, INTERPOLATION_PERIOD_MS, output_positions);
        
        // 应用到舵机（这里可以直接更新PWM，因为已经在servo层做过限位检查）
        // 实际应用中，可以通过队列发送到PWM任务
        
        // 检查是否全部到达
        if (multi_interpolator_all_reached(&g_interpolator)) {
            system_set_state(SYSTEM_STATE_IDLE);
        } else {
            system_set_state(SYSTEM_STATE_MOVING);
        }
        
        // 等待下一个周期
        vTaskDelayUntil(&last_wake_time, period);
    }
}

BaseType_t task_motion_create(void) {
    BaseType_t ret = xTaskCreate(
        task_motion_func,
        "MotionTask",
        TASK_STACK_SIZE_MOTION,
        NULL,
        TASK_PRIORITY_MOTION,
        &g_motion_task_handle
    );
    
    return ret;
}


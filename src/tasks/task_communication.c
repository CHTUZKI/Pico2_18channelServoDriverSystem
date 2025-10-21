/**
 * @file task_communication.c
 * @brief 通信任务实现
 * @date 2025-10-21
 */

#include "tasks/task_communication.h"
#include "communication/usb_handler.h"
#include "utils/error_handler.h"
#include "config/config.h"
#include <FreeRTOS.h>
#include <task.h>

// 任务句柄
static TaskHandle_t g_comm_task_handle = NULL;

void task_communication_func(void *pvParameters) {
    (void)pvParameters;
    
    // 初始化USB通信
    usb_handler_init();
    
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(10);  // 10ms周期
    
    while (1) {
        // 处理USB通信
        usb_handler_process();
        
        // 等待下一个周期
        vTaskDelayUntil(&last_wake_time, period);
    }
}

BaseType_t task_communication_create(void) {
    BaseType_t ret = xTaskCreate(
        task_communication_func,
        "CommTask",
        TASK_STACK_SIZE_COMM,
        NULL,
        TASK_PRIORITY_COMM,
        &g_comm_task_handle
    );
    
    return ret;
}


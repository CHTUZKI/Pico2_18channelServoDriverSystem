/**
 * @file task_communication.h
 * @brief 通信任务（Core 0）
 * @date 2025-10-21
 */

#ifndef TASK_COMMUNICATION_H
#define TASK_COMMUNICATION_H

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief 创建通信任务
 * @return pdPASS 成功, pdFAIL 失败
 */
BaseType_t task_communication_create(void);

/**
 * @brief 通信任务函数
 * @param pvParameters 参数指针
 */
void task_communication_func(void *pvParameters);

#endif // TASK_COMMUNICATION_H


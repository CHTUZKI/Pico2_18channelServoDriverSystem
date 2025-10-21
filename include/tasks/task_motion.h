/**
 * @file task_motion.h
 * @brief 运动控制任务（Core 0）
 * @date 2025-10-21
 */

#ifndef TASK_MOTION_H
#define TASK_MOTION_H

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief 创建运动控制任务
 * @return pdPASS 成功, pdFAIL 失败
 */
BaseType_t task_motion_create(void);

/**
 * @brief 运动控制任务函数
 * @param pvParameters 参数指针
 */
void task_motion_func(void *pvParameters);

#endif // TASK_MOTION_H


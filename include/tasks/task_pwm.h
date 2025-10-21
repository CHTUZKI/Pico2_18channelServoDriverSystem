/**
 * @file task_pwm.h
 * @brief PWM更新任务（Core 1）
 * @date 2025-10-21
 */

#ifndef TASK_PWM_H
#define TASK_PWM_H

#include "FreeRTOS.h"
#include "task.h"

/**
 * @brief 创建PWM任务（Core 1）
 * @return pdPASS 成功, pdFAIL 失败
 */
BaseType_t task_pwm_create(void);

/**
 * @brief PWM任务函数
 * @param pvParameters 参数指针
 */
void task_pwm_func(void *pvParameters);

#endif // TASK_PWM_H


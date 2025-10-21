/**
 * @file FreeRTOSConfig.h
 * @brief FreeRTOS配置文件（RP2350双核SMP）
 * @date 2025-10-21
 * 
 * 基于FreeRTOS官方RP2040/RP2350 SMP移植层
 */

#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include <stdint.h>

// Include Pico SDK headers needed by FreeRTOS port
#include "hardware/structs/sio.h"
#include "hardware/regs/intctrl.h"

// =============== RP2350双核SMP配置 ===============
// 这些配置对应FreeRTOS SMP版本
#define configNUMBER_OF_CORES                   2
#define configUSE_CORE_AFFINITY                 1
#define configRUN_MULTIPLE_PRIORITIES           1
#define configUSE_PASSIVE_IDLE_HOOK             0

// RP2350架构配置
#define configENABLE_MPU                        0
#define configENABLE_TRUSTZONE                  0
#define configENABLE_FPU                        1

// 基本配置
#define configUSE_PREEMPTION                    1
#define configUSE_TICKLESS_IDLE                 0
#define configCPU_CLOCK_HZ                      150000000  // 150MHz
#define configTICK_RATE_HZ                      1000       // 1ms tick
#define configMAX_PRIORITIES                    5
#define configMINIMAL_STACK_SIZE                256
#define configMAX_TASK_NAME_LEN                 16
#define configUSE_16_BIT_TICKS                  0
#define configIDLE_SHOULD_YIELD                 1
#define configUSE_TASK_NOTIFICATIONS            1
#define configTASK_NOTIFICATION_ARRAY_ENTRIES   3
#define configUSE_MUTEXES                       1
#define configUSE_RECURSIVE_MUTEXES             1
#define configUSE_COUNTING_SEMAPHORES           1
#define configUSE_ALTERNATIVE_API               0
#define configQUEUE_REGISTRY_SIZE               10
#define configUSE_QUEUE_SETS                    0
#define configUSE_TIME_SLICING                  1
#define configUSE_NEWLIB_REENTRANT              0
#define configENABLE_BACKWARD_COMPATIBILITY     0
#define configSTACK_DEPTH_TYPE                  uint32_t
#define configMESSAGE_BUFFER_LENGTH_TYPE        size_t

// 内存分配
#define configSUPPORT_STATIC_ALLOCATION         0
#define configSUPPORT_DYNAMIC_ALLOCATION        1
#define configTOTAL_HEAP_SIZE                   (128 * 1024)  // 128KB堆
#define configAPPLICATION_ALLOCATED_HEAP        0

// 钩子函数
#define configUSE_IDLE_HOOK                     1
#define configUSE_TICK_HOOK                     1
#define configCHECK_FOR_STACK_OVERFLOW          2
#define configUSE_MALLOC_FAILED_HOOK            1
#define configUSE_DAEMON_TASK_STARTUP_HOOK      0

// 运行时统计
#define configGENERATE_RUN_TIME_STATS           0
#define configUSE_TRACE_FACILITY                1
#define configUSE_STATS_FORMATTING_FUNCTIONS    0

// 协程
#define configUSE_CO_ROUTINES                   0
#define configMAX_CO_ROUTINE_PRIORITIES         2

// 软件定时器
#define configUSE_TIMERS                        1
#define configTIMER_TASK_PRIORITY               (configMAX_PRIORITIES - 1)
#define configTIMER_QUEUE_LENGTH                10
#define configTIMER_TASK_STACK_DEPTH            configMINIMAL_STACK_SIZE * 2

// 可选功能
#define INCLUDE_vTaskPrioritySet                1
#define INCLUDE_uxTaskPriorityGet               1
#define INCLUDE_vTaskDelete                     1
#define INCLUDE_vTaskCleanUpResources           0
#define INCLUDE_vTaskSuspend                    1
#define INCLUDE_vTaskDelayUntil                 1
#define INCLUDE_vTaskDelay                      1
#define INCLUDE_xTaskGetSchedulerState          1
#define INCLUDE_xTaskGetCurrentTaskHandle       1
#define INCLUDE_uxTaskGetStackHighWaterMark     1
#define INCLUDE_xTaskGetIdleTaskHandle          0
#define INCLUDE_eTaskGetState                   1
#define INCLUDE_xEventGroupSetBitFromISR        1
#define INCLUDE_xTimerPendFunctionCall          1
#define INCLUDE_xTaskAbortDelay                 0
#define INCLUDE_xTaskGetHandle                  0
#define INCLUDE_xTaskResumeFromISR              1

// 中断优先级（RP2350要求）
#define configKERNEL_INTERRUPT_PRIORITY         255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY    16

// RP2350 Pico SDK集成
#define configSUPPORT_PICO_SYNC_INTEROP         1
#define configSUPPORT_PICO_TIME_INTEROP         1

// SMP双核自旋锁配置（RP2040/RP2350必需）
#define configSMP_SPINLOCK_0                    0
#define configSMP_SPINLOCK_1                    1

// 断言
extern void vAssertCalled(const char *const pcFileName, unsigned long ulLine);
#define configASSERT(x) if((x) == 0) vAssertCalled(__FILE__, __LINE__)

#endif // FREERTOS_CONFIG_H


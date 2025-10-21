/**
 * @file error_handler.h
 * @brief 错误处理和状态管理
 * @date 2025-10-21
 */

#ifndef ERROR_HANDLER_H
#define ERROR_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 系统错误码定义
 */
typedef enum {
    ERROR_NONE = 0x00,              // 无错误
    
    // 通信错误 (0x10-0x1F)
    ERROR_COMM_TIMEOUT = 0x10,      // 通信超时
    ERROR_COMM_CRC = 0x11,          // CRC校验错误
    ERROR_COMM_FRAME = 0x12,        // 帧格式错误
    ERROR_COMM_OVERFLOW = 0x13,     // 缓冲区溢出
    
    // 命令错误 (0x20-0x2F)
    ERROR_CMD_INVALID = 0x20,       // 无效命令
    ERROR_CMD_PARAM = 0x21,         // 参数错误
    ERROR_CMD_LENGTH = 0x22,        // 长度错误
    ERROR_CMD_ID = 0x23,            // ID错误
    
    // 舵机错误 (0x30-0x3F)
    ERROR_SERVO_LIMIT = 0x30,       // 超出限位
    ERROR_SERVO_ANGLE = 0x31,       // 角度无效
    ERROR_SERVO_DISABLED = 0x32,    // 舵机未使能
    ERROR_SERVO_BUSY = 0x33,        // 舵机繁忙
    
    // Flash错误 (0x40-0x4F)
    ERROR_FLASH_READ = 0x40,        // Flash读取错误
    ERROR_FLASH_WRITE = 0x41,       // Flash写入错误
    ERROR_FLASH_ERASE = 0x42,       // Flash擦除错误
    ERROR_FLASH_VERSION = 0x43,     // 版本不匹配
    
    // 系统错误 (0xF0-0xFF)
    ERROR_SYSTEM_INIT = 0xF0,       // 初始化失败
    ERROR_SYSTEM_TASK = 0xF1,       // 任务错误
    ERROR_SYSTEM_MEMORY = 0xF2,     // 内存错误
    ERROR_EMERGENCY_STOP = 0xFF     // 紧急停止
} error_code_t;

/**
 * @brief 系统状态定义
 */
typedef enum {
    SYSTEM_STATE_IDLE = 0,          // 空闲
    SYSTEM_STATE_RUNNING,           // 运行中
    SYSTEM_STATE_MOVING,            // 运动中
    SYSTEM_STATE_ERROR,             // 错误状态
    SYSTEM_STATE_EMERGENCY_STOP     // 紧急停止
} system_state_t;

/**
 * @brief 错误状态结构体
 */
typedef struct {
    error_code_t last_error;        // 最后一次错误
    uint32_t error_count;           // 错误计数
    uint32_t comm_error_count;      // 通信错误计数
    uint32_t crc_error_count;       // CRC错误计数
    system_state_t state;           // 系统状态
    bool emergency_stop;            // 急停标志
} error_status_t;

/**
 * @brief 初始化错误处理模块
 */
void error_handler_init(void);

/**
 * @brief 设置错误状态
 * @param error 错误码
 */
void error_set(error_code_t error);

/**
 * @brief 清除错误状态
 */
void error_clear(void);

/**
 * @brief 获取最后一次错误
 * @return 错误码
 */
error_code_t error_get_last(void);

/**
 * @brief 获取错误状态
 * @return 错误状态指针
 */
const error_status_t* error_get_status(void);

/**
 * @brief 设置系统状态
 * @param state 系统状态
 */
void system_set_state(system_state_t state);

/**
 * @brief 获取系统状态
 * @return 系统状态
 */
system_state_t system_get_state(void);

/**
 * @brief 触发紧急停止
 */
void emergency_stop_trigger(void);

/**
 * @brief 清除紧急停止
 */
void emergency_stop_clear(void);

/**
 * @brief 检查是否处于紧急停止状态
 * @return true 紧急停止, false 正常
 */
bool is_emergency_stopped(void);

/**
 * @brief LED指示更新（根据状态闪烁）
 */
void error_led_update(void);

#endif // ERROR_HANDLER_H


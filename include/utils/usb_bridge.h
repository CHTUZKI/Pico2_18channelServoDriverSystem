/**
 * @file usb_bridge.h
 * @brief USB桥接器 - Core1独占串口收发，Core0通过共享内存通信
 * @date 2025-10-24
 * 
 * 架构说明：
 * - Core 1: 完全负责USB CDC收发，避免双核串口冲突
 * - Core 0: 通过环形缓冲区与Core 1通信
 * - 双向通信：TX缓冲区（Core0->Core1->USB）、RX缓冲区（USB->Core1->Core0）
 */

#ifndef USB_BRIDGE_H
#define USB_BRIDGE_H

#include <stdint.h>
#include <stdbool.h>
#include "pico/time.h"  // 提供time_us_32()函数

// ==================== 配置参数 ====================

#define USB_BRIDGE_TX_BUFFER_SIZE   2048    // 发送缓冲区大小（Core0->Core1）
#define USB_BRIDGE_RX_BUFFER_SIZE   512     // 接收缓冲区大小（Core1->Core0）
#define USB_BRIDGE_MAX_PACKET       256     // 单个数据包最大长度

// ==================== API函数 ====================

/**
 * @brief 初始化USB桥接器
 * 必须在Core 0上调用，且在启动Core 1之前
 */
void usb_bridge_init(void);

/**
 * @brief 启动Core 1的USB处理线程
 * 这会启动Core 1，负责：
 * 1. 从TX缓冲区读取并通过USB发送
 * 2. 从USB接收并写入RX缓冲区
 * 3. PWM监控输出
 */
void usb_bridge_start_core1(void);

// ==================== 发送接口 (Core 0 使用) ====================

/**
 * @brief 发送格式化字符串到USB（类似printf）
 * 
 * @param fmt 格式字符串
 * @param ... 可变参数
 * @return 写入的字节数，-1表示缓冲区满
 * 
 * @note 非阻塞，如果缓冲区满则返回-1
 */
int usb_bridge_printf(const char *fmt, ...);

/**
 * @brief 发送原始数据到USB
 * 
 * @param data 数据指针
 * @param len 数据长度
 * @return 实际写入的字节数
 */
int usb_bridge_write(const uint8_t *data, uint16_t len);

/**
 * @brief 发送字符串到USB
 * 
 * @param str 字符串指针
 * @return 实际写入的字节数
 */
int usb_bridge_puts(const char *str);

/**
 * @brief 刷新发送缓冲区（等待所有数据发送完成）
 * 
 * @param timeout_ms 超时时间（毫秒），0表示立即返回
 * @return true 成功刷新，false 超时
 */
bool usb_bridge_flush(uint32_t timeout_ms);

// ==================== 接收接口 (Core 0 使用) ====================

/**
 * @brief 检查是否有可读数据
 * @return 可读字节数
 */
uint32_t usb_bridge_available(void);

/**
 * @brief 从USB读取数据
 * 
 * @param buffer 缓冲区
 * @param len 要读取的字节数
 * @return 实际读取的字节数
 */
int usb_bridge_read(uint8_t *buffer, uint16_t len);

/**
 * @brief 读取一个字节
 * @return 读取的字节，-1表示无数据
 */
int usb_bridge_getchar(void);

/**
 * @brief 读取一行（直到换行符）
 * 
 * @param buffer 缓冲区
 * @param max_len 最大长度
 * @return 实际读取的字节数，-1表示无数据
 */
int usb_bridge_readline(char *buffer, uint16_t max_len);

/**
 * @brief 清空接收缓冲区
 */
void usb_bridge_clear_rx(void);

// ==================== 统计信息 ====================

typedef struct {
    uint32_t tx_bytes;          // 总共发送的字节数
    uint32_t rx_bytes;          // 总共接收的字节数
    uint32_t tx_overflow;       // 发送缓冲区溢出次数
    uint32_t rx_overflow;       // 接收缓冲区溢出次数
    uint32_t tx_available;      // 发送缓冲区可用空间
    uint32_t rx_available;      // 接收缓冲区可用数据
} usb_bridge_stats_t;

/**
 * @brief 获取统计信息
 */
void usb_bridge_get_stats(usb_bridge_stats_t *stats);

// ==================== 便捷宏定义 ====================

// 替换printf的宏（推荐使用）
#define USB_PRINTF(fmt, ...)    usb_bridge_printf(fmt, ##__VA_ARGS__)

// 日志级别宏（带时间戳和级别标记）
#if DEBUG_ENABLE
    #define LOG_DEBUG(fmt, ...)     usb_bridge_printf("[DEBUG][%lu] " fmt, time_us_32()/1000, ##__VA_ARGS__)
    #define LOG_INFO(fmt, ...)      usb_bridge_printf("[INFO][%lu] " fmt, time_us_32()/1000, ##__VA_ARGS__)
    #define LOG_WARN(fmt, ...)      usb_bridge_printf("[WARN][%lu] " fmt, time_us_32()/1000, ##__VA_ARGS__)
    #define LOG_ERROR(fmt, ...)     usb_bridge_printf("[ERROR][%lu] " fmt, time_us_32()/1000, ##__VA_ARGS__)
    #define LOG_CRITICAL(fmt, ...)  usb_bridge_printf("[CRITICAL][%lu] " fmt, time_us_32()/1000, ##__VA_ARGS__)
#else
    // 发布版本只保留错误和关键日志
    #define LOG_DEBUG(fmt, ...)     ((void)0)
    #define LOG_INFO(fmt, ...)      ((void)0)
    #define LOG_WARN(fmt, ...)      ((void)0)
    #define LOG_ERROR(fmt, ...)     usb_bridge_printf("[ERROR] " fmt, ##__VA_ARGS__)
    #define LOG_CRITICAL(fmt, ...)  usb_bridge_printf("[CRITICAL] " fmt, ##__VA_ARGS__)
#endif

#endif // USB_BRIDGE_H


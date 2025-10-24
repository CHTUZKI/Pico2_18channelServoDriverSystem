/**
 * @file usb_bridge.c
 * @brief USB桥接器实现 - 使用无锁环形缓冲区实现高效双核通信
 * @date 2025-10-24
 * 
 * 技术要点：
 * 1. 无锁环形缓冲区（Lock-free Ring Buffer）
 * 2. 单生产者单消费者模型（SPSC）
 * 3. 内存屏障保证可见性
 * 4. Core 1独占USB串口
 */

#include "utils/usb_bridge.h"
#include "monitor/gpio_monitor.h"
#include "config/config.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/time.h"
#include "hardware/pwm.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

// ==================== 无锁环形缓冲区 ====================

typedef struct {
    uint8_t buffer[USB_BRIDGE_TX_BUFFER_SIZE];
    volatile uint32_t head;     // 写入位置（生产者：Core 0）
    volatile uint32_t tail;     // 读取位置（消费者：Core 1）
} ring_buffer_tx_t;

typedef struct {
    uint8_t buffer[USB_BRIDGE_RX_BUFFER_SIZE];
    volatile uint32_t head;     // 写入位置（生产者：Core 1）
    volatile uint32_t tail;     // 读取位置（消费者：Core 0）
} ring_buffer_rx_t;

// 全局缓冲区实例
static ring_buffer_tx_t tx_buffer;  // Core 0 写，Core 1 读
static ring_buffer_rx_t rx_buffer;  // Core 1 写，Core 0 读

// 统计信息
static volatile usb_bridge_stats_t stats = {0};

// ==================== 内存屏障宏 ====================

// 确保写入操作对其他核心可见
#define MEMORY_BARRIER() __dmb()

// ==================== TX缓冲区操作（发送：Core0 -> Core1 -> USB）====================

/**
 * @brief 获取TX缓冲区可用空间
 */
static inline uint32_t tx_buffer_available_space(void) {
    uint32_t head = tx_buffer.head;
    uint32_t tail = tx_buffer.tail;
    
    if (head >= tail) {
        return USB_BRIDGE_TX_BUFFER_SIZE - (head - tail) - 1;
    } else {
        return tail - head - 1;
    }
}

/**
 * @brief 获取TX缓冲区已用空间
 */
static inline uint32_t tx_buffer_used_space(void) {
    uint32_t head = tx_buffer.head;
    uint32_t tail = tx_buffer.tail;
    
    if (head >= tail) {
        return head - tail;
    } else {
        return USB_BRIDGE_TX_BUFFER_SIZE - (tail - head);
    }
}

/**
 * @brief 写入TX缓冲区（Core 0调用）
 */
static int tx_buffer_write(const uint8_t *data, uint16_t len) {
    uint32_t available = tx_buffer_available_space();
    if (len > available) {
        stats.tx_overflow++;
        len = available;  // 部分写入
    }
    
    if (len == 0) return 0;
    
    uint32_t head = tx_buffer.head;
    
    // 写入数据（可能需要环绕）
    for (uint16_t i = 0; i < len; i++) {
        tx_buffer.buffer[head] = data[i];
        head = (head + 1) % USB_BRIDGE_TX_BUFFER_SIZE;
    }
    
    // 内存屏障：确保数据写入完成后再更新head
    MEMORY_BARRIER();
    tx_buffer.head = head;
    
    stats.tx_bytes += len;
    return len;
}

/**
 * @brief 从TX缓冲区读取（Core 1调用）
 */
static int tx_buffer_read(uint8_t *buffer, uint16_t max_len) {
    uint32_t used = tx_buffer_used_space();
    uint16_t len = (max_len < used) ? max_len : used;
    
    if (len == 0) return 0;
    
    uint32_t tail = tx_buffer.tail;
    
    // 读取数据
    for (uint16_t i = 0; i < len; i++) {
        buffer[i] = tx_buffer.buffer[tail];
        tail = (tail + 1) % USB_BRIDGE_TX_BUFFER_SIZE;
    }
    
    // 内存屏障：确保数据读取完成后再更新tail
    MEMORY_BARRIER();
    tx_buffer.tail = tail;
    
    return len;
}

// ==================== RX缓冲区操作（接收：USB -> Core1 -> Core0）====================

/**
 * @brief 获取RX缓冲区可用空间
 */
static inline uint32_t rx_buffer_available_space(void) {
    uint32_t head = rx_buffer.head;
    uint32_t tail = rx_buffer.tail;
    
    if (head >= tail) {
        return USB_BRIDGE_RX_BUFFER_SIZE - (head - tail) - 1;
    } else {
        return tail - head - 1;
    }
}

/**
 * @brief 获取RX缓冲区已用空间
 */
static inline uint32_t rx_buffer_used_space(void) {
    uint32_t head = rx_buffer.head;
    uint32_t tail = rx_buffer.tail;
    
    if (head >= tail) {
        return head - tail;
    } else {
        return USB_BRIDGE_RX_BUFFER_SIZE - (tail - head);
    }
}

/**
 * @brief 写入RX缓冲区（Core 1调用）
 */
static int rx_buffer_write(const uint8_t *data, uint16_t len) {
    uint32_t available = rx_buffer_available_space();
    if (len > available) {
        stats.rx_overflow++;
        len = available;
    }
    
    if (len == 0) return 0;
    
    uint32_t head = rx_buffer.head;
    
    for (uint16_t i = 0; i < len; i++) {
        rx_buffer.buffer[head] = data[i];
        head = (head + 1) % USB_BRIDGE_RX_BUFFER_SIZE;
    }
    
    MEMORY_BARRIER();
    rx_buffer.head = head;
    
    stats.rx_bytes += len;
    return len;
}

/**
 * @brief 从RX缓冲区读取（Core 0调用）
 */
static int rx_buffer_read(uint8_t *buffer, uint16_t max_len) {
    uint32_t used = rx_buffer_used_space();
    uint16_t len = (max_len < used) ? max_len : used;
    
    if (len == 0) return 0;
    
    uint32_t tail = rx_buffer.tail;
    
    for (uint16_t i = 0; i < len; i++) {
        buffer[i] = rx_buffer.buffer[tail];
        tail = (tail + 1) % USB_BRIDGE_RX_BUFFER_SIZE;
    }
    
    MEMORY_BARRIER();
    rx_buffer.tail = tail;
    
    return len;
}

// ==================== Core 1主循环 ====================

/**
 * @brief Core 1主循环：USB收发 + PWM监控
 */
static void usb_bridge_core1_main(void) {
    // ========== Core 1接管USB操作 ==========
    // USB已经在Core 0初始化（枚举完成）
    // Core 1只负责收发数据
    
    // 短暂延迟确保Core 0初始化完成
    sleep_ms(100);
    
    // Core 1启动消息（直接printf，此时USB已就绪）
    printf("\n========================================\n");
    printf("[CORE1] USB Bridge Started\n");
    printf("[CORE1] Taking over USB I/O operations\n");
    printf("[CORE1] Core 0 -> TX Buffer -> Core 1 -> USB\n");
    printf("[CORE1] USB -> Core 1 -> RX Buffer -> Core 0\n");
    printf("[CORE1] PWM monitoring enabled\n");
    printf("========================================\n\n");
    
    uint8_t tx_temp[256];  // 临时缓冲区
    uint8_t rx_temp[128];
    
    uint32_t last_pwm_report = 0;
    const uint32_t PWM_REPORT_INTERVAL_MS = 2000;
    uint32_t pwm_report_count = 0;
    
    while (true) {
        // ========== 0. 处理USB任务（必须定期调用）==========
        // TinyUSB需要定期调用tud_task()来处理USB事件
        tight_loop_contents();  // 让watchdog知道我们还活着
        
        // ========== 1. 处理发送：TX缓冲区 -> USB ==========
        int tx_len = tx_buffer_read(tx_temp, sizeof(tx_temp));
        if (tx_len > 0) {
            // 直接输出到USB（Core 1独占）
            fwrite(tx_temp, 1, tx_len, stdout);
            fflush(stdout);  // 立即刷新
        }
        
        // ========== 2. 处理接收：USB -> RX缓冲区 ==========
        int ch;
        int rx_count = 0;
        while ((ch = getchar_timeout_us(0)) != PICO_ERROR_TIMEOUT && rx_count < sizeof(rx_temp)) {
            rx_temp[rx_count++] = (uint8_t)ch;
        }
        
        if (rx_count > 0) {
            rx_buffer_write(rx_temp, rx_count);
        }
        
        // ========== 3. PWM监控输出（直接printf）==========
#if ENABLE_GPIO_MONITOR
        uint32_t current_time = time_us_32() / 1000;
        if (current_time - last_pwm_report > PWM_REPORT_INTERVAL_MS) {
            printf("\n=== [CORE1-PWM] Monitor Report #%lu ===\n", pwm_report_count++);
            
            static const uint8_t MONITOR_PINS[] = {
                0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17
            };
            
            for (int i = 0; i < 18; i++) {
                uint gpio = MONITOR_PINS[i];
                uint slice = pwm_gpio_to_slice_num(gpio);
                uint chan = pwm_gpio_to_channel(gpio);
                
                uint32_t cc_reg = pwm_hw->slice[slice].cc;
                uint16_t level = (chan == PWM_CHAN_A) ? (cc_reg & 0xFFFF) : (cc_reg >> 16);
                uint16_t wrap = pwm_hw->slice[slice].top;
                bool enabled = (pwm_hw->slice[slice].csr & PWM_CH0_CSR_EN_BITS) != 0;
                
                uint32_t pulse_us = (level + 1) / 2;
                
                printf("  GPIO%02d S%dC%c: L=%5d W=%5d PW=%4luus %s\n",
                       gpio, slice, chan ? 'B' : 'A',
                       level, wrap, pulse_us,
                       enabled ? "EN" : "DIS");
            }
            
            printf("=================================\n\n");
            fflush(stdout);
            last_pwm_report = current_time;
        }
#endif
        
        // 非常短的延迟，保持高响应性
        // 不要sleep太久，否则USB响应会变慢
        busy_wait_us(50);  // 50微秒忙等待
    }
}

// ==================== API实现 ====================

/**
 * @brief 初始化USB桥接器
 */
void usb_bridge_init(void) {
    // 清空缓冲区
    memset(&tx_buffer, 0, sizeof(tx_buffer));
    memset(&rx_buffer, 0, sizeof(rx_buffer));
    memset((void*)&stats, 0, sizeof(stats));
}

/**
 * @brief 启动Core 1
 */
void usb_bridge_start_core1(void) {
    multicore_launch_core1(usb_bridge_core1_main);
}

// ==================== 发送接口（Core 0使用）====================

/**
 * @brief printf风格输出
 */
int usb_bridge_printf(const char *fmt, ...) {
    char buffer[256];
    
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    
    if (len >= sizeof(buffer)) {
        len = sizeof(buffer) - 1;
    }
    
    return tx_buffer_write((uint8_t*)buffer, len);
}

/**
 * @brief 写入原始数据
 */
int usb_bridge_write(const uint8_t *data, uint16_t len) {
    return tx_buffer_write(data, len);
}

/**
 * @brief 写入字符串
 */
int usb_bridge_puts(const char *str) {
    return tx_buffer_write((uint8_t*)str, strlen(str));
}

/**
 * @brief 刷新缓冲区
 */
bool usb_bridge_flush(uint32_t timeout_ms) {
    uint32_t start = time_us_32() / 1000;
    
    while (tx_buffer_used_space() > 0) {
        if (timeout_ms > 0) {
            uint32_t elapsed = (time_us_32() / 1000) - start;
            if (elapsed > timeout_ms) {
                return false;
            }
        }
        sleep_ms(1);
    }
    
    return true;
}

// ==================== 接收接口（Core 0使用）====================

/**
 * @brief 检查可读字节数
 */
uint32_t usb_bridge_available(void) {
    return rx_buffer_used_space();
}

/**
 * @brief 读取数据
 */
int usb_bridge_read(uint8_t *buffer, uint16_t len) {
    return rx_buffer_read(buffer, len);
}

/**
 * @brief 读取单字节
 */
int usb_bridge_getchar(void) {
    uint8_t ch;
    if (rx_buffer_read(&ch, 1) == 1) {
        return ch;
    }
    return -1;
}

/**
 * @brief 读取一行
 */
int usb_bridge_readline(char *buffer, uint16_t max_len) {
    int count = 0;
    
    while (count < max_len - 1) {
        int ch = usb_bridge_getchar();
        if (ch == -1) break;
        
        buffer[count++] = (char)ch;
        
        if (ch == '\n') break;
    }
    
    buffer[count] = '\0';
    return count;
}

/**
 * @brief 清空接收缓冲区
 */
void usb_bridge_clear_rx(void) {
    rx_buffer.tail = rx_buffer.head;
    MEMORY_BARRIER();
}

// ==================== 统计信息 ====================

/**
 * @brief 获取统计信息
 */
void usb_bridge_get_stats(usb_bridge_stats_t *out_stats) {
    out_stats->tx_bytes = stats.tx_bytes;
    out_stats->rx_bytes = stats.rx_bytes;
    out_stats->tx_overflow = stats.tx_overflow;
    out_stats->rx_overflow = stats.rx_overflow;
    out_stats->tx_available = tx_buffer_available_space();
    out_stats->rx_available = rx_buffer_used_space();
}


/**
 * @file ao_communication.h
 * @brief Communication主动对象 - USB通信、协议解析、命令处理
 * @date 2025-10-23
 */

#ifndef AO_COMMUNICATION_H
#define AO_COMMUNICATION_H

#include "qpc.h"
#include "communication/protocol.h"
#include "utils/ring_buffer.h"

// ==================== Communication AO声明 ====================

/**
 * @brief Communication主动对象类
 */
typedef struct {
    QActive super;  // 继承QActive
    
    // 时间事件
    QTimeEvt usb_poll_timer;    // USB轮询定时器（10ms）
    
    // 协议解析器
    protocol_parser_t parser;
    
    // 缓冲区
    ring_buffer_t rx_buffer;
    ring_buffer_t tx_buffer;
    uint8_t rx_buffer_mem[USB_RX_BUFFER_SIZE];
    uint8_t tx_buffer_mem[USB_TX_BUFFER_SIZE];
    
    // 状态数据
    bool usb_connected;
    uint32_t cmd_count;
} AO_Communication_t;

// 全局实例声明
extern AO_Communication_t AO_Communication_inst;
extern QActive * const AO_Communication;

// ==================== 公共接口 ====================

/**
 * @brief 构造Communication AO
 */
void AO_Communication_ctor(void);

#endif // AO_COMMUNICATION_H


/**
 * @file usb_handler.c
 * @brief USB CDC通信处理实现
 * @date 2025-10-21
 */

#include "communication/usb_handler.h"
#include "communication/protocol.h"
#include "communication/commands.h"
#include "utils/ring_buffer.h"
#include "utils/error_handler.h"
#include "config/config.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include <string.h>

// 接收和发送缓冲区
static uint8_t g_rx_buffer_mem[USB_RX_BUFFER_SIZE];
static uint8_t g_tx_buffer_mem[USB_TX_BUFFER_SIZE];
static ring_buffer_t g_rx_buffer;
static ring_buffer_t g_tx_buffer;

// 协议解析器
static protocol_parser_t g_parser;

// USB连接状态
static bool g_usb_connected = false;

bool usb_handler_init(void) {
    // 注意：不要在这里调用stdio_init_all()！
    // USB CDC应该在main()中初始化
    // 重复初始化会导致TinyUSB状态混乱
    
    // 初始化环形缓冲区
    ring_buffer_init(&g_rx_buffer, g_rx_buffer_mem, USB_RX_BUFFER_SIZE);
    ring_buffer_init(&g_tx_buffer, g_tx_buffer_mem, USB_TX_BUFFER_SIZE);
    
    // 初始化协议解析器
    protocol_parser_init(&g_parser);
    
    // 初始化命令处理器
    commands_init();
    
    return true;
}

void usb_handler_process(void) {
    // 重要：必须调用tud_task()来处理USB事件
    // 这是TinyUSB正常工作的关键！
    tud_task();
    
    // 检查USB连接状态
    g_usb_connected = tud_cdc_connected();
    
    if (!g_usb_connected) {
        return;
    }
    
    // 接收数据到环形缓冲区
    while (tud_cdc_available()) {
        uint8_t byte = tud_cdc_read_char();
        
        if (!ring_buffer_put(&g_rx_buffer, byte)) {
            // 缓冲区满，丢弃数据
            error_set(ERROR_COMM_OVERFLOW);
            break;
        }
    }
    
    // 解析接收到的数据
    uint8_t byte;
    while (ring_buffer_get(&g_rx_buffer, &byte)) {
        if (protocol_parse_byte(&g_parser, byte)) {
            // 帧接收完成，处理命令
            const protocol_frame_t *frame = protocol_get_frame(&g_parser);
            if (frame != NULL) {
                command_result_t result;
                
                if (commands_process(frame, &result)) {
                    // 构建响应
                    uint8_t resp_buffer[128];
                    uint16_t resp_len = protocol_build_response(
                        frame->id,
                        frame->cmd,
                        result.resp_code,
                        result.data,
                        result.data_len,
                        resp_buffer,
                        sizeof(resp_buffer)
                    );
                    
                    // 发送响应
                    if (resp_len > 0) {
                        usb_send_data(resp_buffer, resp_len);
                    }
                }
            }
            
            // 重置解析器准备接收下一帧
            protocol_parser_reset(&g_parser);
        }
    }
    
    // 检查协议超时
    protocol_check_timeout(&g_parser);
    
    // 发送缓冲区的数据
    if (!ring_buffer_is_empty(&g_tx_buffer) && tud_cdc_write_available()) {
        uint8_t tx_byte;
        size_t sent = 0;
        
        while (ring_buffer_get(&g_tx_buffer, &tx_byte) && sent < 64) {
            tud_cdc_write_char(tx_byte);
            sent++;
        }
        
        if (sent > 0) {
            tud_cdc_write_flush();
        }
    }
}

uint32_t usb_send_data(const uint8_t *data, uint32_t len) {
    if (!g_usb_connected || data == NULL || len == 0) {
        return 0;
    }
    
    // 写入发送缓冲区
    size_t written = ring_buffer_write(&g_tx_buffer, data, len);
    
    // 如果缓冲区有空间且USB可写，立即发送
    if (tud_cdc_write_available()) {
        uint8_t byte;
        size_t sent = 0;
        
        while (ring_buffer_get(&g_tx_buffer, &byte) && sent < 64) {
            tud_cdc_write_char(byte);
            sent++;
        }
        
        if (sent > 0) {
            tud_cdc_write_flush();
        }
    }
    
    return written;
}

uint32_t usb_receive_data(uint8_t *data, uint32_t max_len) {
    if (data == NULL || max_len == 0) {
        return 0;
    }
    
    return ring_buffer_read(&g_rx_buffer, data, max_len);
}

bool usb_is_connected(void) {
    return g_usb_connected;
}

void usb_clear_rx_buffer(void) {
    ring_buffer_clear(&g_rx_buffer);
}

uint32_t usb_get_rx_available(void) {
    return ring_buffer_count(&g_rx_buffer);
}


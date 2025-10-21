/**
 * @file usb_handler.h
 * @brief USB CDC通信处理
 * @date 2025-10-21
 */

#ifndef USB_HANDLER_H
#define USB_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief 初始化USB CDC通信
 * @return true 成功, false 失败
 */
bool usb_handler_init(void);

/**
 * @brief USB通信处理（轮询模式）
 * 应在主循环或任务中定期调用
 */
void usb_handler_process(void);

/**
 * @brief 发送数据
 * @param data 数据指针
 * @param len 数据长度
 * @return 实际发送的字节数
 */
uint32_t usb_send_data(const uint8_t *data, uint32_t len);

/**
 * @brief 接收数据
 * @param data 数据缓冲区
 * @param max_len 最大长度
 * @return 实际接收的字节数
 */
uint32_t usb_receive_data(uint8_t *data, uint32_t max_len);

/**
 * @brief 检查USB是否连接
 * @return true 已连接, false 未连接
 */
bool usb_is_connected(void);

/**
 * @brief 清空接收缓冲区
 */
void usb_clear_rx_buffer(void);

/**
 * @brief 获取接收缓冲区可用数据量
 * @return 可用字节数
 */
uint32_t usb_get_rx_available(void);

#endif // USB_HANDLER_H


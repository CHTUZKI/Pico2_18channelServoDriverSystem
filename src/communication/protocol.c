/**
 * @file protocol.c
 * @brief 通信协议实现
 * @date 2025-10-21
 */

#include "communication/protocol.h"
#include "communication/crc16.h"
#include "utils/error_handler.h"
#include "pico/stdlib.h"
#include <string.h>

void protocol_parser_init(protocol_parser_t *parser) {
    memset(parser, 0, sizeof(protocol_parser_t));
    parser->state = PARSE_STATE_IDLE;
}

void protocol_parser_reset(protocol_parser_t *parser) {
    parser->state = PARSE_STATE_IDLE;
    parser->data_index = 0;
}

bool protocol_parse_byte(protocol_parser_t *parser, uint8_t byte) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    parser->last_byte_time = now;
    
    switch (parser->state) {
        case PARSE_STATE_IDLE:
            if (byte == PROTOCOL_HEADER_1) {
                parser->frame.header[0] = byte;
                parser->state = PARSE_STATE_HEADER_1;
            }
            break;
            
        case PARSE_STATE_HEADER_1:
            if (byte == PROTOCOL_HEADER_2) {
                parser->frame.header[1] = byte;
                parser->state = PARSE_STATE_HEADER_2;
            } else if (byte == PROTOCOL_HEADER_1) {
                // 可能是新帧开始
                parser->frame.header[0] = byte;
            } else {
                // 错误，重置
                protocol_parser_reset(parser);
            }
            break;
            
        case PARSE_STATE_HEADER_2:
            parser->frame.id = byte;
            parser->state = PARSE_STATE_ID;
            break;
            
        case PARSE_STATE_ID:
            parser->frame.cmd = byte;
            parser->state = PARSE_STATE_CMD;
            break;
            
        case PARSE_STATE_CMD:
            parser->frame.len = byte;
            parser->data_index = 0;
            
            // 检查数据长度合法性并决定下一状态
            if (parser->frame.len > PROTOCOL_MAX_DATA_LEN) {
                error_set(ERROR_CMD_LENGTH);
                protocol_parser_reset(parser);
                parser->error_count++;
            } else if (parser->frame.len == 0) {
                // 无数据，直接接收CRC
                parser->state = PARSE_STATE_CRC_H;
            } else {
                // 有数据，接收数据
                parser->state = PARSE_STATE_DATA;
            }
            break;
            
        case PARSE_STATE_LEN:
            // LEN状态保留用于协议扩展，当前直接在CMD状态处理长度
            // 如果意外进入此状态，重置解析器
            protocol_parser_reset(parser);
            parser->error_count++;
            break;
            
        case PARSE_STATE_DATA:
            parser->frame.data[parser->data_index++] = byte;
            
            if (parser->data_index >= parser->frame.len) {
                // 数据接收完成，接收CRC
                parser->state = PARSE_STATE_CRC_H;
            }
            break;
            
        case PARSE_STATE_CRC_H:
            parser->frame.crc = (byte << 8);  // 高字节
            parser->state = PARSE_STATE_CRC_L;
            break;
            
        case PARSE_STATE_CRC_L:
            parser->frame.crc |= byte;  // 低字节
            
            // 验证CRC（从ID到数据结束，不包括帧头）
            uint8_t temp_buffer[PROTOCOL_MAX_DATA_LEN + 3];
            temp_buffer[0] = parser->frame.id;
            temp_buffer[1] = parser->frame.cmd;
            temp_buffer[2] = parser->frame.len;
            memcpy(&temp_buffer[3], parser->frame.data, parser->frame.len);
            
            uint16_t calculated_crc = crc16_ccitt(temp_buffer, 3 + parser->frame.len);
            
            if (calculated_crc == parser->frame.crc) {
                // CRC校验通过
                parser->state = PARSE_STATE_COMPLETE;
                parser->frame_count++;
                return true;  // 帧接收完成
            } else {
                // CRC错误
                error_set(ERROR_COMM_CRC);
                protocol_parser_reset(parser);
                parser->error_count++;
            }
            break;
            
        case PARSE_STATE_COMPLETE:
            // 不应该到这里，重置
            protocol_parser_reset(parser);
            break;
    }
    
    return false;
}

const protocol_frame_t* protocol_get_frame(protocol_parser_t *parser) {
    if (parser->state == PARSE_STATE_COMPLETE) {
        return &parser->frame;
    }
    return NULL;
}

void protocol_check_timeout(protocol_parser_t *parser) {
    if (parser->state == PARSE_STATE_IDLE) {
        return;  // 空闲状态不检查超时
    }
    
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    if (now - parser->last_byte_time > PROTOCOL_TIMEOUT_MS) {
        // 超时，重置解析器
        error_set(ERROR_COMM_TIMEOUT);
        protocol_parser_reset(parser);
        parser->timeout_count++;
    }
}

uint16_t protocol_build_frame(uint8_t id, uint8_t cmd,
                               const uint8_t *data, uint8_t data_len,
                               uint8_t *buffer, uint16_t buffer_size) {
    // 检查缓冲区大小
    uint16_t frame_len = 5 + data_len + 2;  // 头2+ID1+CMD1+LEN1+数据+CRC2
    if (buffer_size < frame_len) {
        return 0;
    }
    
    // 检查数据长度
    if (data_len > PROTOCOL_MAX_DATA_LEN) {
        return 0;
    }
    
    // 构建帧
    buffer[0] = PROTOCOL_HEADER_1;
    buffer[1] = PROTOCOL_HEADER_2;
    buffer[2] = id;
    buffer[3] = cmd;
    buffer[4] = data_len;
    
    if (data_len > 0 && data != NULL) {
        memcpy(&buffer[5], data, data_len);
    }
    
    // 计算CRC
    uint16_t crc = crc16_ccitt(buffer, 5 + data_len);
    buffer[5 + data_len] = (crc >> 8) & 0xFF;      // CRC高字节
    buffer[5 + data_len + 1] = crc & 0xFF;          // CRC低字节
    
    return frame_len;
}

uint16_t protocol_build_response(uint8_t id, uint8_t cmd, uint8_t resp_code,
                                  const uint8_t *data, uint8_t data_len,
                                  uint8_t *buffer, uint16_t buffer_size) {
    // 响应数据：响应码 + 数据
    uint8_t resp_data[PROTOCOL_MAX_DATA_LEN];
    resp_data[0] = resp_code;
    
    uint8_t total_len = 1;
    if (data_len > 0 && data != NULL) {
        memcpy(&resp_data[1], data, data_len);
        total_len += data_len;
    }
    
    return protocol_build_frame(id, cmd, resp_data, total_len, buffer, buffer_size);
}


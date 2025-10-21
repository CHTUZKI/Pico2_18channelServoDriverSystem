/**
 * @file protocol.h
 * @brief 通信协议定义和解析
 * @date 2025-10-21
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>
#include "config/config.h"

// ==================== 协议常量 ====================
#define PROTOCOL_HEADER_1       0xFF
#define PROTOCOL_HEADER_2       0xFE
#define PROTOCOL_MIN_FRAME_LEN  7       // 最小帧长：头2+ID1+CMD1+LEN1+CRC2

// ==================== 命令定义 ====================
// 位置控制命令
#define CMD_MOVE_SINGLE         0x01    // 单轴位置控制
#define CMD_MOVE_MULTI          0x02    // 多轴选择性控制
#define CMD_MOVE_ALL            0x03    // 全轴批量控制

// 查询命令
#define CMD_GET_SINGLE          0x10    // 查询单轴状态
#define CMD_GET_ALL             0x11    // 查询全轴状态
#define CMD_GET_STREAM          0x12    // 流式数据上报

// 配置命令
#define CMD_SET_PARAM           0x15    // 参数设置
#define CMD_ENABLE              0x20    // 使能舵机
#define CMD_DISABLE             0x21    // 失能舵机

// 存储命令
#define CMD_SAVE_FLASH          0x30    // 保存参数到Flash
#define CMD_LOAD_FLASH          0x31    // 从Flash加载参数
#define CMD_RESET_FACTORY       0x32    // 恢复出厂设置

// 系统命令
#define CMD_PING                0xFE    // 心跳/连接检测
#define CMD_ESTOP               0xFF    // 紧急停止

// ==================== 响应码 ====================
#define RESP_OK                 0x00    // 成功
#define RESP_ERROR              0x01    // 错误
#define RESP_INVALID_CMD        0x02    // 无效命令
#define RESP_INVALID_PARAM      0x03    // 无效参数
#define RESP_CRC_ERROR          0x04    // CRC错误
#define RESP_TIMEOUT            0x05    // 超时
#define RESP_BUSY               0x06    // 繁忙

// ==================== 协议帧结构 ====================
/**
 * @brief 协议帧格式
 * [0xFF][0xFE][ID][CMD][LEN][DATA...][CRC_H][CRC_L]
 */
typedef struct {
    uint8_t header[2];      // 帧头 0xFF 0xFE
    uint8_t id;             // 设备ID（0x00=广播，0x01-0xFE=单播）
    uint8_t cmd;            // 命令字节
    uint8_t len;            // 数据长度（不包括帧头、ID、CMD、LEN、CRC）
    uint8_t data[PROTOCOL_MAX_DATA_LEN]; // 数据内容
    uint16_t crc;           // CRC-16校验
} protocol_frame_t;

// ==================== 解析状态机 ====================
typedef enum {
    PARSE_STATE_IDLE = 0,       // 空闲，等待帧头
    PARSE_STATE_HEADER_1,       // 接收到第一个帧头
    PARSE_STATE_HEADER_2,       // 接收到第二个帧头
    PARSE_STATE_ID,             // 接收ID
    PARSE_STATE_CMD,            // 接收命令
    PARSE_STATE_LEN,            // 接收长度
    PARSE_STATE_DATA,           // 接收数据
    PARSE_STATE_CRC_H,          // 接收CRC高字节
    PARSE_STATE_CRC_L,          // 接收CRC低字节
    PARSE_STATE_COMPLETE        // 帧接收完成
} parse_state_t;

/**
 * @brief 协议解析器结构体
 */
typedef struct {
    parse_state_t state;        // 当前状态
    protocol_frame_t frame;     // 当前帧
    uint8_t data_index;         // 数据索引
    uint32_t last_byte_time;    // 最后接收字节时间（ms）
    uint32_t timeout_count;     // 超时计数
    uint32_t error_count;       // 错误计数
    uint32_t frame_count;       // 接收帧计数
} protocol_parser_t;

/**
 * @brief 初始化协议解析器
 * @param parser 解析器指针
 */
void protocol_parser_init(protocol_parser_t *parser);

/**
 * @brief 解析单个字节
 * @param parser 解析器指针
 * @param byte 接收的字节
 * @return true 帧接收完成, false 继续接收
 */
bool protocol_parse_byte(protocol_parser_t *parser, uint8_t byte);

/**
 * @brief 获取完整帧
 * @param parser 解析器指针
 * @return 帧指针，NULL表示无有效帧
 */
const protocol_frame_t* protocol_get_frame(protocol_parser_t *parser);

/**
 * @brief 重置解析器
 * @param parser 解析器指针
 */
void protocol_parser_reset(protocol_parser_t *parser);

/**
 * @brief 检查超时
 * @param parser 解析器指针
 */
void protocol_check_timeout(protocol_parser_t *parser);

/**
 * @brief 构建响应帧
 * @param id 设备ID
 * @param cmd 命令
 * @param resp_code 响应码
 * @param data 数据
 * @param data_len 数据长度
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 实际帧长度
 */
uint16_t protocol_build_response(uint8_t id, uint8_t cmd, uint8_t resp_code,
                                  const uint8_t *data, uint8_t data_len,
                                  uint8_t *buffer, uint16_t buffer_size);

/**
 * @brief 构建数据帧
 * @param id 设备ID
 * @param cmd 命令
 * @param data 数据
 * @param data_len 数据长度
 * @param buffer 输出缓冲区
 * @param buffer_size 缓冲区大小
 * @return 实际帧长度
 */
uint16_t protocol_build_frame(uint8_t id, uint8_t cmd,
                               const uint8_t *data, uint8_t data_len,
                               uint8_t *buffer, uint16_t buffer_size);

#endif // PROTOCOL_H


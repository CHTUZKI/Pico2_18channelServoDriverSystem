/**
 * @file commands.h
 * @brief 命令处理器
 * @date 2025-10-21
 */

#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>
#include <stdbool.h>
#include "communication/protocol.h"

/**
 * @brief 命令处理结果
 */
typedef struct {
    uint8_t resp_code;          // 响应码
    uint8_t data[PROTOCOL_MAX_DATA_LEN]; // 响应数据
    uint8_t data_len;           // 响应数据长度
} command_result_t;

/**
 * @brief 初始化命令处理器
 */
void commands_init(void);

/**
 * @brief 处理接收到的命令
 * @param frame 协议帧
 * @param result 处理结果
 * @return true 需要响应, false 不需要响应
 */
bool commands_process(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理MOVE_SINGLE命令
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_move_single(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理MOVE_ALL命令
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_move_all(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理GET_SINGLE命令
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_get_single(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理GET_ALL命令
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_get_all(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理ENABLE命令
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_enable(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理DISABLE命令
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_disable(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理SAVE_FLASH命令
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_save_flash(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理LOAD_FLASH命令
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_load_flash(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理ESTOP命令
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_emergency_stop(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理PING命令
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_ping(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理MOVE_TRAPEZOID命令（梯形速度运动）
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_move_trapezoid(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理TRAJ_ADD_POINT命令（添加轨迹点）
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_traj_add_point(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理TRAJ_START命令（开始执行轨迹）
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_traj_start(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理TRAJ_STOP命令（停止执行轨迹）
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_traj_stop(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理TRAJ_CLEAR命令（清空轨迹）
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_traj_clear(const protocol_frame_t *frame, command_result_t *result);

/**
 * @brief 处理TRAJ_GET_INFO命令（查询轨迹信息）
 * @param frame 协议帧
 * @param result 处理结果
 */
void cmd_traj_get_info(const protocol_frame_t *frame, command_result_t *result);

#endif // COMMANDS_H


/**
 * @file cmd_motion_buffer.c
 * @brief 运动缓冲区相关命令处理实现
 * @date 2025-10-26
 */

#include "communication/commands.h"
#include "communication/protocol.h"
#include "motion/motion_scheduler.h"  // 使用调度器接口
#include "utils/usb_bridge.h"
#include "config/config.h"
#include <string.h>

// 运动调度器提供缓冲区管理接口

// ==================== 调试宏 ====================
#if DEBUG_COMMAND
    #define CMD_DEBUG(...) usb_bridge_printf(__VA_ARGS__)
#else
    #define CMD_DEBUG(...) ((void)0)
#endif

/**
 * @brief 处理ADD_MOTION_BLOCK命令
 * @description 数据格式：[timestamp_ms(4)] [servo_id(1)] [angle(2)] [velocity(2)] [accel(2)] [decel(2)]
 *              total: 13字节
 */
void cmd_add_motion_block(const protocol_frame_t *frame, command_result_t *result) {
    // 检查数据长度
    if (frame->data_len != 13) {
        result->resp_code = RESP_INVALID_PARAM;
        result->data_len = 0;
        CMD_DEBUG("[CMD] ADD_BLOCK: Invalid length %d (expected 13)\n", frame->data_len);
        return;
    }
    
    // 解析数据
    motion_block_t block;
    const uint8_t *data = frame->data;
    
    // timestamp_ms (4字节, 小端序)
    block.timestamp_ms = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | 
                        ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    
    // servo_id (1字节)
    block.servo_id = data[4];
    
    // angle (2字节, 有符号, 0.01度精度)
    int16_t angle_raw = (int16_t)(data[5] | (data[6] << 8));
    block.target_angle = (float)angle_raw / 100.0f;
    
    // velocity (2字节, 无符号, 0.1度/秒精度)
    uint16_t vel_raw = data[7] | (data[8] << 8);
    block.velocity = (float)vel_raw / 10.0f;
    
    // acceleration (2字节, 无符号, 0.1度/秒²精度)
    uint16_t accel_raw = data[9] | (data[10] << 8);
    block.acceleration = (float)accel_raw / 10.0f;
    
    // deceleration (2字节, 无符号, 0.1度/秒²精度, 0表示与加速度相同)
    uint16_t decel_raw = data[11] | (data[12] << 8);
    block.deceleration = (float)decel_raw / 10.0f;
    
    // 参数校验
    if (block.servo_id >= SERVO_COUNT) {
        result->resp_code = RESP_INVALID_PARAM;
        result->data_len = 0;
        CMD_DEBUG("[CMD] ADD_BLOCK: Invalid servo_id %d\n", block.servo_id);
        return;
    }
    
    if (block.target_angle < -180.0f || block.target_angle > 180.0f) {
        result->resp_code = RESP_INVALID_PARAM;
        result->data_len = 0;
        CMD_DEBUG("[CMD] ADD_BLOCK: Invalid angle %d\n", (int)block.target_angle);
        return;
    }
    
    // 添加到缓冲区
    if (!motion_scheduler_add_block(&block)) {
        result->resp_code = RESP_BUSY;  // 缓冲区已满
        result->data_len = 0;
        CMD_DEBUG("[CMD] ADD_BLOCK: Buffer full\n");
        return;
    }
    
    // 成功添加（调试输出由motion_buffer.c负责）
    
    result->resp_code = RESP_OK;
    result->data[0] = motion_scheduler_get_available();  // 返回可用空间
    result->data_len = 1;
}

/**
 * @brief 处理START_MOTION命令
 * @description 数据格式：[reserved(1)] 或无数据
 */
void cmd_start_motion(const protocol_frame_t *frame, command_result_t *result) {
    if (!motion_scheduler_start()) {
        result->resp_code = RESP_ERROR;  // 缓冲区为空
        result->data_len = 0;
        CMD_DEBUG("[CMD] START_MOTION: Buffer empty\n");
        return;
    }
    
    // 成功启动（调试输出由motion_buffer.c负责）
    result->resp_code = RESP_OK;
    result->data_len = 0;
}

/**
 * @brief 处理STOP_MOTION命令
 */
void cmd_stop_motion(const protocol_frame_t *frame, command_result_t *result) {
    motion_scheduler_stop();
    CMD_DEBUG("[CMD] STOP_MOTION\n");
    result->resp_code = RESP_OK;
    result->data_len = 0;
}

/**
 * @brief 处理PAUSE_MOTION命令
 */
void cmd_pause_motion(const protocol_frame_t *frame, command_result_t *result) {
    motion_scheduler_pause();
    CMD_DEBUG("[CMD] PAUSE_MOTION\n");
    result->resp_code = RESP_OK;
    result->data_len = 0;
}

/**
 * @brief 处理RESUME_MOTION命令
 */
void cmd_resume_motion(const protocol_frame_t *frame, command_result_t *result) {
    motion_scheduler_resume();
    CMD_DEBUG("[CMD] RESUME_MOTION\n");
    result->resp_code = RESP_OK;
    result->data_len = 0;
}

/**
 * @brief 处理CLEAR_BUFFER命令
 */
void cmd_clear_buffer(const protocol_frame_t *frame, command_result_t *result) {
    motion_scheduler_clear();
    // 调试输出由motion_buffer.c负责
    result->resp_code = RESP_OK;
    result->data_len = 0;
}

/**
 * @brief 处理GET_BUFFER_STATUS命令
 * @description 返回数据：[count(1)] [running(1)] [paused(1)] [available(1)]
 */
void cmd_get_buffer_status(const protocol_frame_t *frame, command_result_t *result) {
    result->resp_code = RESP_OK;
    result->data[0] = motion_scheduler_get_count();
    result->data[1] = motion_scheduler_is_running() ? 1 : 0;
    result->data[2] = motion_scheduler_is_paused() ? 1 : 0;
    result->data[3] = motion_scheduler_get_available();
    result->data_len = 4;
    
    CMD_DEBUG("[CMD] BUFFER_STATUS: count=%d avail=%d\n",
             result->data[0], result->data[3]);
}


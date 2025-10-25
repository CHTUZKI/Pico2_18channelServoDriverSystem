/**
 * @file cmd_servo_360.c
 * @brief 360度连续旋转舵机命令处理实现
 * @date 2025-10-25
 */

#include "communication/commands.h"
#include "communication/protocol.h"
#include "motion/planner.h"
#include "servo/servo_360.h"
#include "utils/usb_bridge.h"
#include "config/config.h"
#include <string.h>
#include <stdlib.h>

// ==================== 调试宏 ====================
#if DEBUG_COMMAND
    #define CMD_DEBUG(...) usb_bridge_printf(__VA_ARGS__)
#else
    #define CMD_DEBUG(...) ((void)0)
#endif

/**
 * @brief 处理ADD_CONTINUOUS_MOTION命令（添加360度舵机速度块到缓冲区）
 * @description 数据格式：[timestamp_ms(4)] [servo_id(1)] [speed(1,signed)] [accel(1)] [decel(1)] [duration_ms(2)]
 *              total: 10字节
 */
void cmd_add_continuous_motion(const protocol_frame_t *frame, command_result_t *result) {
    // 检查数据长度
    if (frame->len != 10) {
        result->resp_code = RESP_INVALID_PARAM;
        result->data_len = 0;
        CMD_DEBUG("[CMD] ADD_CONTINUOUS: Invalid length %d (expected 10)\n", frame->len);
        return;
    }
    
    // 解析数据
    const uint8_t *data = frame->data;
    
    // timestamp_ms (4字节, 小端序)
    uint32_t timestamp_ms = (uint32_t)data[0] | ((uint32_t)data[1] << 8) | 
                            ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
    
    // servo_id (1字节)
    uint8_t servo_id = data[4];
    
    // speed (1字节, 有符号, -100 到 +100)
    int8_t target_speed = (int8_t)data[5];
    
    // accel_rate (1字节, 无符号, %/秒)
    uint8_t accel_rate = data[6];
    
    // decel_rate (1字节, 无符号, %/秒, 0表示与加速度相同)
    uint8_t decel_rate = data[7];
    
    // duration_ms (2字节, 小端序, 毫秒)
    uint16_t duration_ms = data[8] | (data[9] << 8);
    
    // 参数校验
    if (servo_id >= SERVO_COUNT) {
        result->resp_code = RESP_INVALID_PARAM;
        result->data_len = 0;
        CMD_DEBUG("[CMD] ADD_CONTINUOUS: Invalid servo_id %d\n", servo_id);
        return;
    }
    
    if (target_speed < -100 || target_speed > 100) {
        result->resp_code = RESP_INVALID_PARAM;
        result->data_len = 0;
        CMD_DEBUG("[CMD] ADD_CONTINUOUS: Invalid speed %d\n", target_speed);
        return;
    }
    
    // 添加到规划器（支持速度平滑过渡）
    if (!planner_add_continuous_motion(timestamp_ms, servo_id, target_speed, 
                                      accel_rate, decel_rate, duration_ms)) {
        result->resp_code = RESP_BUSY;  // 缓冲区已满
        result->data_len = 0;
        CMD_DEBUG("[CMD] ADD_CONTINUOUS: Planner buffer full\n");
        return;
    }
    
    // 成功添加
    result->resp_code = RESP_OK;
    result->data[0] = planner_available();  // 返回可用空间
    result->data_len = 1;
}

/**
 * @brief 处理SERVO_360_SET_SPEED命令（直接设置速度，不经过规划器）
 * @description 数据格式：[servo_id(1)] [speed(1,signed)]
 *              total: 2字节
 */
void cmd_servo_360_set_speed(const protocol_frame_t *frame, command_result_t *result) {
    if (frame->len != 2) {
        result->resp_code = RESP_INVALID_PARAM;
        result->data_len = 0;
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    int8_t speed = (int8_t)frame->data[1];
    
    if (servo_id >= SERVO_COUNT) {
        result->resp_code = RESP_INVALID_PARAM;
        result->data_len = 0;
        return;
    }
    
    // 直接设置速度（即时命令）
    if (!servo_360_set_speed(servo_id, speed)) {
        result->resp_code = RESP_ERROR;
        result->data_len = 0;
        CMD_DEBUG("[CMD] SERVO_360_SET_SPEED: Failed S%d\n", servo_id);
        return;
    }
    
    CMD_DEBUG("[CMD] SERVO_360_SET_SPEED: S%d speed=%d%%\n", servo_id, speed);
    result->resp_code = RESP_OK;
    result->data_len = 0;
}

/**
 * @brief 处理SERVO_360_SOFT_STOP命令（软停止）
 * @description 数据格式：[servo_id(1)]
 *              total: 1字节, 0xFF表示全部
 */
void cmd_servo_360_soft_stop(const protocol_frame_t *frame, command_result_t *result) {
    if (frame->len != 1) {
        result->resp_code = RESP_INVALID_PARAM;
        result->data_len = 0;
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    
    if (!servo_360_soft_stop(servo_id)) {
        result->resp_code = RESP_ERROR;
        result->data_len = 0;
        return;
    }
    
    CMD_DEBUG("[CMD] SERVO_360_SOFT_STOP: S%d\n", servo_id);
    result->resp_code = RESP_OK;
    result->data_len = 0;
}

/**
 * @brief 处理SERVO_360_SET_ACCEL命令（设置加减速参数）
 * @description 数据格式：[servo_id(1)] [accel_rate(1)] [decel_rate(1)]
 *              total: 3字节
 */
void cmd_servo_360_set_accel(const protocol_frame_t *frame, command_result_t *result) {
    if (frame->len != 3) {
        result->resp_code = RESP_INVALID_PARAM;
        result->data_len = 0;
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    uint8_t accel_rate = frame->data[1];
    uint8_t decel_rate = frame->data[2];
    
    if (servo_id >= SERVO_COUNT) {
        result->resp_code = RESP_INVALID_PARAM;
        result->data_len = 0;
        return;
    }
    
    servo_360_set_acceleration(servo_id, accel_rate);
    servo_360_set_deceleration(servo_id, decel_rate);
    
    CMD_DEBUG("[CMD] SERVO_360_SET_ACCEL: S%d accel=%d decel=%d\n", 
             servo_id, accel_rate, decel_rate);
    result->resp_code = RESP_OK;
    result->data_len = 0;
}

/**
 * @brief 处理SERVO_360_GET_INFO命令（查询360度舵机状态）
 * @description 返回数据：[current_speed(1,signed)] [target_speed(1,signed)] [enabled(1)] [moving(1)]
 *              total: 4字节
 */
void cmd_servo_360_get_info(const protocol_frame_t *frame, command_result_t *result) {
    if (frame->len != 1) {
        result->resp_code = RESP_INVALID_PARAM;
        result->data_len = 0;
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    
    if (servo_id >= SERVO_COUNT) {
        result->resp_code = RESP_INVALID_PARAM;
        result->data_len = 0;
        return;
    }
    
    // 获取360度舵机信息
    const servo_360_t *info = servo_360_get_info(servo_id);
    if (info == NULL) {
        result->resp_code = RESP_ERROR;
        result->data_len = 0;
        return;
    }
    
    result->resp_code = RESP_OK;
    result->data[0] = (uint8_t)info->current_speed;    // 有符号转无符号
    result->data[1] = (uint8_t)info->target_speed;
    result->data[2] = info->enabled ? 1 : 0;
    result->data[3] = servo_360_is_moving(servo_id) ? 1 : 0;
    result->data_len = 4;
}


/**
 * @file commands.c
 * @brief 命令处理器实现
 * @date 2025-10-21
 */

#include "communication/commands.h"
#include "servo/servo_control.h"
#include "pwm/pwm_driver.h"
#include "storage/param_manager.h"
#include "utils/error_handler.h"
#include "utils/usb_bridge.h"
#include "config/config.h"
#include "motion/interpolation.h"
#include <string.h>

// 调试宏
#if DEBUG_COMMAND
    #define CMD_DEBUG(fmt, ...) usb_bridge_printf("[CMD] " fmt, ##__VA_ARGS__)
#else
    #define CMD_DEBUG(fmt, ...)
#endif

// 命令统计
static uint32_t g_cmd_count = 0;
static uint32_t g_cmd_error_count = 0;

// 轨迹队列（每个舵机一个）
static trajectory_queue_t g_trajectories[SERVO_COUNT];

void commands_init(void) {
    g_cmd_count = 0;
    g_cmd_error_count = 0;
}

bool commands_process(const protocol_frame_t *frame, command_result_t *result) {
    if (frame == NULL || result == NULL) {
        return false;
    }
    
    // 初始化结果
    result->resp_code = RESP_OK;
    result->data_len = 0;
    
    g_cmd_count++;
    
    // 根据命令字分发
    switch (frame->cmd) {
        case CMD_MOVE_SINGLE:
            cmd_move_single(frame, result);
            break;
            
        case CMD_MOVE_ALL:
            cmd_move_all(frame, result);
            break;
            
        case CMD_GET_SINGLE:
            cmd_get_single(frame, result);
            break;
            
        case CMD_GET_ALL:
            cmd_get_all(frame, result);
            break;
            
        case CMD_ENABLE:
            cmd_enable(frame, result);
            break;
            
        case CMD_DISABLE:
            cmd_disable(frame, result);
            break;
            
        case CMD_SAVE_FLASH:
            cmd_save_flash(frame, result);
            break;
            
        case CMD_LOAD_FLASH:
            cmd_load_flash(frame, result);
            break;
            
        case CMD_ESTOP:
            cmd_emergency_stop(frame, result);
            break;
            
        case CMD_PING:
            cmd_ping(frame, result);
            break;
            
        case CMD_MOVE_TRAPEZOID:
            cmd_move_trapezoid(frame, result);
            break;
            
        case CMD_TRAJ_ADD_POINT:
            cmd_traj_add_point(frame, result);
            break;
            
        case CMD_TRAJ_START:
            cmd_traj_start(frame, result);
            break;
            
        case CMD_TRAJ_STOP:
            cmd_traj_stop(frame, result);
            break;
            
        case CMD_TRAJ_CLEAR:
            cmd_traj_clear(frame, result);
            break;
            
        case CMD_TRAJ_GET_INFO:
            cmd_traj_get_info(frame, result);
            break;
            
        default:
            result->resp_code = RESP_INVALID_CMD;
            error_set(ERROR_CMD_INVALID);
            g_cmd_error_count++;
            break;
    }
    
    return true;  // 需要响应
}

void cmd_move_single(const protocol_frame_t *frame, command_result_t *result) {
    // 数据格式：[ID][角度高][角度低][速度高][速度低]
    if (frame->len < 5) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_PARAM);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    uint16_t angle_raw = (frame->data[1] << 8) | frame->data[2];
    uint16_t speed_ms = (frame->data[3] << 8) | frame->data[4];
    (void)speed_ms;  // TODO: 未来用于运动插值时间控制
    
    // 检查ID
    if (servo_id >= SERVO_COUNT) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_ID);
        return;
    }
    
    // 转换角度（角度 × 100，例如90.5度 = 9050）
    float angle = angle_raw / 100.0f;
    
    #if DEBUG_COMMAND
    // 【调试】打印servo_id和angle
    CMD_DEBUG("MOVE_SINGLE: servo_id=%d, angle=%.1f\n", servo_id, angle);
    #endif
    
    // 设置角度
    if (!servo_set_angle(servo_id, angle)) {
        result->resp_code = RESP_ERROR;
        return;
    }
    
    result->resp_code = RESP_OK;
}

void cmd_move_all(const protocol_frame_t *frame, command_result_t *result) {
    // 数据格式：[角度1高][角度1低]...[角度18高][角度18低][速度高][速度低]
    // 总长度：18*2 + 2 = 38字节
    if (frame->len < 38) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_PARAM);
        return;
    }
    
    float angles[SERVO_COUNT];
    
    // 解析18个角度
    for (int i = 0; i < SERVO_COUNT; i++) {
        uint16_t angle_raw = (frame->data[i * 2] << 8) | frame->data[i * 2 + 1];
        angles[i] = angle_raw / 100.0f;
    }
    
    uint16_t speed_ms = (frame->data[36] << 8) | frame->data[37];
    (void)speed_ms;  // TODO: 未来用于运动插值时间控制
    
    // 批量设置角度
    if (!servo_set_all_angles(angles)) {
        result->resp_code = RESP_ERROR;
        return;
    }
    
    result->resp_code = RESP_OK;
}

void cmd_get_single(const protocol_frame_t *frame, command_result_t *result) {
    // 数据格式：[ID]
    if (frame->len < 1) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_PARAM);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    
    if (servo_id >= SERVO_COUNT) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_ID);
        return;
    }
    
    // 获取舵机信息
    const servo_t *servo = servo_get_info(servo_id);
    if (servo == NULL) {
        result->resp_code = RESP_ERROR;
        return;
    }
    
    // 构建响应数据：[ID][当前角度高][当前角度低][目标角度高][目标角度低][使能]
    result->data[0] = servo_id;
    
    uint16_t current_angle = (uint16_t)(servo->current_angle * 100);
    result->data[1] = (current_angle >> 8) & 0xFF;
    result->data[2] = current_angle & 0xFF;
    
    uint16_t target_angle = (uint16_t)(servo->target_angle * 100);
    result->data[3] = (target_angle >> 8) & 0xFF;
    result->data[4] = target_angle & 0xFF;
    
    result->data[5] = servo->enabled ? 1 : 0;
    result->data_len = 6;
    result->resp_code = RESP_OK;
}

void cmd_get_all(const protocol_frame_t *frame, command_result_t *result) {
    // 响应数据：18个舵机的当前角度
    // 格式：[角度1高][角度1低]...[角度18高][角度18低]
    
    for (int i = 0; i < SERVO_COUNT; i++) {
        float angle = servo_get_angle(i);
        uint16_t angle_raw = (uint16_t)(angle * 100);
        
        result->data[i * 2] = (angle_raw >> 8) & 0xFF;
        result->data[i * 2 + 1] = angle_raw & 0xFF;
    }
    
    result->data_len = SERVO_COUNT * 2;
    result->resp_code = RESP_OK;
}

void cmd_enable(const protocol_frame_t *frame, command_result_t *result) {
    // 数据格式：[ID] (0xFF=全部)
    if (frame->len < 1) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_PARAM);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    
    if (servo_id == 0xFF) {
        // 使能所有舵机
        servo_enable(0xFF, true);
    } else if (servo_id < SERVO_COUNT) {
        // 使能单个舵机
        servo_enable(servo_id, true);
    } else {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_ID);
        return;
    }
    
    result->resp_code = RESP_OK;
}

void cmd_disable(const protocol_frame_t *frame, command_result_t *result) {
    // 数据格式：[ID] (0xFF=全部)
    if (frame->len < 1) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_PARAM);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    
    if (servo_id == 0xFF) {
        // 禁用所有舵机
        servo_enable(0xFF, false);
    } else if (servo_id < SERVO_COUNT) {
        // 禁用单个舵机
        servo_enable(servo_id, false);
    } else {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_ID);
        return;
    }
    
    result->resp_code = RESP_OK;
}

void cmd_save_flash(const protocol_frame_t *frame, command_result_t *result) {
    // 保存参数和位置到Flash
    bool save_ok = param_manager_save();           // 保存校准参数
    bool pos_ok = param_manager_save_positions();  // 保存当前位置
    
    if (save_ok && pos_ok) {
        result->resp_code = RESP_OK;
    } else {
        result->resp_code = RESP_ERROR;
        error_set(ERROR_FLASH_WRITE);
    }
}

void cmd_load_flash(const protocol_frame_t *frame, command_result_t *result) {
    // 从Flash加载参数
    if (param_manager_load()) {
        result->resp_code = RESP_OK;
    } else {
        result->resp_code = RESP_ERROR;
        error_set(ERROR_FLASH_READ);
    }
}

void cmd_emergency_stop(const protocol_frame_t *frame, command_result_t *result) {
    // 触发紧急停止
    emergency_stop_trigger();
    
    // 禁用所有PWM
    pwm_emergency_stop();
    
    result->resp_code = RESP_OK;
}

void cmd_ping(const protocol_frame_t *frame, command_result_t *result) {
    // 心跳响应
    // 返回系统状态信息
    result->data[0] = SYSTEM_VERSION_MAJOR;
    result->data[1] = SYSTEM_VERSION_MINOR;
    result->data[2] = SYSTEM_VERSION_PATCH;
    result->data[3] = (uint8_t)system_get_state();
    
    result->data_len = 4;
    result->resp_code = RESP_OK;
}

// ==================== 梯形速度和轨迹规划命令 ====================

void cmd_move_trapezoid(const protocol_frame_t *frame, command_result_t *result) {
    /**
     * 数据格式（9字节）：
     * [0]: servo_id
     * [1-2]: target_angle (int16, 角度×100)
     * [3-4]: max_velocity (uint16, 度/秒×10)
     * [5-6]: acceleration (uint16, 度/秒²×10)
     * [7-8]: deceleration (uint16, 度/秒²×10, 如果为0则使用acceleration)
     */
    if (frame->len < 9) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_PARAM);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    if (servo_id >= SERVO_COUNT) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_ID);
        return;
    }
    
    // 解析参数
    int16_t angle_raw = (int16_t)((frame->data[1] << 8) | frame->data[2]);
    uint16_t velocity_raw = (frame->data[3] << 8) | frame->data[4];
    uint16_t accel_raw = (frame->data[5] << 8) | frame->data[6];
    uint16_t decel_raw = (frame->data[7] << 8) | frame->data[8];
    
    // 转换为实际值
    float target_angle = (float)angle_raw / 100.0f;
    motion_params_t params;
    params.max_velocity = (float)velocity_raw / 10.0f;
    params.acceleration = (float)accel_raw / 10.0f;
    params.deceleration = (float)decel_raw / 10.0f;
    
    #if DEBUG_COMMAND
    CMD_DEBUG("MOVE_TRAPEZOID: id=%d, angle=%.1f, v=%.1f, a=%.1f, d=%.1f\\n",
              servo_id, target_angle, params.max_velocity, 
              params.acceleration, params.deceleration);
    #endif
    
    // 设置梯形速度运动（通过servo_control模块）
    servo_move_trapezoid(servo_id, target_angle, &params);
    
    result->resp_code = RESP_OK;
}

void cmd_traj_add_point(const protocol_frame_t *frame, command_result_t *result) {
    /**
     * 数据格式（11字节）：
     * [0]: servo_id
     * [1-2]: position (int16, 角度×100)
     * [3-4]: max_velocity (uint16, 度/秒×10)
     * [5-6]: acceleration (uint16, 度/秒²×10)
     * [7-8]: deceleration (uint16, 度/秒²×10)
     * [9-10]: dwell_time_ms (uint16, 毫秒)
     */
    if (frame->len < 11) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_PARAM);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    if (servo_id >= SERVO_COUNT) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_ID);
        return;
    }
    
    // 解析参数
    int16_t pos_raw = (int16_t)((frame->data[1] << 8) | frame->data[2]);
    uint16_t velocity_raw = (frame->data[3] << 8) | frame->data[4];
    uint16_t accel_raw = (frame->data[5] << 8) | frame->data[6];
    uint16_t decel_raw = (frame->data[7] << 8) | frame->data[8];
    uint16_t dwell_ms = (frame->data[9] << 8) | frame->data[10];
    
    // 转换为实际值
    float position = (float)pos_raw / 100.0f;
    motion_params_t params;
    params.max_velocity = (float)velocity_raw / 10.0f;
    params.acceleration = (float)accel_raw / 10.0f;
    params.deceleration = (float)decel_raw / 10.0f;
    
    // 添加到轨迹队列
    if (trajectory_add_point(&g_trajectories[servo_id], position, &params, dwell_ms)) {
        result->resp_code = RESP_OK;
    } else {
        result->resp_code = RESP_ERROR;  // 队列已满
    }
}

void cmd_traj_start(const protocol_frame_t *frame, command_result_t *result) {
    /**
     * 数据格式（2字节）：
     * [0]: servo_id
     * [1]: loop (0=不循环, 1=循环)
     */
    if (frame->len < 2) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_PARAM);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    if (servo_id >= SERVO_COUNT) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_ID);
        return;
    }
    
    bool loop = (frame->data[1] != 0);
    
    // 开始执行轨迹
    if (trajectory_start(&g_trajectories[servo_id], loop)) {
        // 将轨迹队列绑定到舵机的插值器
        servo_set_trajectory(servo_id, &g_trajectories[servo_id]);
        result->resp_code = RESP_OK;
    } else {
        result->resp_code = RESP_ERROR;  // 轨迹为空
    }
}

void cmd_traj_stop(const protocol_frame_t *frame, command_result_t *result) {
    /**
     * 数据格式（1字节）：
     * [0]: servo_id
     */
    if (frame->len < 1) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_PARAM);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    if (servo_id >= SERVO_COUNT) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_ID);
        return;
    }
    
    // 停止轨迹
    trajectory_stop(&g_trajectories[servo_id]);
    result->resp_code = RESP_OK;
}

void cmd_traj_clear(const protocol_frame_t *frame, command_result_t *result) {
    /**
     * 数据格式（1字节）：
     * [0]: servo_id
     */
    if (frame->len < 1) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_PARAM);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    if (servo_id >= SERVO_COUNT) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_ID);
        return;
    }
    
    // 清空轨迹
    trajectory_clear(&g_trajectories[servo_id]);
    result->resp_code = RESP_OK;
}

void cmd_traj_get_info(const protocol_frame_t *frame, command_result_t *result) {
    /**
     * 数据格式（1字节）：
     * [0]: servo_id
     * 
     * 响应数据（3字节）：
     * [0]: point_count
     * [1]: current_index
     * [2]: running (0=stopped, 1=running) | loop (bit1)
     */
    if (frame->len < 1) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_PARAM);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    if (servo_id >= SERVO_COUNT) {
        result->resp_code = RESP_INVALID_PARAM;
        error_set(ERROR_CMD_ID);
        return;
    }
    
    // 获取轨迹信息
    trajectory_queue_t *traj = &g_trajectories[servo_id];
    result->data[0] = traj->count;
    result->data[1] = traj->current_index;
    result->data[2] = (traj->running ? 0x01 : 0x00) | (traj->loop ? 0x02 : 0x00);
    result->data_len = 3;
    result->resp_code = RESP_OK;
}


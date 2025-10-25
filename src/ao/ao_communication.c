/**
 * @file ao_communication.c  
 * @brief Communication主动对象实现
 * @date 2025-10-23
 */

#define QP_IMPL
#include "qpc.h"
#include "ao/ao_communication.h"
#include "ao/ao_motion.h"
#include "ao/ao_system.h"
#include "events/events.h"
#include "events/event_types.h"
#include "config/config.h"
#include "communication/protocol.h"
#include "communication/crc16.h"
#include "servo/servo_control.h"
#include "servo/servo_manager.h"
#include "storage/param_manager.h"
#include "motion/interpolation.h"  // 添加：用于motion_params_t
#include "utils/ring_buffer.h"
#include "utils/usb_bridge.h"  // 使用USB桥接器代替直接访问USB
#include <stdio.h>
#include <string.h>

Q_DEFINE_THIS_FILE

// 调试开关由 config.h 中的 DEBUG_USB 控制

// ==================== Communication AO实例 ====================

AO_Communication_t AO_Communication_inst;
QActive * const AO_Communication = &AO_Communication_inst.super;

// ==================== 内部辅助函数声明 ====================

static void send_response(uint8_t id, uint8_t cmd, uint8_t resp_code, 
                         const uint8_t *data, uint16_t data_len);
static void handle_move_single(const protocol_frame_t *frame);
static void handle_move_all(const protocol_frame_t *frame);
static void handle_move_trapezoid(const protocol_frame_t *frame);
static void handle_get_single(const protocol_frame_t *frame);
static void handle_get_all(const protocol_frame_t *frame);
static void handle_enable(const protocol_frame_t *frame);
static void handle_set_start_positions(const protocol_frame_t *frame);
static void handle_ping(const protocol_frame_t *frame);

// ==================== 状态处理函数声明 ====================

static QState AO_Communication_initial(AO_Communication_t * const me, void const * const par);
static QState AO_Communication_active(AO_Communication_t * const me, QEvt const * const e);

// ==================== 构造函数 ====================

void AO_Communication_ctor(void) {
    AO_Communication_t *me = &AO_Communication_inst;
    
    QActive_ctor(&me->super, Q_STATE_CAST(&AO_Communication_initial));
    QTimeEvt_ctorX(&me->usb_poll_timer, &me->super, USB_RX_DATA_SIG, 0U);
    
    ring_buffer_init(&me->rx_buffer, me->rx_buffer_mem, USB_RX_BUFFER_SIZE);
    ring_buffer_init(&me->tx_buffer, me->tx_buffer_mem, USB_TX_BUFFER_SIZE);
    protocol_parser_init(&me->parser);
    
    me->usb_connected = false;
    me->cmd_count = 0;
}

// ==================== 状态机实现 ====================

static QState AO_Communication_initial(AO_Communication_t * const me, void const * const par) {
    (void)par;
    #if DEBUG_USB
    LOG_DEBUG("[AO-COMM] Initial state\n");
    #endif
    QTimeEvt_armX(&me->usb_poll_timer, 10, 10);
    return Q_TRAN(&AO_Communication_active);
}

static QState AO_Communication_active(AO_Communication_t * const me, QEvt const * const e) {
    QState status;
    
    switch (e->sig) {
        case Q_ENTRY_SIG: {
            #if DEBUG_USB
            LOG_DEBUG("[AO-COMM] Entering ACTIVE state\n");
            #endif
            status = Q_HANDLED();
            break;
        }
        
        case USB_RX_DATA_SIG: {
            // 调试：确认事件触发
            static uint32_t event_count = 0;
            event_count++;
            
            #if DEBUG_USB
            if (event_count % 100 == 0) {  // 每100次打印一次
                LOG_DEBUG("[USB] Event #%lu triggered\n", event_count);
            }
            #endif
            
            // USB操作由Core 1负责，这里只从Bridge读取
            me->usb_connected = true;  // USB Bridge启动后即可用
            
            // 调试：每秒打印一次状态（64KB缓冲区足够）
            static uint32_t poll_count = 0;
            poll_count++;
            #if DEBUG_USB
            if (poll_count % 100 == 0) {  // 10ms * 100 = 1s
                LOG_DEBUG("[USB] Poll #%lu\n", poll_count);
            }
            #endif
            
            #if DEBUG_USB
            uint32_t available = usb_bridge_available();
            if (available > 0) {
                LOG_DEBUG("[USB] Bridge has %lu bytes available\n", available);
            }
            #endif
            
            if (me->usb_connected) {
                uint32_t rx_count = 0;
                while (usb_bridge_available() > 0) {
                    int ch = usb_bridge_getchar();
                    if (ch < 0) break;
                    uint8_t byte = (uint8_t)ch;
                    ring_buffer_put(&me->rx_buffer, byte);
                    rx_count++;
                }
                
                #if DEBUG_USB
                if (rx_count > 0) {
                    LOG_DEBUG("[USB] RX: %lu bytes, starting parse...\n", rx_count);
                }
                #endif
                
                uint8_t byte;
                uint32_t parse_count = 0;
                bool entered_loop = false;
                
                while (ring_buffer_get(&me->rx_buffer, &byte)) {
                    if (!entered_loop) {
                        #if DEBUG_USB
                        LOG_DEBUG("[USB] Entered parse loop, first byte: 0x%02X\n", byte);
                        #endif
                        entered_loop = true;
                    }
                    parse_count++;
                    bool frame_complete = protocol_parse_byte(&me->parser, byte);
                    
                    #if DEBUG_USB
                    if (frame_complete) {
                        usb_bridge_printf("[USB] Frame parsing complete\n");
                    }
                    #endif
                    
                    if (frame_complete) {
                        const protocol_frame_t *frame = protocol_get_frame(&me->parser);
                        
                        #if DEBUG_USB
                        if (frame == NULL) {
                            usb_bridge_printf("[USB] ERROR: Frame is NULL after parsing!\n");
                        }
                        #endif
                        
                        if (frame != NULL) {
                            me->cmd_count++;
                            #if DEBUG_USB
                            usb_bridge_printf("[USB] CMD: 0x%02X, len=%d\n", frame->cmd, frame->len);
                            #endif
                            
                            switch (frame->cmd) {
                                case CMD_MOVE_SINGLE:
                                    handle_move_single(frame);
                                    break;
                                case CMD_MOVE_ALL:
                                    handle_move_all(frame);
                                    break;
                                case CMD_MOVE_TRAPEZOID:
                                    handle_move_trapezoid(frame);
                                    break;
                                case CMD_GET_SINGLE:
                                    handle_get_single(frame);
                                    break;
                                case CMD_GET_ALL:
                                    handle_get_all(frame);
                                    break;
                                case CMD_ENABLE:
                                case CMD_DISABLE:
                                    handle_enable(frame);
                                    break;
                                case CMD_SAVE_FLASH:
                                    {
                                        static QEvt const flash_save = QEVT_INITIALIZER(CMD_FLASH_SAVE_SIG);
                                        QACTIVE_POST(AO_System, &flash_save, me);
                                    }
                                    send_response(frame->id, frame->cmd, RESP_OK, NULL, 0);
                                    break;
                                case CMD_LOAD_FLASH:
                                    {
                                        static QEvt const flash_load = QEVT_INITIALIZER(CMD_FLASH_LOAD_SIG);
                                        QACTIVE_POST(AO_System, &flash_load, me);
                                    }
                                    send_response(frame->id, frame->cmd, RESP_OK, NULL, 0);
                                    break;
                                case CMD_SET_START_POSITIONS:
                                    handle_set_start_positions(frame);
                                    break;
                                case CMD_PING:
                                    handle_ping(frame);
                                    break;
                                case CMD_ESTOP:
                                    {
                                        static QEvt const estop = QEVT_INITIALIZER(ESTOP_SIG);
                                        QACTIVE_POST(AO_System, &estop, me);
                                        QACTIVE_POST(AO_Motion, &estop, me);
                                    }
                                    send_response(frame->id, frame->cmd, RESP_OK, NULL, 0);
                                    break;
                                default:
                                    send_response(frame->id, frame->cmd, RESP_INVALID_CMD, NULL, 0);
                                    break;
                            }
                        }
                        protocol_parser_reset(&me->parser);
                    }
                }
                
                #if DEBUG_USB
                if (rx_count > 0 && !entered_loop) {
                    LOG_DEBUG("[USB] WARNING: Had RX data but never entered parse loop!\n");
                }
                if (parse_count > 0) {
                    LOG_DEBUG("[USB] Parsed %lu bytes total\n", parse_count);
                }
                #endif
                
                // 发送TX缓冲区中的数据
                if (!ring_buffer_is_empty(&me->tx_buffer)) {
                    #if DEBUG_USB
                    LOG_DEBUG("[USB] TX buffer not empty, attempting to send...\n");
                    #endif
                    
                    // 发送数据到USB Bridge
                    uint8_t tx_byte;
                    size_t sent = 0;
                    uint8_t tx_buffer_temp[64];
                    while (ring_buffer_get(&me->tx_buffer, &tx_byte) && sent < 64) {
                        tx_buffer_temp[sent++] = tx_byte;
                    }
                    if (sent > 0) {
                        usb_bridge_write(tx_buffer_temp, sent);
                        #if DEBUG_USB
                        LOG_DEBUG("[USB] TX: sent %d bytes\n", sent);
                        #endif
                    } else {
                        #if DEBUG_USB
                        LOG_DEBUG("[USB] TX: write not available\n");
                        #endif
                    }
                }
            }
            status = Q_HANDLED();
            break;
        }
        
        default: {
            status = Q_SUPER(&QHsm_top);
            break;
        }
    }
    
    return status;
}

// ==================== 命令处理函数实现 ====================

static void handle_move_single(const protocol_frame_t *frame) {
    #if DEBUG_USB
        LOG_DEBUG("[CMD] MOVE_SINGLE handler called\n");
    #endif
    
    if (frame->len < 5) {
        #if DEBUG_USB
        LOG_DEBUG("[CMD] Invalid len: %d < 5\n", frame->len);
        #endif
        send_response(frame->id, frame->cmd, RESP_INVALID_PARAM, NULL, 0);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    uint16_t angle = (frame->data[1] << 8) | frame->data[2];
    uint16_t duration = (frame->data[3] << 8) | frame->data[4];
    
    #if DEBUG_USB
    LOG_DEBUG("[CMD] Parsed: Servo=%d, Angle=%d (%.1f°), Duration=%dms\n",
           servo_id, angle, (float)angle/100.0f, duration);
    #endif
    
    // 先检查参数再分配事件（避免内存泄漏）
    if (servo_id >= SERVO_COUNT) {
        #if DEBUG_USB
        LOG_DEBUG("[CMD] Invalid servo ID: %d >= %d\n", servo_id, SERVO_COUNT);
        #endif
        send_response(frame->id, frame->cmd, RESP_INVALID_PARAM, NULL, 0);
        return;
    }
    
    // 参数验证通过，分配事件
    MotionStartEvt *evt = Q_NEW(MotionStartEvt, MOTION_START_SIG);
    evt->axis_count = 1;
    evt->axis_ids[0] = servo_id;
    evt->duration_ms = duration;
    
    // 【修复Bug】：填充完整的target_positions数组
    // 将所有舵机的目标位置设为当前位置，只修改指定的servo_id
    for (uint8_t i = 0; i < SERVO_COUNT; i++) {
        evt->target_positions[i] = servo_get_angle(i);  // 保持当前角度
    }
    evt->target_positions[servo_id] = (float)angle / 100.0f;  // 只修改指定舵机的目标
    
    #if DEBUG_USB
    LOG_DEBUG("[CMD] target_positions[%d]=%.1f (others keep current)\n", 
              servo_id, evt->target_positions[servo_id]);
    LOG_DEBUG("[CMD] Posting MOTION_START to AO_Motion\n");
    #endif
    QACTIVE_POST(AO_Motion, &evt->super, AO_Communication);
    
    #if DEBUG_USB
    LOG_DEBUG("[CMD] Sending OK response\n");
    #endif
    send_response(frame->id, frame->cmd, RESP_OK, NULL, 0);
}

static void handle_move_all(const protocol_frame_t *frame) {
    #if DEBUG_USB
    LOG_DEBUG("[CMD] MOVE_ALL handler called, len=%d\n", frame->len);
    #endif
    
    if (frame->len < 38) {
        #if DEBUG_USB
        LOG_DEBUG("[CMD] Invalid len: %d < 38\n", frame->len);
        #endif
        send_response(frame->id, frame->cmd, RESP_INVALID_PARAM, NULL, 0);
        return;
    }
    
    #if DEBUG_USB
    LOG_DEBUG("[CMD] Allocating MotionStartEvt...\n");
    #endif
    
    // 参数验证通过，分配事件
    MotionStartEvt *evt = Q_NEW(MotionStartEvt, MOTION_START_SIG);
    
    #if DEBUG_USB
    LOG_DEBUG("[CMD] Event allocated: %p\n", (void*)evt);
    #endif
    
    evt->axis_count = SERVO_COUNT;
    
    for (uint8_t i = 0; i < SERVO_COUNT; i++) {
        uint16_t angle = (frame->data[i*2] << 8) | frame->data[i*2+1];
        evt->target_positions[i] = (float)angle / 100.0f;
        evt->axis_ids[i] = i;
    }
    
    evt->duration_ms = (frame->data[36] << 8) | frame->data[37];
    
    #if DEBUG_USB
    LOG_DEBUG("[CMD] Parsed: %d axes, duration=%d ms\n", evt->axis_count, evt->duration_ms);
    LOG_DEBUG("[CMD] Target angles: %.1f %.1f %.1f ...\n", 
           evt->target_positions[0], evt->target_positions[1], evt->target_positions[2]);
    LOG_DEBUG("[CMD] Posting to AO_Motion...\n");
    #endif
    
    QACTIVE_POST(AO_Motion, &evt->super, AO_Communication);
    
    #if DEBUG_USB
    LOG_DEBUG("[CMD] Sending OK response...\n");
    #endif
    
    send_response(frame->id, frame->cmd, RESP_OK, NULL, 0);
    
    #if DEBUG_USB
    LOG_DEBUG("[CMD] MOVE_ALL handler complete\n");
    #endif
}

static void handle_move_trapezoid(const protocol_frame_t *frame) {
    // 数据格式（9字节）：
    // [0]: servo_id
    // [1-2]: target_angle (int16, 角度×100)
    // [3-4]: max_velocity (uint16, 度/秒×10)
    // [5-6]: acceleration (uint16, 度/秒²×10)
    // [7-8]: deceleration (uint16, 度/秒²×10, 如果为0则使用acceleration)
    if (frame->len < 9) {
        send_response(frame->id, frame->cmd, RESP_INVALID_PARAM, NULL, 0);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    if (servo_id >= SERVO_COUNT) {
        send_response(frame->id, frame->cmd, RESP_INVALID_PARAM, NULL, 0);
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
    // 命令调试信息（整数格式）
    int angle_int = (int)target_angle;
    int vel_int = velocity_raw;  // 已经是×10
    int acc_int = accel_raw;
    LOG_DEBUG("[CMD] TRAPEZOID: Servo%d->%ddeg v=%d.%d a=%d.%d\n",
              servo_id, angle_int, vel_int/10, vel_int%10, acc_int/10, acc_int%10);
    #endif
    
    // 调用AO_Motion的梯形速度接口设置插值器
    bool result = AO_Motion_set_trapezoid(servo_id, target_angle, &params);
    
    if (result) {
        // 重要！发送MOTION_START事件，触发AO_Motion状态转换到moving
        // 这样才会执行插值更新循环
        MotionStartEvt *start_evt = Q_NEW(MotionStartEvt, MOTION_START_SIG);
        start_evt->axis_count = 1;
        start_evt->axis_ids[0] = servo_id;
        start_evt->duration_ms = 0;  // 梯形速度模式不使用这个参数
        
        // 填充target_positions（保持其他舵机不动）
        for (uint8_t i = 0; i < SERVO_COUNT; i++) {
            start_evt->target_positions[i] = servo_get_angle(i);
        }
        start_evt->target_positions[servo_id] = target_angle;
        
        QACTIVE_POST(AO_Motion, &start_evt->super, AO_Communication);
        
        send_response(frame->id, frame->cmd, RESP_OK, NULL, 0);
    } else {
        send_response(frame->id, frame->cmd, RESP_ERROR, NULL, 0);
    }
}

static void handle_get_single(const protocol_frame_t *frame) {
    if (frame->len < 1) {
        send_response(frame->id, frame->cmd, RESP_INVALID_PARAM, NULL, 0);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    if (servo_id >= SERVO_COUNT) {
        send_response(frame->id, frame->cmd, RESP_INVALID_PARAM, NULL, 0);
        return;
    }
    
    float angle = servo_get_angle(servo_id);
    uint16_t angle_x100 = (uint16_t)(angle * 100.0f);
    
    uint8_t resp_data[4];
    resp_data[0] = servo_id;
    resp_data[1] = (angle_x100 >> 8) & 0xFF;
    resp_data[2] = angle_x100 & 0xFF;
    resp_data[3] = servo_is_enabled(servo_id) ? 1 : 0;
    
    send_response(frame->id, frame->cmd, RESP_OK, resp_data, 4);
}

static void handle_get_all(const protocol_frame_t *frame) {
    (void)frame;
    uint8_t resp_data[SERVO_COUNT * 3];
    
    for (uint8_t i = 0; i < SERVO_COUNT; i++) {
        float angle = servo_get_angle(i);
        uint16_t angle_x100 = (uint16_t)(angle * 100.0f);
        
        resp_data[i*3] = i;
        resp_data[i*3+1] = (angle_x100 >> 8) & 0xFF;
        resp_data[i*3+2] = angle_x100 & 0xFF;
    }
    
    send_response(frame->id, frame->cmd, RESP_OK, resp_data, SERVO_COUNT * 3);
}

static void handle_enable(const protocol_frame_t *frame) {
    if (frame->len < 1) {
        send_response(frame->id, frame->cmd, RESP_INVALID_PARAM, NULL, 0);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    bool enable = (frame->cmd == CMD_ENABLE);
    
    servo_manager_enable(servo_id, enable);
    send_response(frame->id, frame->cmd, RESP_OK, NULL, 0);
}

static void handle_set_start_positions(const protocol_frame_t *frame) {
    #if DEBUG_USB
    LOG_DEBUG("[CMD] SET_START_POSITIONS handler called\n");
    #endif
    
    // 数据格式：[angle0_H][angle0_L] ... [angle17_H][angle17_L]
    // 总长度：18 * 2 = 36字节
    if (frame->len < 36) {
        #if DEBUG_USB
        LOG_DEBUG("[CMD] Invalid len: %d < 36\n", frame->len);
        #endif
        send_response(frame->id, frame->cmd, RESP_INVALID_PARAM, NULL, 0);
        return;
    }
    
    #if DEBUG_USB
    LOG_DEBUG("[CMD] Parsing 18 angles...\n");
    #endif
    
    // 解析18个角度
    float angles[SERVO_COUNT];
    for (int i = 0; i < SERVO_COUNT; i++) {
        uint16_t angle_x100 = (frame->data[i*2] << 8) | frame->data[i*2 + 1];
        angles[i] = angle_x100 / 100.0f;
    }
    
    #if DEBUG_USB
    LOG_DEBUG("[CMD] Calling param_manager_set_start_positions...\n");
    #endif
    
    // 保存到Flash
    if (param_manager_set_start_positions(angles)) {
        #if DEBUG_USB
        LOG_DEBUG("[CMD] Set start positions: OK\n");
        #endif
        send_response(frame->id, frame->cmd, RESP_OK, NULL, 0);
    } else {
        #if DEBUG_USB
        LOG_DEBUG("[CMD] Set start positions: FAIL\n");
        #endif
        send_response(frame->id, frame->cmd, RESP_ERROR, NULL, 0);
    }
    
    #if DEBUG_USB
    LOG_DEBUG("[CMD] SET_START_POSITIONS handler finished\n");
    #endif
}

static void handle_ping(const protocol_frame_t *frame) {
    uint8_t resp_data[4] = {0x50, 0x4F, 0x4E, 0x47};  // "PONG"
    send_response(frame->id, frame->cmd, RESP_OK, resp_data, 4);
}

static void send_response(uint8_t id, uint8_t cmd, uint8_t resp_code,
                         const uint8_t *data, uint16_t data_len) {
    uint8_t resp_buffer[128];
    uint16_t idx = 0;
    
    resp_buffer[idx++] = PROTOCOL_FRAME_HEADER1;
    resp_buffer[idx++] = PROTOCOL_FRAME_HEADER2;
    resp_buffer[idx++] = id;
    resp_buffer[idx++] = cmd;
    resp_buffer[idx++] = data_len + 1;
    resp_buffer[idx++] = resp_code;
    
    if (data != NULL && data_len > 0) {
        memcpy(&resp_buffer[idx], data, data_len);
        idx += data_len;
    }
    
    uint16_t crc = crc16_ccitt(&resp_buffer[2], idx - 2);
    resp_buffer[idx++] = (crc >> 8) & 0xFF;
    resp_buffer[idx++] = crc & 0xFF;
    
    AO_Communication_t *me = &AO_Communication_inst;
    ring_buffer_write(&me->tx_buffer, resp_buffer, idx);
    
    #if DEBUG_USB
    LOG_DEBUG("[RESP] Built response: len=%d, resp_code=%d\n", 
           idx, resp_code);
    LOG_DEBUG("[RESP] Frame: ");
    for (uint16_t i = 0; i < idx && i < 32; i++) {
        LOG_DEBUG("%02X ", resp_buffer[i]);
    }
    LOG_DEBUG("\n");
    #endif
}

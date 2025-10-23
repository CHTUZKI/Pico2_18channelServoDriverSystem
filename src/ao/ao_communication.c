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
#include "utils/ring_buffer.h"
#include "tusb.h"
#include <stdio.h>
#include <string.h>

Q_DEFINE_THIS_FILE

// ==================== 调试开关 ====================
// 设置为0关闭USB调试输出，设置为1打开调试输出
#define USB_DEBUG_ENABLE 0

// ==================== Communication AO实例 ====================

AO_Communication_t AO_Communication_inst;
QActive * const AO_Communication = &AO_Communication_inst.super;

// ==================== 内部辅助函数声明 ====================

static void send_response(uint8_t id, uint8_t cmd, uint8_t resp_code, 
                         const uint8_t *data, uint16_t data_len);
static void handle_move_single(const protocol_frame_t *frame);
static void handle_move_all(const protocol_frame_t *frame);
static void handle_get_single(const protocol_frame_t *frame);
static void handle_get_all(const protocol_frame_t *frame);
static void handle_enable(const protocol_frame_t *frame);
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
    #if USB_DEBUG_ENABLE
    printf("[AO-COMM] Initial state\n");
    #endif
    QTimeEvt_armX(&me->usb_poll_timer, 10, 10);
    return Q_TRAN(&AO_Communication_active);
}

static QState AO_Communication_active(AO_Communication_t * const me, QEvt const * const e) {
    QState status;
    
    switch (e->sig) {
        case Q_ENTRY_SIG: {
            #if USB_DEBUG_ENABLE
            printf("[AO-COMM] Entering ACTIVE state\n");
            #endif
            status = Q_HANDLED();
            break;
        }
        
        case USB_RX_DATA_SIG: {
            tud_task();
            me->usb_connected = tud_cdc_connected();
            
            // 调试：每秒打印一次USB状态
            static uint32_t poll_count = 0;
            poll_count++;
            #if USB_DEBUG_ENABLE
            if (poll_count % 100 == 0) {  // 10ms * 100 = 1s
                printf("[USB] Poll #%lu, Connected: %d\n", poll_count, me->usb_connected);
            }
            #endif
            
            if (me->usb_connected) {
                uint32_t rx_count = 0;
                while (tud_cdc_available()) {
                    uint8_t byte = tud_cdc_read_char();
                    ring_buffer_put(&me->rx_buffer, byte);
                    rx_count++;
                }
                
                #if USB_DEBUG_ENABLE
                if (rx_count > 0) {
                    printf("[USB] RX: %lu bytes\n", rx_count);
                }
                #endif
                
                uint8_t byte;
                while (ring_buffer_get(&me->rx_buffer, &byte)) {
                    if (protocol_parse_byte(&me->parser, byte)) {
                        const protocol_frame_t *frame = protocol_get_frame(&me->parser);
                        
                        if (frame != NULL) {
                            me->cmd_count++;
                            #if USB_DEBUG_ENABLE
                            printf("[USB] CMD: 0x%02X, len=%d\n", frame->cmd, frame->len);
                            #endif
                            
                            switch (frame->cmd) {
                                case CMD_MOVE_SINGLE:
                                    handle_move_single(frame);
                                    break;
                                case CMD_MOVE_ALL:
                                    handle_move_all(frame);
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
                
                if (!ring_buffer_is_empty(&me->tx_buffer) && tud_cdc_write_available()) {
                    uint8_t tx_byte;
                    size_t sent = 0;
                    while (ring_buffer_get(&me->tx_buffer, &tx_byte) && sent < 64) {
                        tud_cdc_write_char(tx_byte);
                        sent++;
                    }
                    if (sent > 0) {
                        tud_cdc_write_flush();
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
    #if USB_DEBUG_ENABLE
    printf("[CMD] MOVE_SINGLE handler called\n");
    #endif
    
    if (frame->len < 5) {
        #if USB_DEBUG_ENABLE
        printf("[CMD] Invalid len: %d < 5\n", frame->len);
        #endif
        send_response(frame->id, frame->cmd, RESP_INVALID_PARAM, NULL, 0);
        return;
    }
    
    uint8_t servo_id = frame->data[0];
    uint16_t angle = (frame->data[1] << 8) | frame->data[2];
    uint16_t duration = (frame->data[3] << 8) | frame->data[4];
    
    #if USB_DEBUG_ENABLE
    printf("[CMD] Parsed: Servo=%d, Angle=%d (%.1f°), Duration=%dms\n",
           servo_id, angle, (float)angle/100.0f, duration);
    #endif
    
    // 先检查参数再分配事件（避免内存泄漏）
    if (servo_id >= SERVO_COUNT) {
        #if USB_DEBUG_ENABLE
        printf("[CMD] Invalid servo ID: %d >= %d\n", servo_id, SERVO_COUNT);
        #endif
        send_response(frame->id, frame->cmd, RESP_INVALID_PARAM, NULL, 0);
        return;
    }
    
    // 参数验证通过，分配事件
    MotionStartEvt *evt = Q_NEW(MotionStartEvt, MOTION_START_SIG);
    evt->axis_count = 1;
    evt->axis_ids[0] = servo_id;
    evt->target_positions[0] = (float)angle / 100.0f;
    evt->duration_ms = duration;
    
    #if USB_DEBUG_ENABLE
    printf("[CMD] Posting MOTION_START to AO_Motion\n");
    #endif
    QACTIVE_POST(AO_Motion, &evt->super, AO_Communication);
    
    #if USB_DEBUG_ENABLE
    printf("[CMD] Sending OK response\n");
    #endif
    send_response(frame->id, frame->cmd, RESP_OK, NULL, 0);
}

static void handle_move_all(const protocol_frame_t *frame) {
    if (frame->len < 38) {
        send_response(frame->id, frame->cmd, RESP_INVALID_PARAM, NULL, 0);
        return;
    }
    
    // 参数验证通过，分配事件
    MotionStartEvt *evt = Q_NEW(MotionStartEvt, MOTION_START_SIG);
    evt->axis_count = SERVO_COUNT;
    
    for (uint8_t i = 0; i < SERVO_COUNT; i++) {
        uint16_t angle = (frame->data[i*2] << 8) | frame->data[i*2+1];
        evt->target_positions[i] = (float)angle / 100.0f;
        evt->axis_ids[i] = i;
    }
    
    evt->duration_ms = (frame->data[36] << 8) | frame->data[37];
    
    QACTIVE_POST(AO_Motion, &evt->super, AO_Communication);
    send_response(frame->id, frame->cmd, RESP_OK, NULL, 0);
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
    size_t written = ring_buffer_write(&me->tx_buffer, resp_buffer, idx);
    
    #if USB_DEBUG_ENABLE
    printf("[RESP] Built response: len=%d, written=%d, resp_code=%d\n", 
           idx, written, resp_code);
    #endif
}

/**
 * @file pinout.h
 * @brief GPIO引脚定义
 * @date 2025-10-21
 */

#ifndef PINOUT_H
#define PINOUT_H

// ==================== 舵机PWM输出引脚 (GPIO 0-17) ====================
#define SERVO_PIN_0     0       // 舵机0 - 左前腿髋关节
#define SERVO_PIN_1     1       // 舵机1 - 左前腿大腿
#define SERVO_PIN_2     2       // 舵机2 - 左前腿小腿
#define SERVO_PIN_3     3       // 舵机3 - 右前腿髋关节
#define SERVO_PIN_4     4       // 舵机4 - 右前腿大腿
#define SERVO_PIN_5     5       // 舵机5 - 右前腿小腿
#define SERVO_PIN_6     6       // 舵机6 - 左中腿髋关节
#define SERVO_PIN_7     7       // 舵机7 - 左中腿大腿
#define SERVO_PIN_8     8       // 舵机8 - 左中腿小腿
#define SERVO_PIN_9     9       // 舵机9 - 右中腿髋关节
#define SERVO_PIN_10    10      // 舵机10 - 右中腿大腿
#define SERVO_PIN_11    11      // 舵机11 - 右中腿小腿
#define SERVO_PIN_12    12      // 舵机12 - 左后腿髋关节
#define SERVO_PIN_13    13      // 舵机13 - 左后腿大腿
#define SERVO_PIN_14    14      // 舵机14 - 左后腿小腿
#define SERVO_PIN_15    15      // 舵机15 - 右后腿髋关节
#define SERVO_PIN_16    18      // 舵机16 - 右后腿大腿（使用GPIO18避免与GPIO0冲突）
#define SERVO_PIN_17    19      // 舵机17 - 右后腿小腿（使用GPIO19避免与GPIO1冲突）

// 舵机引脚数组 (方便遍历)
static const uint8_t SERVO_PINS[18] = {
    SERVO_PIN_0,  SERVO_PIN_1,  SERVO_PIN_2,
    SERVO_PIN_3,  SERVO_PIN_4,  SERVO_PIN_5,
    SERVO_PIN_6,  SERVO_PIN_7,  SERVO_PIN_8,
    SERVO_PIN_9,  SERVO_PIN_10, SERVO_PIN_11,
    SERVO_PIN_12, SERVO_PIN_13, SERVO_PIN_14,
    SERVO_PIN_15, SERVO_PIN_16, SERVO_PIN_17
};

// ==================== 系统功能引脚 ====================
#define PIN_LED_BUILTIN     25      // 板载LED (GP25)

// ==================== USB通信 ====================
// USB使用硬件接口，无需GPIO定义

// ==================== 预留扩展引脚 ====================
// GPIO 20-24: 可用于传感器、调试等
// 注意：GPIO 18-19 已被舵机16-17占用
#define PIN_DEBUG_TX        20      // 调试串口TX (可选)
#define PIN_DEBUG_RX        21      // 调试串口RX (可选)

// GPIO 26-28: ADC功能引脚
#define PIN_ADC0            26      // ADC通道0 (电源监测)
#define PIN_ADC1            27      // ADC通道1 (预留)
#define PIN_ADC2            28      // ADC通道2 (预留)

#endif // PINOUT_H


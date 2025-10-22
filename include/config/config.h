/**
 * @file config.h
 * @brief 系统配置参数
 * @date 2025-10-21
 */

#ifndef CONFIG_H
#define CONFIG_H

// ==================== 系统参数 ====================
#define SYSTEM_VERSION_MAJOR    1
#define SYSTEM_VERSION_MINOR    0
#define SYSTEM_VERSION_PATCH    0

// ==================== 舵机参数 ====================
#define SERVO_COUNT             18          // 舵机数量

// 180度位置舵机参数
#define SERVO_MIN_PULSE_US      500         // 最小脉宽 (μs)
#define SERVO_MAX_PULSE_US      2500        // 最大脉宽 (μs)
#define SERVO_CENTER_PULSE_US   1500        // 中位脉宽 (μs)
#define SERVO_MIN_ANGLE         0.0f        // 最小角度 (度)
#define SERVO_MAX_ANGLE         180.0f      // 最大角度 (度)

// 360度连续旋转舵机参数
#define SERVO_360_MIN_PULSE_US          500     // 最小脉宽 (满速反转)
#define SERVO_360_MAX_PULSE_US          2500    // 最大脉宽 (满速正转)
#define SERVO_360_NEUTRAL_PULSE_US      1500    // 中点脉宽 (停止)
#define SERVO_360_DEADZONE_US           50      // 死区范围 (±μs)
#define SERVO_360_MIN_SPEED_THRESHOLD   5       // 最小速度阈值 (%, 防抖动)
#define SERVO_360_DEFAULT_ACCEL         50      // 默认加速度 (%/s)
#define SERVO_360_DEFAULT_DECEL         80      // 默认减速度 (%/s)
#define SERVO_360_SOFT_STOP_FACTOR      0.90f   // 软停止衰减系数
#define SERVO_360_DIRECTION_CHANGE_DELAY_MS  200  // 方向切换延迟 (ms)
#define SERVO_360_TIMEOUT_MS            3000    // 超时保护 (ms)

// ==================== PWM参数 ====================
#define PWM_FREQUENCY_HZ        50          // PWM频率 50Hz
#define PWM_PERIOD_MS           20          // PWM周期 20ms
#define PWM_PERIOD_US           20000       // PWM周期 20000μs
#define PWM_RESOLUTION_US       1           // PWM分辨率 1μs

// PIO时钟频率 (1MHz = 1μs分辨率)
#define PIO_CLOCK_FREQ          1000000     // 1 MHz

// ==================== 通信参数 ====================
#define USB_BAUDRATE            115200      // USB波特率
#define USB_RX_BUFFER_SIZE      512         // 接收缓冲区大小
#define USB_TX_BUFFER_SIZE      512         // 发送缓冲区大小
#define USB_TIMEOUT_MS          100         // USB超时时间

// 协议参数
#define PROTOCOL_FRAME_HEADER1  0xFF        // 帧头1
#define PROTOCOL_FRAME_HEADER2  0xFE        // 帧头2
#define PROTOCOL_MAX_DATA_LEN   128         // 最大数据长度
#define PROTOCOL_TIMEOUT_MS     1000        // 协议超时

// ==================== 运动控制参数 ====================
#define INTERPOLATION_PERIOD_MS 20          // 插值周期 (与PWM同步)
#define DEFAULT_MOVE_TIME_MS    1000        // 默认运动时间
#define MIN_MOVE_TIME_MS        20          // 最小运动时间
#define MAX_MOVE_TIME_MS        10000       // 最大运动时间

// ==================== FreeRTOS参数 ====================
// 任务堆栈大小
#define TASK_STACK_SIZE_COMM    2048        // 通信任务
#define TASK_STACK_SIZE_MOTION  2048        // 运动任务
#define TASK_STACK_SIZE_PWM     1024        // PWM任务
#define TASK_STACK_SIZE_STATUS  1024        // 状态任务

// 任务优先级
#define TASK_PRIORITY_COMM      3           // 通信任务 (高)
#define TASK_PRIORITY_MOTION    2           // 运动任务 (中)
#define TASK_PRIORITY_PWM       4           // PWM任务 (最高)
#define TASK_PRIORITY_STATUS    1           // 状态任务 (低)

// 队列大小
#define QUEUE_SIZE_CMD          10          // 命令队列
#define QUEUE_SIZE_STATUS       10          // 状态队列

// ==================== Flash存储参数 ====================
#define FLASH_STORAGE_OFFSET    (256 * 1024)    // Flash偏移地址 (256KB)
#ifndef FLASH_SECTOR_SIZE
#define FLASH_SECTOR_SIZE       4096            // Flash扇区大小（如SDK已定义则使用SDK值）
#endif
#define FLASH_PARAM_VERSION     1               // 参数版本号

// ==================== 安全参数 ====================
#define WATCHDOG_TIMEOUT_MS     5000        // 看门狗超时时间
#define COMM_TIMEOUT_MS         3000        // 通信超时时间
#define ESTOP_DELAY_MS          10          // 急停响应时间

// ==================== LED指示 ====================
#define LED_BLINK_NORMAL_MS     1000        // 正常闪烁周期
#define LED_BLINK_ERROR_MS      200         // 错误闪烁周期
#define LED_BLINK_COMM_MS       50          // 通信闪烁

#endif // CONFIG_H


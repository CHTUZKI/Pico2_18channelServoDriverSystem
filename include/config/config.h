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

// ==================== 调试开关 ====================
// 主开关
#define DEBUG_ENABLE            1       // 主调试开关（关闭后禁用所有调试输出）

// 启动和系统信息（建议始终开启）
#define DEBUG_STARTUP           1       // 启动信息（版本、平台、初始化状态）
#define DEBUG_SYSTEM            0       // 系统心跳（LED闪烁计数）

// 通信相关调试（当前关闭，减少输出）
#define DEBUG_USB               0       // USB底层（Event轮询、缓冲区状态）
#define DEBUG_PROTOCOL          0       // 协议解析（帧解析、CRC校验）
#define DEBUG_COMMAND           1       // 命令处理（显示接收到的命令）✅ 开启
#define DEBUG_AO_COMM           0       // Communication AO状态机

// 运动控制调试
#define DEBUG_MOTION            0       // 运动插值每次更新（会很多，不推荐）
#define DEBUG_MOTION_SUMMARY    1       // 运动摘要（开始参数、曲线分段）✅ 开启
#define DEBUG_MOTION_PROGRESS   1       // 运动进度（25%/50%/75%关键点）✅ 开启
#define DEBUG_AO_MOTION         0       // Motion AO状态转换（已优化，可关闭）

// 运动缓冲区和调度器调试（新架构）
#define DEBUG_SCHEDULER         1       // 调度器执行（显示指令触发）✅ 开启
#define DEBUG_BUFFER            1       // 缓冲区操作（添加/移除指令）✅ 开启

// 舵机和PWM调试
#define DEBUG_SERVO             0       // 舵机角度转换（每次都输出，太多）
#define DEBUG_PWM               0       // PWM底层设置（每次都输出，太多）
#define DEBUG_PWM_SUMMARY       1       // PWM变化摘要（仅输出变化超过阈值的）✅ 新增

// 存储调试
#define DEBUG_FLASH             0       // Flash读写操作
#define DEBUG_PARAM             0       // 参数管理（启动时查看参数）

// ==================== 测试开关 ====================
#define ENABLE_AUTO_TEST        0       // 自动测试模式（上电自动运行测试序列）
#define ENABLE_GPIO_MONITOR     1       // GPIO监控器（Core1每2秒报告PWM状态）
#define PWM_MONITOR_INTERVAL_SEC  2     // PWM监控报告间隔（秒）

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

// ==================== QP/C参数 ====================
// 主动对象优先级
#define AO_PRIORITY_COMM        3           // 通信AO (高)
#define AO_PRIORITY_MOTION      2           // 运动AO (中)
#define AO_PRIORITY_SYSTEM      1           // 系统AO (低)

// 事件队列大小
#define AO_QUEUE_SIZE_COMM      10          // 通信AO队列
#define AO_QUEUE_SIZE_MOTION    5           // 运动AO队列
#define AO_QUEUE_SIZE_SYSTEM    5           // 系统AO队列

// 时间事件周期
#define TIME_EVENT_INTERP_MS    20          // 插值时间事件周期（20ms = 50Hz，更平滑）
#define TIME_EVENT_LED_MS       1000        // LED时间事件周期

// ==================== Flash存储参数 ====================
#define FLASH_STORAGE_OFFSET    (256 * 1024)    // Flash偏移地址 (256KB)
// FLASH_SECTOR_SIZE由Pico SDK定义，不要重复定义
// #ifndef FLASH_SECTOR_SIZE
// #define FLASH_SECTOR_SIZE       4096            // Flash扇区大小
// #endif
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


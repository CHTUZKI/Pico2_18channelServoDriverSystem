/**
 * @file planner.h
 * @brief 运动规划器 - Look-Ahead Motion Planner
 * @date 2025-10-25
 * 
 * 功能：
 * - 运动块缓冲区管理（环形队列）
 * - 前瞻规划（Look-Ahead Planning）
 * - 速度平滑过渡（Junction Speed Calculation）
 * - 时间戳驱动的自主调度
 * 
 * 设计理念：
 * - 工业级运动规划器架构
 * - 反向传递计算最大进入速度
 * - 前向传递确保速度连续
 * - 自动重新计算梯形曲线参数
 */

#ifndef PLANNER_H
#define PLANNER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "config/config.h"

// ==================== 配置参数 ====================

#define PLANNER_BUFFER_SIZE     32      // 规划缓冲区大小（32条指令）
#define PLANNER_TICK_MS         10      // 规划器检查间隔（10ms）

// 速度衔接参数
#define MIN_JUNCTION_SPEED      5.0f    // 最小衔接速度（度/秒）
#define JUNCTION_DEVIATION      0.05f   // 衔接偏差系数（越小越平滑，但速度越慢）

// ==================== 运动块结构 ====================

/**
 * @brief 规划块标志位
 */
typedef struct {
    uint8_t recalculate : 1;        // 需要重新规划
    uint8_t nominal_length : 1;     // 能否达到最大速度
    uint8_t junction_valid : 1;     // 衔接速度已计算
    uint8_t is_continuous : 1;      // 是否为360度连续旋转模式
    uint8_t reserved : 4;           // 保留
} plan_block_flags_t;

/**
 * @brief 运动规划块（整合原 motion_block 和规划参数）
 */
typedef struct {
    // ========== 原始运动指令 ==========
    uint32_t timestamp_ms;          // 执行时间戳（从start开始计时）
    uint8_t servo_id;               // 舵机ID (0-17)
    float target_angle;             // 目标角度（度）
    float max_velocity;             // 最大速度（度/秒）
    float acceleration;             // 加速度（度/秒²）
    float deceleration;             // 减速度（度/秒²，0表示与加速度相同）
    
    // ========== 几何参数 ==========
    float start_angle;              // 起始角度（规划器计算）
    float distance;                 // 移动距离（带符号）
    float abs_distance;             // 移动距离（绝对值）
    
    // ========== 速度规划参数 ==========
    float entry_speed;              // 进入速度（规划器计算）
    float exit_speed;               // 退出速度（规划器计算）
    float max_entry_speed;          // 最大允许进入速度
    float max_junction_speed;       // 最大衔接速度（与下一块）
    float nominal_speed;            // 标称速度（规划后的最大速度）
    
    // ========== 梯形曲线参数（规划后）==========
    float t_accel;                  // 加速阶段时间（秒）
    float t_const;                  // 匀速阶段时间（秒）
    float t_decel;                  // 减速阶段时间（秒）
    float v_max_actual;             // 实际达到的最大速度
    uint32_t duration_ms;           // 总时间（毫秒）
    
    // ========== 360度连续旋转参数（新增）==========
    int8_t target_speed_pct;        // 目标速度百分比 (-100 到 +100)
    int8_t entry_speed_pct;         // 进入速度百分比（规划器计算）
    int8_t exit_speed_pct;          // 退出速度百分比（规划器计算）
    uint8_t accel_rate;             // 加速度（%/秒）
    uint8_t decel_rate;             // 减速度（%/秒）
    
    // ========== 状态标志 ==========
    plan_block_flags_t flags;       // 标志位
    bool valid;                     // 是否有效
} plan_block_t;

/**
 * @brief 运动规划器（整合缓冲区+调度器+前瞻规划）
 */
typedef struct {
    // ========== 环形缓冲区 ==========
    plan_block_t blocks[PLANNER_BUFFER_SIZE];
    uint8_t head;                   // 写入位置（生产者）
    uint8_t tail;                   // 读取位置（消费者）
    uint8_t count;                  // 当前块数量
    
    // ========== 调度状态 ==========
    bool running;                   // 是否正在执行
    bool paused;                    // 是否暂停
    uint32_t start_time;            // 开始执行的绝对时间
    
    // ========== 规划状态 ==========
    bool recalculate_flag;          // 全局重新规划标志
    uint8_t last_servo_id;          // 上一个舵机ID（用于判断连续性）
    float last_target_angle;        // 上一个目标角度
} motion_planner_t;

/**
 * @brief 运动执行回调函数类型
 * @param block 规划块指针
 */
typedef void (*planner_execute_callback_t)(const plan_block_t *block);

// ==================== 初始化函数 ====================

/**
 * @brief 初始化运动规划器
 * @param planner 规划器指针
 */
void planner_init(motion_planner_t *planner);

/**
 * @brief 设置运动执行回调函数
 * @param callback 回调函数指针
 */
void planner_set_callback(planner_execute_callback_t callback);

/**
 * @brief 获取全局运动规划器指针
 * @return 规划器指针
 */
motion_planner_t* planner_get_instance(void);

// ==================== 缓冲区管理 ====================

/**
 * @brief 添加运动块到规划器（位置模式 - 180度舵机）
 * @param timestamp_ms 时间戳
 * @param servo_id 舵机ID
 * @param target_angle 目标角度
 * @param velocity 速度
 * @param acceleration 加速度
 * @param deceleration 减速度（0表示使用加速度）
 * @return true 成功, false 失败（缓冲区满）
 */
bool planner_add_motion(uint32_t timestamp_ms, 
                       uint8_t servo_id,
                       float target_angle,
                       float velocity,
                       float acceleration,
                       float deceleration);

/**
 * @brief 添加360度舵机速度控制块到规划器（速度模式 - 360度舵机）
 * @param timestamp_ms 时间戳
 * @param servo_id 舵机ID
 * @param target_speed_pct 目标速度百分比 (-100 到 +100)
 * @param accel_rate 加速度 (%/秒)
 * @param decel_rate 减速度 (%/秒，0表示使用加速度）
 * @param duration_ms 持续时间（毫秒，0表示持续到下一个块）
 * @return true 成功, false 失败（缓冲区满）
 */
bool planner_add_continuous_motion(uint32_t timestamp_ms,
                                   uint8_t servo_id,
                                   int8_t target_speed_pct,
                                   uint8_t accel_rate,
                                   uint8_t decel_rate,
                                   uint32_t duration_ms);

/**
 * @brief 清空规划器缓冲区
 */
void planner_clear(void);

/**
 * @brief 获取缓冲区可用空间
 * @return 可用空间数量
 */
uint8_t planner_available(void);

/**
 * @brief 获取缓冲区中的块数量
 * @return 块数量
 */
uint8_t planner_get_count(void);

/**
 * @brief 检查缓冲区是否为空
 * @return true 为空, false 不为空
 */
bool planner_is_empty(void);

/**
 * @brief 检查缓冲区是否已满
 * @return true 已满, false 未满
 */
bool planner_is_full(void);

// ==================== 调度控制 ====================

/**
 * @brief 启动规划器执行
 * @return true 成功, false 失败（缓冲区为空）
 */
bool planner_start(void);

/**
 * @brief 停止规划器
 */
void planner_stop(void);

/**
 * @brief 暂停规划器
 */
void planner_pause(void);

/**
 * @brief 恢复规划器
 */
void planner_resume(void);

/**
 * @brief 检查是否正在运行
 * @return true 运行中, false 已停止
 */
bool planner_is_running(void);

/**
 * @brief 检查是否暂停
 * @return true 暂停中, false 未暂停
 */
bool planner_is_paused(void);

// ==================== 规划器核心 ====================

/**
 * @brief 更新规划器（定期调用，执行调度和规划）
 * @description 
 *   1. 检查时间戳，执行到时的运动
 *   2. 触发前瞻规划（如果需要）
 */
void planner_update(void);

/**
 * @brief 重新计算速度规划（Look-Ahead）
 * @description 
 *   执行反向传递和前向传递，计算所有块的进入/退出速度
 */
void planner_recalculate(void);

/**
 * @brief 强制重新规划所有块
 */
void planner_force_recalculate(void);

// ==================== 查询函数 ====================

/**
 * @brief 获取当前执行块（peek，不移除）
 * @return 块指针，NULL表示无块
 */
plan_block_t* planner_get_current_block(void);

/**
 * @brief 丢弃当前执行块
 * @return true 成功, false 失败（缓冲区为空）
 */
bool planner_discard_current_block(void);

/**
 * @brief 获取指定索引的块（用于规划算法）
 * @param index 索引（0=tail, 1=tail+1, ...）
 * @return 块指针，NULL表示无效索引
 */
plan_block_t* planner_get_block_by_index(uint8_t index);

/**
 * @brief 获取规划器状态字符串（调试用）
 * @param buf 缓冲区
 * @param size 缓冲区大小
 */
void planner_get_status(char *buf, size_t size);

// ==================== 辅助函数 ====================

/**
 * @brief 计算两个块之间的衔接速度
 * @param prev 前一个块
 * @param current 当前块
 * @return 衔接速度（度/秒）
 */
float planner_calculate_junction_speed(const plan_block_t *prev, 
                                       const plan_block_t *current);

/**
 * @brief 根据进入/退出速度重新计算梯形曲线
 * @param block 规划块指针
 */
void planner_recalculate_trapezoid(plan_block_t *block);

#endif // PLANNER_H


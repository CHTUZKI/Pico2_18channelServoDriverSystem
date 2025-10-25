/**
 * @file planner.c
 * @brief 运动规划器实现 - Look-Ahead Motion Planner
 * @date 2025-10-25
 * 
 * 算法参考：grblHAL planner
 * 核心思想：
 *   1. 反向传递（Reverse Pass）：从后往前计算最大进入速度
 *   2. 前向传递（Forward Pass）：从前往后确保速度连续
 *   3. 重新计算梯形曲线参数
 */

#include "motion/planner.h"
#include "motion/interpolation.h"
#include "utils/usb_bridge.h"
#include "servo/servo_control.h"
#include "pico/stdlib.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// ==================== 调试宏 ====================
#if DEBUG_PLANNER
    #define PLANNER_DEBUG(...) usb_bridge_printf(__VA_ARGS__)
#else
    #define PLANNER_DEBUG(...) ((void)0)
#endif

// ==================== 全局数据 ====================

// 全局运动规划器实例
static motion_planner_t g_planner;

// 运动执行回调函数
static planner_execute_callback_t g_execute_callback = NULL;

// ==================== 辅助宏 ====================

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

// 环形缓冲区索引操作
#define NEXT_INDEX(idx) (((idx) + 1) % PLANNER_BUFFER_SIZE)
#define PREV_INDEX(idx) (((idx) == 0) ? (PLANNER_BUFFER_SIZE - 1) : ((idx) - 1))

// ==================== 前向声明 ====================

static void planner_reverse_pass(void);
static void planner_forward_pass(void);
static plan_block_t* planner_get_block(uint8_t index);

// ==================== 初始化函数 ====================

void planner_init(motion_planner_t *planner) {
    if (planner == NULL) {
        planner = &g_planner;
    }
    
    memset(planner, 0, sizeof(motion_planner_t));
    planner->head = 0;
    planner->tail = 0;
    planner->count = 0;
    planner->running = false;
    planner->paused = false;
    planner->recalculate_flag = false;
    planner->last_servo_id = 0xFF;
    
    PLANNER_DEBUG("[PLANNER] Initialized\n");
}

void planner_set_callback(planner_execute_callback_t callback) {
    g_execute_callback = callback;
    PLANNER_DEBUG("[PLANNER] Callback registered\n");
}

motion_planner_t* planner_get_instance(void) {
    return &g_planner;
}

// ==================== 缓冲区管理 ====================

bool planner_add_motion(uint32_t timestamp_ms, 
                       uint8_t servo_id,
                       float target_angle,
                       float velocity,
                       float acceleration,
                       float deceleration) {
    // 检查缓冲区是否已满
    if (g_planner.count >= PLANNER_BUFFER_SIZE) {
        PLANNER_DEBUG("[PLANNER] Buffer full, cannot add motion\n");
        return false;
    }
    
    // 获取当前舵机的起始角度
    float start_angle;
    if (g_planner.count > 0 && g_planner.last_servo_id == servo_id) {
        // 如果是同一个舵机的连续运动，使用上一个目标角度作为起始
        start_angle = g_planner.last_target_angle;
    } else {
        // 否则使用当前角度
        start_angle = servo_get_angle(servo_id);
    }
    
    // 获取写入位置
    plan_block_t *block = &g_planner.blocks[g_planner.head];
    
    // ========== 填充原始运动参数 ==========
    block->timestamp_ms = timestamp_ms;
    block->servo_id = servo_id;
    block->target_angle = target_angle;
    block->max_velocity = velocity;
    block->acceleration = acceleration;
    block->deceleration = (deceleration > 0.0f) ? deceleration : acceleration;
    
    // ========== 计算几何参数 ==========
    block->start_angle = start_angle;
    block->distance = target_angle - start_angle;
    block->abs_distance = fabsf(block->distance);
    
    // ========== 初始化速度规划参数 ==========
    block->entry_speed = 0.0f;              // 初始假设从静止开始
    block->exit_speed = 0.0f;               // 初始假设减速到0
    block->max_entry_speed = velocity;      // 最大进入速度不超过最大速度
    block->max_junction_speed = 0.0f;       // 稍后计算
    block->nominal_speed = velocity;        // 标称速度
    
    // ========== 初始化标志位 ==========
    block->flags.recalculate = true;        // 需要重新规划
    block->flags.nominal_length = false;    // 稍后计算
    block->flags.junction_valid = false;
    block->valid = true;
    
    // ========== 初步计算梯形曲线（假设从0到0）==========
    motion_params_t params = {
        .max_velocity = velocity,
        .acceleration = acceleration,
        .deceleration = block->deceleration
    };
    
    // 使用插值模块计算梯形曲线参数
    float t_accel, t_const, t_decel, v_max_actual;
    
    // 计算完整的梯形曲线（从0速度到0速度）
    if (block->abs_distance > 0.0f) {
        float d_accel = (velocity * velocity) / (2.0f * acceleration);
        float d_decel = (velocity * velocity) / (2.0f * block->deceleration);
        
        if (d_accel + d_decel <= block->abs_distance) {
            // 标准梯形
            v_max_actual = velocity;
            t_accel = velocity / acceleration;
            t_decel = velocity / block->deceleration;
            t_const = (block->abs_distance - d_accel - d_decel) / velocity;
            block->flags.nominal_length = true;
        } else {
            // 三角形
            float inv_2a = 1.0f / (2.0f * acceleration);
            float inv_2d = 1.0f / (2.0f * block->deceleration);
            v_max_actual = sqrtf(block->abs_distance / (inv_2a + inv_2d));
            t_accel = v_max_actual / acceleration;
            t_decel = v_max_actual / block->deceleration;
            t_const = 0.0f;
            block->flags.nominal_length = false;
        }
    } else {
        // 距离为0
        t_accel = 0.0f;
        t_const = 0.0f;
        t_decel = 0.0f;
        v_max_actual = 0.0f;
    }
    
    block->t_accel = t_accel;
    block->t_const = t_const;
    block->t_decel = t_decel;
    block->v_max_actual = v_max_actual;
    block->duration_ms = (uint32_t)((t_accel + t_const + t_decel) * 1000.0f);
    
    // ========== 更新缓冲区状态 ==========
    g_planner.head = NEXT_INDEX(g_planner.head);
    g_planner.count++;
    g_planner.last_servo_id = servo_id;
    g_planner.last_target_angle = target_angle;
    
    // ========== 触发重新规划 ==========
    g_planner.recalculate_flag = true;
    
    PLANNER_DEBUG("[PLANNER] Added: t=%d S%d %d->%d deg, v=%d, count=%d\n",
                 (int)timestamp_ms, servo_id, (int)start_angle, 
                 (int)target_angle, (int)velocity, g_planner.count);
    
    return true;
}

bool planner_add_continuous_motion(uint32_t timestamp_ms,
                                   uint8_t servo_id,
                                   int8_t target_speed_pct,
                                   uint8_t accel_rate,
                                   uint8_t decel_rate,
                                   uint32_t duration_ms) {
    // 检查缓冲区是否已满
    if (g_planner.count >= PLANNER_BUFFER_SIZE) {
        PLANNER_DEBUG("[PLANNER] Buffer full, cannot add continuous motion\n");
        return false;
    }
    
    // 检查参数
    if (servo_id >= SERVO_COUNT) {
        return false;
    }
    
    // 限制速度范围
    if (target_speed_pct > 100) target_speed_pct = 100;
    if (target_speed_pct < -100) target_speed_pct = -100;
    
    // 获取当前舵机的速度状态
    int8_t start_speed = 0;
    if (g_planner.count > 0 && g_planner.last_servo_id == servo_id) {
        // 如果是同一个舵机的连续运动，获取上一个块的退出速度
        plan_block_t *last_block = &g_planner.blocks[PREV_INDEX(g_planner.head)];
        if (last_block->flags.is_continuous) {
            start_speed = last_block->exit_speed_pct;
        }
    }
    
    // 获取写入位置
    plan_block_t *block = &g_planner.blocks[g_planner.head];
    memset(block, 0, sizeof(plan_block_t));
    
    // ========== 填充基本参数 ==========
    block->timestamp_ms = timestamp_ms;
    block->servo_id = servo_id;
    block->flags.is_continuous = true;
    
    // ========== 填充360度舵机参数 ==========
    block->target_speed_pct = target_speed_pct;
    block->entry_speed_pct = start_speed;          // 初始假设从上一个速度开始
    block->exit_speed_pct = target_speed_pct;      // 初始假设达到目标速度
    block->accel_rate = (accel_rate > 0) ? accel_rate : 50;  // 默认50%/秒
    block->decel_rate = (decel_rate > 0) ? decel_rate : block->accel_rate;
    
    // ========== 计算加速时间 ==========
    // 速度变化量（百分比）
    int16_t speed_change = abs(target_speed_pct - start_speed);
    
    // 加速时间（秒） = 速度变化 / 加速度
    float accel_time = (float)speed_change / (float)block->accel_rate;
    
    // 如果指定了持续时间，使用指定时间
    if (duration_ms > 0) {
        block->duration_ms = duration_ms;
        block->t_accel = (accel_time < (float)duration_ms / 1000.0f) ? accel_time : (float)duration_ms / 1000.0f;
        block->t_const = ((float)duration_ms / 1000.0f) - block->t_accel;
        block->t_decel = 0.0f;
    } else {
        // 没有指定持续时间，只计算加速时间
        block->duration_ms = (uint32_t)(accel_time * 1000.0f);
        block->t_accel = accel_time;
        block->t_const = 0.0f;
        block->t_decel = 0.0f;
    }
    
    // ========== 初始化标志位 ==========
    block->flags.recalculate = true;
    block->flags.junction_valid = false;
    block->flags.nominal_length = (block->t_const > 0);
    block->valid = true;
    
    // 这些字段对360度舵机无意义，但为了兼容性填充
    block->start_angle = 0.0f;
    block->target_angle = 0.0f;
    block->distance = 0.0f;
    block->abs_distance = 0.0f;
    block->max_velocity = 0.0f;
    block->acceleration = 0.0f;
    block->deceleration = 0.0f;
    
    // ========== 更新缓冲区状态 ==========
    g_planner.head = NEXT_INDEX(g_planner.head);
    g_planner.count++;
    g_planner.last_servo_id = servo_id;
    
    // ========== 触发重新规划 ==========
    g_planner.recalculate_flag = true;
    
    PLANNER_DEBUG("[PLANNER] Added continuous: t=%d S%d speed=%d%%, accel=%d, count=%d\n",
                 (int)timestamp_ms, servo_id, target_speed_pct, 
                 block->accel_rate, g_planner.count);
    
    return true;
}

void planner_clear(void) {
    g_planner.head = 0;
    g_planner.tail = 0;
    g_planner.count = 0;
    g_planner.running = false;
    g_planner.paused = false;
    g_planner.recalculate_flag = false;
    g_planner.last_servo_id = 0xFF;
    
    for (int i = 0; i < PLANNER_BUFFER_SIZE; i++) {
        g_planner.blocks[i].valid = false;
    }
    
    PLANNER_DEBUG("[PLANNER] Cleared\n");
}

uint8_t planner_available(void) {
    return PLANNER_BUFFER_SIZE - g_planner.count;
}

uint8_t planner_get_count(void) {
    return g_planner.count;
}

bool planner_is_empty(void) {
    return g_planner.count == 0;
}

bool planner_is_full(void) {
    return g_planner.count >= PLANNER_BUFFER_SIZE;
}

// ==================== 调度控制 ====================

bool planner_start(void) {
    if (g_planner.count == 0) {
        return false;
    }
    
    g_planner.running = true;
    g_planner.paused = false;
    g_planner.start_time = to_ms_since_boot(get_absolute_time());
    
    // 执行一次规划
    planner_recalculate();
    
    PLANNER_DEBUG("[PLANNER] Started with %d blocks\n", g_planner.count);
    
    return true;
}

void planner_stop(void) {
    g_planner.running = false;
    g_planner.paused = false;
}

void planner_pause(void) {
    g_planner.paused = true;
}

void planner_resume(void) {
    g_planner.paused = false;
}

bool planner_is_running(void) {
    return g_planner.running;
}

bool planner_is_paused(void) {
    return g_planner.paused;
}

// ==================== 规划器核心 ====================

void planner_update(void) {
    // 检查是否需要重新规划
    if (g_planner.recalculate_flag && g_planner.count > 0) {
        planner_recalculate();
        g_planner.recalculate_flag = false;
    }
    
    // 检查是否正在运行
    if (!g_planner.running || g_planner.paused) {
        return;
    }
    
    // 检查缓冲区是否为空
    if (g_planner.count == 0) {
        planner_stop();
        PLANNER_DEBUG("[PLANNER] All blocks executed\n");
        return;
    }
    
    // 获取当前时间
    uint32_t current_time = to_ms_since_boot(get_absolute_time());
    uint32_t elapsed_ms = current_time - g_planner.start_time;
    
    // 获取下一个待执行的块
    plan_block_t *block = &g_planner.blocks[g_planner.tail];
    
    // 检查是否到达执行时间
    if (elapsed_ms >= block->timestamp_ms) {
        PLANNER_DEBUG("[PLANNER] >>> Execute: t=%d S%d->%d v=%.1f entry=%.1f exit=%.1f\n",
                     (int)block->timestamp_ms, block->servo_id,
                     (int)block->target_angle, block->v_max_actual,
                     block->entry_speed, block->exit_speed);
        
        // 通过回调执行运动
        if (g_execute_callback != NULL) {
            g_execute_callback(block);
        }
        
        // 移除已执行的块
        planner_discard_current_block();
    }
}

void planner_recalculate(void) {
    if (g_planner.count == 0) {
        return;
    }
    
    PLANNER_DEBUG("[PLANNER] === Recalculating %d blocks ===\n", g_planner.count);
    
    // 反向传递：计算最大进入速度
    planner_reverse_pass();
    
    // 前向传递：确保速度连续
    planner_forward_pass();
    
    PLANNER_DEBUG("[PLANNER] === Recalculation complete ===\n");
}

void planner_force_recalculate(void) {
    g_planner.recalculate_flag = true;
}

// ==================== 查询函数 ====================

plan_block_t* planner_get_current_block(void) {
    if (g_planner.count == 0) {
        return NULL;
    }
    return &g_planner.blocks[g_planner.tail];
}

bool planner_discard_current_block(void) {
    if (g_planner.count == 0) {
        return false;
    }
    
    g_planner.blocks[g_planner.tail].valid = false;
    g_planner.tail = NEXT_INDEX(g_planner.tail);
    g_planner.count--;
    
    return true;
}

plan_block_t* planner_get_block_by_index(uint8_t index) {
    if (index >= g_planner.count) {
        return NULL;
    }
    
    uint8_t block_index = (g_planner.tail + index) % PLANNER_BUFFER_SIZE;
    return &g_planner.blocks[block_index];
}

void planner_get_status(char *buf, size_t size) {
    if (buf == NULL || size == 0) {
        return;
    }
    
    snprintf(buf, size, 
             "Planner: count=%d/%d, running=%d, paused=%d, recalc=%d",
             g_planner.count, PLANNER_BUFFER_SIZE,
             g_planner.running, g_planner.paused,
             g_planner.recalculate_flag);
}

// ==================== 核心规划算法 ====================

/**
 * @brief 反向传递 - 从后往前计算最大进入速度
 * 
 * 算法：
 *   从最后一个块开始，往前遍历
 *   每个块的最大进入速度 = min(
 *       自身最大速度,
 *       sqrt(退出速度² + 2 * 加速度 * 距离),
 *       衔接速度限制
 *   )
 */
static void planner_reverse_pass(void) {
    if (g_planner.count <= 1) {
        // 只有一个块或无块，无需规划
        if (g_planner.count == 1) {
            plan_block_t *block = planner_get_block(0);
            block->entry_speed = 0.0f;
            block->exit_speed = 0.0f;
        }
        return;
    }
    
    // 从倒数第二个块开始（最后一个块的退出速度必须为0）
    uint8_t block_index = PREV_INDEX(g_planner.head);
    plan_block_t *current = &g_planner.blocks[block_index];
    
    // 最后一个块必须减速到0
    current->exit_speed = 0.0f;
    
    // 从后往前遍历
    while (block_index != g_planner.tail) {
        plan_block_t *next = current;
        block_index = PREV_INDEX(block_index);
        current = &g_planner.blocks[block_index];
        
        if (!current->flags.recalculate) {
            continue;
        }
        
        // 计算衔接速度（如果是同一个舵机）
        if (current->servo_id == next->servo_id && 
            !current->flags.junction_valid) {
            current->max_junction_speed = planner_calculate_junction_speed(current, next);
            current->flags.junction_valid = true;
        } else if (current->servo_id != next->servo_id) {
            current->max_junction_speed = 0.0f;  // 不同舵机，必须停止
        }
        
        // 计算基于物理加速度的最大进入速度
        // v_entry_max² = v_exit² + 2 * a * d
        float v_exit = next->entry_speed;
        float v_max_from_accel = sqrtf(v_exit * v_exit + 
                                       2.0f * current->acceleration * current->abs_distance);
        
        // 最大进入速度 = min(标称速度, 物理限制, 衔接限制)
        current->max_entry_speed = MIN(current->nominal_speed, v_max_from_accel);
        current->max_entry_speed = MIN(current->max_entry_speed, current->max_junction_speed);
        
        // 退出速度 = 下一个块的进入速度（受衔接速度限制）
        current->exit_speed = MIN(next->entry_speed, current->max_junction_speed);
        
        PLANNER_DEBUG("[REVERSE] Block S%d: max_entry=%.1f exit=%.1f junction=%.1f\n",
                     current->servo_id, current->max_entry_speed, 
                     current->exit_speed, current->max_junction_speed);
    }
    
    // 第一个块的进入速度为0（从静止开始）
    current->entry_speed = 0.0f;
}

/**
 * @brief 前向传递 - 从前往后确保速度连续
 * 
 * 算法：
 *   从第一个块开始，往后遍历
 *   每个块的退出速度 = min(
 *       sqrt(进入速度² + 2 * 加速度 * 距离),
 *       下一个块的进入速度,
 *       衔接速度限制
 *   )
 *   重新计算梯形曲线参数
 */
static void planner_forward_pass(void) {
    if (g_planner.count == 0) {
        return;
    }
    
    plan_block_t *current = &g_planner.blocks[g_planner.tail];
    uint8_t block_index = g_planner.tail;
    
    // 第一个块从0速度开始
    current->entry_speed = 0.0f;
    
    // 从前往后遍历
    while (block_index != g_planner.head) {
        if (current->flags.recalculate) {
            // 计算基于物理加速度的最大退出速度
            // v_exit_max² = v_entry² + 2 * a * d
            float v_max_exit = sqrtf(current->entry_speed * current->entry_speed + 
                                     2.0f * current->acceleration * current->abs_distance);
            
            // 实际退出速度受多重限制
            current->exit_speed = MIN(v_max_exit, current->exit_speed);
            current->exit_speed = MIN(current->exit_speed, current->nominal_speed);
            
            // 根据进入/退出速度重新计算梯形曲线
            planner_recalculate_trapezoid(current);
            
            PLANNER_DEBUG("[FORWARD] Block S%d: entry=%.1f exit=%.1f v_max=%.1f\n",
                         current->servo_id, current->entry_speed, 
                         current->exit_speed, current->v_max_actual);
            
            current->flags.recalculate = false;
        }
        
        // 移动到下一个块
        plan_block_t *next_block = NULL;
        uint8_t next_index = NEXT_INDEX(block_index);
        if (next_index != g_planner.head) {
            next_block = &g_planner.blocks[next_index];
            
            // 下一个块的进入速度 = 当前块的退出速度
            next_block->entry_speed = current->exit_speed;
        }
        
        current = next_block;
        block_index = next_index;
    }
}

/**
 * @brief 计算两个块之间的衔接速度（Junction Speed）
 * 
 * 支持两种模式：
 *   1. 位置模式（180度舵机）：计算角度速度衔接
 *   2. 速度模式（360度舵机）：计算速度百分比衔接
 */
float planner_calculate_junction_speed(const plan_block_t *prev, 
                                       const plan_block_t *current) {
    if (prev == NULL || current == NULL) {
        return 0.0f;
    }
    
    // 如果不是同一个舵机，必须停止
    if (prev->servo_id != current->servo_id) {
        return 0.0f;
    }
    
    // ========== 360度连续旋转模式 ==========
    if (prev->flags.is_continuous && current->flags.is_continuous) {
        // 两个速度块之间的衔接
        // 计算速度变化量
        int16_t speed_diff = abs(current->target_speed_pct - prev->target_speed_pct);
        
        // 如果速度变化很小（< 5%），可以直接衔接
        if (speed_diff < 5) {
            return (float)MIN(abs(prev->target_speed_pct), abs(current->target_speed_pct));
        }
        
        // 速度变化较大，计算安全衔接速度
        // 取两段速度的中间值作为衔接点
        int8_t junction_speed_pct = (prev->target_speed_pct + current->target_speed_pct) / 2;
        
        PLANNER_DEBUG("[JUNCTION-360] S%d: speed=%d%% (prev=%d%% curr=%d%%)\n",
                     prev->servo_id, junction_speed_pct, 
                     prev->target_speed_pct, current->target_speed_pct);
        
        return (float)abs(junction_speed_pct);
    }
    
    // ========== 位置控制模式（180度舵机）==========
    if (!prev->flags.is_continuous && !current->flags.is_continuous) {
        // 如果任一块距离为0，不能衔接
        if (prev->abs_distance < 0.01f || current->abs_distance < 0.01f) {
            return MIN_JUNCTION_SPEED;
        }
        
        // 取两段运动的较小加速度作为限制
        float a_min = MIN(prev->acceleration, current->acceleration);
        
        // 简化的衔接速度计算：
        // 基于两段运动的速度和加速度
        float v_junction = MIN(prev->nominal_speed, current->nominal_speed);
        
        // 应用 Junction Deviation：
        // v_junction = sqrt(2 * a * deviation * distance)
        float avg_distance = (prev->abs_distance + current->abs_distance) * 0.5f;
        float v_junction_dev = sqrtf(2.0f * a_min * JUNCTION_DEVIATION * avg_distance);
        
        v_junction = MIN(v_junction, v_junction_dev);
        v_junction = MAX(v_junction, MIN_JUNCTION_SPEED);
        
        PLANNER_DEBUG("[JUNCTION-POS] S%d: v=%.1f (prev_v=%.1f curr_v=%.1f a=%.1f)\n",
                     prev->servo_id, v_junction, prev->nominal_speed, 
                     current->nominal_speed, a_min);
        
        return v_junction;
    }
    
    // ========== 混合模式（位置+速度）==========
    // 不同模式之间必须停止
    return 0.0f;
}

/**
 * @brief 根据进入/退出速度重新计算梯形曲线
 * 
 * 输入：block->entry_speed, block->exit_speed, block->nominal_speed
 * 输出：block->t_accel, block->t_const, block->t_decel, block->v_max_actual
 */
void planner_recalculate_trapezoid(plan_block_t *block) {
    if (block == NULL || block->abs_distance < 0.01f) {
        return;
    }
    
    float v_entry = block->entry_speed;
    float v_exit = block->exit_speed;
    float v_max = block->nominal_speed;
    float distance = block->abs_distance;
    float accel = block->acceleration;
    float decel = block->deceleration;
    
    // 计算加速段和减速段的距离
    // d_accel = (v_max² - v_entry²) / (2*a)
    // d_decel = (v_max² - v_exit²) / (2*d)
    float d_accel = (v_max * v_max - v_entry * v_entry) / (2.0f * accel);
    float d_decel = (v_max * v_max - v_exit * v_exit) / (2.0f * decel);
    
    float t_accel, t_const, t_decel, v_max_actual;
    
    if (d_accel + d_decel <= distance) {
        // 标准梯形：能够达到最大速度
        v_max_actual = v_max;
        t_accel = (v_max - v_entry) / accel;
        t_decel = (v_max - v_exit) / decel;
        t_const = (distance - d_accel - d_decel) / v_max;
        block->flags.nominal_length = true;
    } else {
        // 三角形/梯形：无法达到最大速度
        // 需要求解交点速度
        // d_accel + d_decel = distance
        // (v² - v_entry²)/(2a) + (v² - v_exit²)/(2d) = distance
        // v² * (1/(2a) + 1/(2d)) = distance + v_entry²/(2a) + v_exit²/(2d)
        
        float c1 = 1.0f / (2.0f * accel);
        float c2 = 1.0f / (2.0f * decel);
        float v_sq = (distance + v_entry * v_entry * c1 + v_exit * v_exit * c2) / (c1 + c2);
        
        if (v_sq > 0.0f) {
            v_max_actual = sqrtf(v_sq);
            
            // 限制在标称速度内
            if (v_max_actual > v_max) {
                v_max_actual = v_max;
            }
            
            t_accel = (v_max_actual - v_entry) / accel;
            t_decel = (v_max_actual - v_exit) / decel;
            t_const = 0.0f;
        } else {
            // 异常情况：直接从进入速度减速到退出速度
            v_max_actual = v_entry;
            t_accel = 0.0f;
            t_const = 0.0f;
            t_decel = (v_entry - v_exit) / decel;
        }
        
        block->flags.nominal_length = false;
    }
    
    // 更新块参数
    block->t_accel = t_accel;
    block->t_const = t_const;
    block->t_decel = t_decel;
    block->v_max_actual = v_max_actual;
    block->duration_ms = (uint32_t)((t_accel + t_const + t_decel) * 1000.0f);
}

// ==================== 辅助函数 ====================

static plan_block_t* planner_get_block(uint8_t index) {
    return planner_get_block_by_index(index);
}


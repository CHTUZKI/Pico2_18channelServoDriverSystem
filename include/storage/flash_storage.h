/**
 * @file flash_storage.h
 * @brief Flash存储接口
 * @date 2025-10-21
 */

#ifndef FLASH_STORAGE_H
#define FLASH_STORAGE_H

#include <stdint.h>
#include <stdbool.h>
#include "servo/servo_control.h"
#include "config/config.h"

/**
 * @brief Flash存储参数结构体
 */
typedef struct {
    uint32_t magic;                         // 魔数标识：0x53565250 ("SVRP")
    uint8_t version;                        // 参数版本号
    uint8_t servo_count;                    // 舵机数量
    uint16_t checksum;                      // 校验和
    
    // 舵机校准参数
    servo_calibration_t calibrations[SERVO_COUNT];
    
    // 预留扩展空间
    uint8_t reserved[128];
} flash_params_t;

/**
 * @brief 初始化Flash存储
 * @return true 成功, false 失败
 */
bool flash_storage_init(void);

/**
 * @brief 保存参数到Flash
 * @param params 参数指针
 * @return true 成功, false 失败
 */
bool flash_save_params(const flash_params_t *params);

/**
 * @brief 从Flash加载参数
 * @param params 参数存储指针
 * @return true 成功, false 失败
 */
bool flash_load_params(flash_params_t *params);

/**
 * @brief 擦除Flash存储区
 * @return true 成功, false 失败
 */
bool flash_erase_storage(void);

/**
 * @brief 验证Flash参数有效性
 * @param params 参数指针
 * @return true 有效, false 无效
 */
bool flash_verify_params(const flash_params_t *params);

/**
 * @brief 计算参数校验和
 * @param params 参数指针
 * @return 校验和
 */
uint16_t flash_calculate_checksum(const flash_params_t *params);

#endif // FLASH_STORAGE_H


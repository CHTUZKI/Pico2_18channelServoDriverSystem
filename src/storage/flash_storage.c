/**
 * @file flash_storage.c
 * @brief Flash存储实现
 * @date 2025-10-21
 */

#include "storage/flash_storage.h"
#include "utils/error_handler.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"
#include <string.h>

// Flash魔数
#define FLASH_MAGIC     0x53565250  // "SVRP"

// Flash地址计算
// XIP_BASE是Flash映射到内存的起始地址（0x10000000）
#define FLASH_TARGET_OFFSET     FLASH_STORAGE_OFFSET
#define FLASH_TARGET_ADDRESS    (XIP_BASE + FLASH_TARGET_OFFSET)

bool flash_storage_init(void) {
    // Flash初始化由SDK自动完成
    return true;
}

uint16_t flash_calculate_checksum(const flash_params_t *params) {
    uint16_t checksum = 0;
    const uint8_t *data = (const uint8_t*)params;
    size_t len = sizeof(flash_params_t) - sizeof(uint16_t);  // 不包括checksum字段
    
    for (size_t i = 0; i < len; i++) {
        checksum += data[i];
    }
    
    return checksum;
}

bool flash_verify_params(const flash_params_t *params) {
    // 检查魔数
    if (params->magic != FLASH_MAGIC) {
        return false;
    }
    
    // 检查版本
    if (params->version != FLASH_PARAM_VERSION) {
        error_set(ERROR_FLASH_VERSION);
        return false;
    }
    
    // 检查舵机数量
    if (params->servo_count != SERVO_COUNT) {
        return false;
    }
    
    // 验证校验和
    uint16_t calculated = flash_calculate_checksum(params);
    if (calculated != params->checksum) {
        return false;
    }
    
    return true;
}

bool flash_save_params(const flash_params_t *params) {
    if (params == NULL) {
        return false;
    }
    
    // 准备要写入的数据
    flash_params_t write_params = *params;
    write_params.magic = FLASH_MAGIC;
    write_params.version = FLASH_PARAM_VERSION;
    write_params.servo_count = SERVO_COUNT;
    write_params.checksum = flash_calculate_checksum(&write_params);
    
    // ⚠️ 关键：Flash写入会禁用中断（硬件限制）
    // 影响：Core 0和Core 1的中断都会暂停约10ms
    // 后果：USB可能丢失少量数据，QP/C时间事件会延迟
    // 这是RP2350硬件限制，无法避免
    
    // 禁用中断（Flash操作期间）
    uint32_t ints = save_and_disable_interrupts();
    
    // 擦除扇区（约5-8ms）
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    
    // 写入数据（约1-2ms）
    flash_range_program(FLASH_TARGET_OFFSET, (const uint8_t*)&write_params, sizeof(flash_params_t));
    
    // 恢复中断
    restore_interrupts(ints);
    
    // 验证写入
    flash_params_t verify_params;
    memcpy(&verify_params, (void*)FLASH_TARGET_ADDRESS, sizeof(flash_params_t));
    
    if (!flash_verify_params(&verify_params)) {
        error_set(ERROR_FLASH_WRITE);
        return false;
    }
    
    return true;
}

bool flash_load_params(flash_params_t *params) {
    if (params == NULL) {
        return false;
    }
    
    // 从Flash读取数据
    memcpy(params, (void*)FLASH_TARGET_ADDRESS, sizeof(flash_params_t));
    
    // 验证数据
    if (!flash_verify_params(params)) {
        error_set(ERROR_FLASH_READ);
        return false;
    }
    
    return true;
}

bool flash_erase_storage(void) {
    // 禁用中断
    uint32_t ints = save_and_disable_interrupts();
    
    // 擦除扇区
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    
    // 恢复中断
    restore_interrupts(ints);
    
    return true;
}


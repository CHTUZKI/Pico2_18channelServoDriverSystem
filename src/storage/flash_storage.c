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
#include <stdio.h>

// 调试宏
#if DEBUG_FLASH
    #define FLASH_DEBUG(fmt, ...) printf("[FLASH] " fmt, ##__VA_ARGS__)
#else
    #define FLASH_DEBUG(fmt, ...)
#endif

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
    
    // checksum 字段在结构体中的偏移量
    // struct: magic(4) + version(1) + servo_count(1) + checksum(2)
    const size_t checksum_offset = 6;  // checksum字段的起始位置
    const size_t checksum_size = 2;    // checksum字段的大小
    
    // 计算 checksum 字段之前的部分
    for (size_t i = 0; i < checksum_offset; i++) {
        checksum += data[i];
    }
    
    // 跳过 checksum 字段，计算之后的部分
    for (size_t i = checksum_offset + checksum_size; i < sizeof(flash_params_t); i++) {
        checksum += data[i];
    }
    
    return checksum;
}

bool flash_verify_params(const flash_params_t *params) {
    // 检查魔数
    if (params->magic != FLASH_MAGIC) {
        FLASH_DEBUG("Verify FAIL: Magic mismatch (got 0x%08X, expect 0x%08X)\n", 
            params->magic, FLASH_MAGIC);
        return false;
    }
    
    // 检查版本
    if (params->version != FLASH_PARAM_VERSION) {
        FLASH_DEBUG("Verify FAIL: Version mismatch (got %d, expect %d)\n",
            params->version, FLASH_PARAM_VERSION);
        error_set(ERROR_FLASH_VERSION);
        return false;
    }
    
    // 检查舵机数量
    if (params->servo_count != SERVO_COUNT) {
        FLASH_DEBUG("Verify FAIL: Servo count mismatch (got %d, expect %d)\n",
            params->servo_count, SERVO_COUNT);
        return false;
    }
    
    // 验证校验和
    uint16_t calculated = flash_calculate_checksum(params);
    if (calculated != params->checksum) {
        FLASH_DEBUG("Verify FAIL: Checksum mismatch (calculated 0x%04X, stored 0x%04X)\n",
            calculated, params->checksum);
        return false;
    }
    
    return true;
}

bool flash_save_params(const flash_params_t *params) {
    if (params == NULL) {
        FLASH_DEBUG("Save params: NULL pointer\n");
        return false;
    }
    
    FLASH_DEBUG("-- Flash Write Debug --\n");
    FLASH_DEBUG("Flash offset: 0x%X (%d KB)\n", FLASH_TARGET_OFFSET, FLASH_TARGET_OFFSET / 1024);
    FLASH_DEBUG("Flash address: 0x%X\n", FLASH_TARGET_ADDRESS);
    FLASH_DEBUG("Sector size: %d bytes\n", FLASH_SECTOR_SIZE);
    FLASH_DEBUG("Params size: %d bytes\n", sizeof(flash_params_t));
    
    // 准备要写入的数据
    flash_params_t write_params = *params;
    write_params.magic = FLASH_MAGIC;
    write_params.version = FLASH_PARAM_VERSION;
    write_params.servo_count = SERVO_COUNT;
    write_params.checksum = flash_calculate_checksum(&write_params);
    
    FLASH_DEBUG("Write data: Magic=0x%08X, Ver=%d, Count=%d, Checksum=0x%04X\n",
        write_params.magic, write_params.version, write_params.servo_count, write_params.checksum);
    
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
    
    FLASH_DEBUG("Flash erase and program completed\n");
    
    // 验证写入
    flash_params_t verify_params;
    memcpy(&verify_params, (void*)FLASH_TARGET_ADDRESS, sizeof(flash_params_t));
    
    FLASH_DEBUG("Read back: Magic=0x%08X, Ver=%d, Count=%d, Checksum=0x%04X\n",
        verify_params.magic, verify_params.version, verify_params.servo_count, verify_params.checksum);
    
    if (!flash_verify_params(&verify_params)) {
        FLASH_DEBUG("Verification FAILED!\n");
        error_set(ERROR_FLASH_WRITE);
        return false;
    }
    
    FLASH_DEBUG("Verification OK\n");
    FLASH_DEBUG("------------------------\n");
    
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


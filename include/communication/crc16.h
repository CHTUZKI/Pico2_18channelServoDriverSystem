/**
 * @file crc16.h
 * @brief CRC-16校验算法（CCITT标准）
 * @date 2025-10-21
 */

#ifndef CRC16_H
#define CRC16_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief 计算CRC-16校验值（CCITT标准）
 * @param data 数据指针
 * @param length 数据长度
 * @return CRC-16校验值
 */
uint16_t crc16_ccitt(const uint8_t *data, size_t length);

/**
 * @brief 验证CRC-16校验
 * @param data 数据指针（包含CRC）
 * @param length 数据长度（包含CRC）
 * @return true 校验通过, false 校验失败
 */
bool crc16_verify(const uint8_t *data, size_t length);

#endif // CRC16_H


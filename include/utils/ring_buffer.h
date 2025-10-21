/**
 * @file ring_buffer.h
 * @brief 环形缓冲区实现
 * @date 2025-10-21
 */

#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief 环形缓冲区结构体
 */
typedef struct {
    uint8_t *buffer;        // 缓冲区指针
    size_t size;            // 缓冲区大小
    volatile size_t head;   // 写入位置
    volatile size_t tail;   // 读取位置
    volatile size_t count;  // 当前数据量
} ring_buffer_t;

/**
 * @brief 初始化环形缓冲区
 * @param rb 环形缓冲区指针
 * @param buffer 缓冲区内存
 * @param size 缓冲区大小
 */
void ring_buffer_init(ring_buffer_t *rb, uint8_t *buffer, size_t size);

/**
 * @brief 向环形缓冲区写入一个字节
 * @param rb 环形缓冲区指针
 * @param data 要写入的数据
 * @return true 成功, false 缓冲区满
 */
bool ring_buffer_put(ring_buffer_t *rb, uint8_t data);

/**
 * @brief 从环形缓冲区读取一个字节
 * @param rb 环形缓冲区指针
 * @param data 读取的数据存储位置
 * @return true 成功, false 缓冲区空
 */
bool ring_buffer_get(ring_buffer_t *rb, uint8_t *data);

/**
 * @brief 向环形缓冲区写入多个字节
 * @param rb 环形缓冲区指针
 * @param data 要写入的数据
 * @param len 数据长度
 * @return 实际写入的字节数
 */
size_t ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, size_t len);

/**
 * @brief 从环形缓冲区读取多个字节
 * @param rb 环形缓冲区指针
 * @param data 读取的数据存储位置
 * @param len 要读取的长度
 * @return 实际读取的字节数
 */
size_t ring_buffer_read(ring_buffer_t *rb, uint8_t *data, size_t len);

/**
 * @brief 获取缓冲区中的数据量
 * @param rb 环形缓冲区指针
 * @return 当前数据量
 */
static inline size_t ring_buffer_count(const ring_buffer_t *rb) {
    return rb->count;
}

/**
 * @brief 获取缓冲区剩余空间
 * @param rb 环形缓冲区指针
 * @return 剩余空间大小
 */
static inline size_t ring_buffer_free(const ring_buffer_t *rb) {
    return rb->size - rb->count;
}

/**
 * @brief 检查缓冲区是否为空
 * @param rb 环形缓冲区指针
 * @return true 空, false 非空
 */
static inline bool ring_buffer_is_empty(const ring_buffer_t *rb) {
    return rb->count == 0;
}

/**
 * @brief 检查缓冲区是否已满
 * @param rb 环形缓冲区指针
 * @return true 满, false 未满
 */
static inline bool ring_buffer_is_full(const ring_buffer_t *rb) {
    return rb->count >= rb->size;
}

/**
 * @brief 清空环形缓冲区
 * @param rb 环形缓冲区指针
 */
void ring_buffer_clear(ring_buffer_t *rb);

/**
 * @brief 查看缓冲区数据（不移除）
 * @param rb 环形缓冲区指针
 * @param offset 偏移量
 * @param data 数据存储位置
 * @return true 成功, false 超出范围
 */
bool ring_buffer_peek(const ring_buffer_t *rb, size_t offset, uint8_t *data);

#endif // RING_BUFFER_H


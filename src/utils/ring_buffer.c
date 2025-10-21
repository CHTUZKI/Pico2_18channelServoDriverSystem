/**
 * @file ring_buffer.c
 * @brief 环形缓冲区实现
 * @date 2025-10-21
 */

#include "utils/ring_buffer.h"
#include <string.h>

void ring_buffer_init(ring_buffer_t *rb, uint8_t *buffer, size_t size) {
    rb->buffer = buffer;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

bool ring_buffer_put(ring_buffer_t *rb, uint8_t data) {
    if (rb->count >= rb->size) {
        return false;  // 缓冲区满
    }
    
    rb->buffer[rb->head] = data;
    rb->head = (rb->head + 1) % rb->size;
    rb->count++;
    
    return true;
}

bool ring_buffer_get(ring_buffer_t *rb, uint8_t *data) {
    if (rb->count == 0) {
        return false;  // 缓冲区空
    }
    
    *data = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % rb->size;
    rb->count--;
    
    return true;
}

size_t ring_buffer_write(ring_buffer_t *rb, const uint8_t *data, size_t len) {
    size_t written = 0;
    
    for (size_t i = 0; i < len; i++) {
        if (!ring_buffer_put(rb, data[i])) {
            break;  // 缓冲区满
        }
        written++;
    }
    
    return written;
}

size_t ring_buffer_read(ring_buffer_t *rb, uint8_t *data, size_t len) {
    size_t read = 0;
    
    for (size_t i = 0; i < len; i++) {
        if (!ring_buffer_get(rb, &data[i])) {
            break;  // 缓冲区空
        }
        read++;
    }
    
    return read;
}

void ring_buffer_clear(ring_buffer_t *rb) {
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

bool ring_buffer_peek(const ring_buffer_t *rb, size_t offset, uint8_t *data) {
    if (offset >= rb->count) {
        return false;  // 超出范围
    }
    
    size_t pos = (rb->tail + offset) % rb->size;
    *data = rb->buffer[pos];
    
    return true;
}


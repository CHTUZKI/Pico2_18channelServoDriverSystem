#!/usr/bin/env python3
"""验证CRC计算"""

def crc16_ccitt(data):
    """CRC-16 CCITT"""
    crc = 0xFFFF
    for byte in data:
        crc ^= (byte << 8)
        for _ in range(8):
            if crc & 0x8000:
                crc = (crc << 1) ^ 0x1021
            else:
                crc = crc << 1
            crc &= 0xFFFF
    return crc

# 测试帧：FF FE 00 20 01 00 1F A6
# CRC范围：00 20 01 00（从ID到数据结束）
data = bytes([0x00, 0x20, 0x01, 0x00])
crc = crc16_ccitt(data)
print(f"数据: {' '.join(f'{b:02X}' for b in data)}")
print(f"计算CRC: {crc:04X}")
print(f"CRC高字节: {(crc >> 8) & 0xFF:02X}")
print(f"CRC低字节: {crc & 0xFF:02X}")

expected_crc = 0x1FA6
print(f"\n期望CRC: {expected_crc:04X}")
print(f"匹配: {'✓' if crc == expected_crc else '✗'}")

# 测试MOVE_ALL的CRC
print("\n" + "="*60)
data2 = bytes([0x00, 0x03, 0x26]) + bytes([0x23, 0x28] * 18) + bytes([0x03, 0xE8])
crc2 = crc16_ccitt(data2)
print(f"MOVE_ALL CRC: {crc2:04X}")
expected_crc2 = 0x3DA2
print(f"期望CRC: {expected_crc2:04X}")
print(f"匹配: {'✓' if crc2 == expected_crc2 else '✗'}")


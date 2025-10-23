def crc16_ccitt(data):
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

# ENABLE舵机0: FF FE 00 20 01 00 1F A6
data1 = bytes([0x00, 0x20, 0x01, 0x00])
crc1 = crc16_ccitt(data1)
print(f"ENABLE[0] CRC: {crc1:04X} (上位机发送: 1FA6, 单片机回复: 3137)")
print(f"  正确: {crc1:04X} == 3137? {crc1 == 0x3137}")

# ENABLE所有: FF FE 00 20 01 FF 01 56  
data2 = bytes([0x00, 0x20, 0x01, 0xFF])
crc2 = crc16_ccitt(data2)
print(f"\nENABLE[ALL] CRC: {crc2:04X} (上位机发送: 0156)")
print(f"  正确: {crc2:04X} == 0156? {crc2 == 0x0156}")


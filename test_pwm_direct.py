#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
直接PWM测试脚本 - 测试GPIO0的PWM输出
"""

import serial
import time
import struct

def crc16_ccitt(data):
    """计算CRC-16 CCITT"""
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

def build_frame(device_id, cmd, data=b''):
    """构建协议帧"""
    frame = bytearray()
    frame.append(0xFF)  # 帧头1
    frame.append(0xFE)  # 帧头2
    frame.append(device_id)
    frame.append(cmd)
    frame.append(len(data))
    frame.extend(data)
    
    # 计算CRC（从ID到数据结束）
    crc = crc16_ccitt(frame[2:])
    frame.append((crc >> 8) & 0xFF)
    frame.append(crc & 0xFF)
    
    return bytes(frame)

def test_servo_0(port_name):
    """测试舵机0的PWM输出"""
    
    print("=" * 60)
    print("GPIO0 PWM输出测试")
    print("=" * 60)
    
    # 打开串口
    ser = serial.Serial(port_name, 115200, timeout=2)
    time.sleep(2)
    
    print(f"\n✓ 串口连接成功: {port_name}")
    
    # 清空接收缓冲区
    if ser.in_waiting > 0:
        ser.read(ser.in_waiting)
    
    # 测试1: 使能舵机0
    print("\n[测试1] 使能舵机0...")
    frame = build_frame(0x00, 0x20, struct.pack('B', 0x00))
    print(f"发送: {' '.join(f'{b:02X}' for b in frame)}")
    ser.write(frame)
    time.sleep(0.5)
    
    # 读取所有响应
    if ser.in_waiting > 0:
        response = ser.read(ser.in_waiting)
        print(f"接收: {response.decode('ascii', errors='ignore')}")
    
    # 测试2: 移动到0度
    print("\n[测试2] 舵机0移动到0度...")
    servo_id = 0
    angle = 0.0
    duration = 1000
    angle_int = int(angle * 100)
    data = struct.pack('>BHH', servo_id, angle_int, duration)
    frame = build_frame(0x00, 0x01, data)
    print(f"发送: {' '.join(f'{b:02X}' for b in frame)}")
    ser.write(frame)
    time.sleep(1.5)
    
    if ser.in_waiting > 0:
        response = ser.read(ser.in_waiting)
        print(f"接收: {response.decode('ascii', errors='ignore')}")
    
    print("\n→ 请用逻辑分析仪观察GPIO0，应该看到500μs脉宽")
    input("按回车继续...")
    
    # 测试3: 移动到90度
    print("\n[测试3] 舵机0移动到90度...")
    angle = 90.0
    angle_int = int(angle * 100)
    data = struct.pack('>BHH', servo_id, angle_int, duration)
    frame = build_frame(0x00, 0x01, data)
    print(f"发送: {' '.join(f'{b:02X}' for b in frame)}")
    ser.write(frame)
    time.sleep(1.5)
    
    if ser.in_waiting > 0:
        response = ser.read(ser.in_waiting)
        print(f"接收: {response.decode('ascii', errors='ignore')}")
    
    print("\n→ 请用逻辑分析仪观察GPIO0，应该看到1500μs脉宽")
    input("按回车继续...")
    
    # 测试4: 移动到180度
    print("\n[测试4] 舵机0移动到180度...")
    angle = 180.0
    angle_int = int(angle * 100)
    data = struct.pack('>BHH', servo_id, angle_int, duration)
    frame = build_frame(0x00, 0x01, data)
    print(f"发送: {' '.join(f'{b:02X}' for b in frame)}")
    ser.write(frame)
    time.sleep(1.5)
    
    if ser.in_waiting > 0:
        response = ser.read(ser.in_waiting)
        print(f"接收: {response.decode('ascii', errors='ignore')}")
    
    print("\n→ 请用逻辑分析仪观察GPIO0，应该看到2500μs脉宽")
    
    # 关闭串口
    ser.close()
    print("\n" + "=" * 60)
    print("测试完成！")
    print("=" * 60)

if __name__ == "__main__":
    import sys
    
    if len(sys.argv) > 1:
        port = sys.argv[1]
    else:
        port = input("请输入串口号 (如 COM7): ").strip()
    
    test_servo_0(port)


#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
舵机循环测试脚本
所有18个舵机从0度到180度往复运动20次
"""

import serial
import time
import struct
import sys

class ServoCycleTester:
    def __init__(self, port='COM7', baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.serial_conn = None
        
        # 协议常量
        self.HEADER1 = 0xFF
        self.HEADER2 = 0xFE
        
        # 命令定义
        self.CMD_ENABLE_SERVO = 0x20
        self.CMD_MOVE_ALL = 0x03
        
        # 响应码
        self.RESP_OK = 0x00
    
    def crc16_ccitt(self, data):
        """计算CRC-16 CCITT校验"""
        crc = 0xFFFF
        for byte in data:
            crc ^= byte << 8
            for _ in range(8):
                if crc & 0x8000:
                    crc = (crc << 1) ^ 0x1021
                else:
                    crc = crc << 1
                crc &= 0xFFFF
        return crc
    
    def build_frame(self, cmd, data=b''):
        """构建协议帧"""
        frame = bytearray()
        frame.append(self.HEADER1)
        frame.append(self.HEADER2)
        frame.append(0x00)  # ID (广播)
        frame.append(cmd)
        frame.append(len(data))
        frame.extend(data)
        
        # 计算CRC（从ID到数据结束）
        crc = self.crc16_ccitt(frame[2:])
        frame.append((crc >> 8) & 0xFF)
        frame.append(crc & 0xFF)
        
        return bytes(frame)
    
    def send_command(self, device_id, cmd, data=b'', timeout=1.0):
        """发送命令并等待响应"""
        frame = self.build_frame(cmd, data)
        
        # 清空接收缓冲区
        self.serial_conn.reset_input_buffer()
        
        # 发送
        self.serial_conn.write(frame)
        print(f"发送: {' '.join(f'{b:02X}' for b in frame)}")
        
        # 等待响应
        start_time = time.time()
        rx_buffer = bytearray()
        
        while time.time() - start_time < timeout:
            if self.serial_conn.in_waiting > 0:
                rx_buffer.extend(self.serial_conn.read(self.serial_conn.in_waiting))
                
                # 查找响应帧
                response = self.parse_response(rx_buffer)
                if response:
                    return response
            
            time.sleep(0.01)
        
        return None
    
    def parse_response(self, data):
        """从混合数据中解析响应帧"""
        for i in range(len(data) - 1):
            if data[i] == self.HEADER1 and data[i+1] == self.HEADER2:
                if len(data) - i < 7:
                    continue
                
                device_id = data[i + 2]
                cmd = data[i + 3]
                data_len = data[i + 4]
                
                frame_len = 7 + data_len
                if len(data) - i < frame_len:
                    continue
                
                frame = data[i:i + frame_len]
                payload = frame[5:5+data_len]
                crc_received = (frame[5+data_len] << 8) | frame[6+data_len]
                crc_calculated = self.crc16_ccitt(frame[2:5+data_len])
                
                if crc_calculated == crc_received:
                    resp_code = payload[0] if len(payload) > 0 else 0xFF
                    resp_data = payload[1:] if len(payload) > 1 else b''
                    return (device_id, cmd, resp_code, resp_data)
        
        return None
    
    def connect(self):
        """连接串口"""
        print(f"正在连接串口 {self.port}...")
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=0.1
            )
            time.sleep(0.5)
            print(f"✓ 已连接到 {self.port}\n")
            return True
        except Exception as e:
            print(f"✗ 连接失败: {e}")
            return False
    
    def disconnect(self):
        """断开串口"""
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            print("\n已断开连接")
    
    def enable_all_servos(self):
        """使能所有舵机"""
        print("使能所有舵机...")
        
        # 使能命令：0xFF表示所有舵机
        data = bytes([0xFF])
        response = self.send_command(0x00, self.CMD_ENABLE_SERVO, data)
        
        if response and response[2] == self.RESP_OK:
            print("✓ 所有舵机已使能\n")
            return True
        else:
            print("✗ 使能失败\n")
            return False
    
    def move_all_servos(self, angle, duration_ms):
        """控制所有舵机同步运动"""
        # 构建数据：18个角度值(uint16) + 运动时长(uint16)
        data = bytearray()
        for i in range(18):
            angle_int = int(angle * 100)  # 角度*100
            data.extend(struct.pack('>H', angle_int))
        data.extend(struct.pack('>H', duration_ms))
        
        response = self.send_command(0x00, self.CMD_MOVE_ALL, bytes(data))
        
        if response and response[2] == self.RESP_OK:
            return True
        else:
            return False
    
    def run_cycle_test(self, cycles=20):
        """运行循环测试：0° → 180° → 0° × N次"""
        print("="*60)
        print(f"开始循环测试 - {cycles}次循环 (0° ⇄ 180°)")
        print("="*60)
        print()
        
        duration = 2000  # 每次运动2秒
        
        for cycle in range(1, cycles + 1):
            print(f"\n--- 第 {cycle}/{cycles} 次循环 ---")
            
            # 第1步：所有舵机移动到 0°
            print(f"→ 移动到 0° (用时{duration}ms)...", end=' ')
            if self.move_all_servos(0.0, duration):
                print("✓")
            else:
                print("✗ 失败!")
                return False
            
            time.sleep(duration / 1000.0 + 0.2)  # 等待运动完成
            
            # 第2步：所有舵机移动到 180°
            print(f"→ 移动到 180° (用时{duration}ms)...", end=' ')
            if self.move_all_servos(180.0, duration):
                print("✓")
            else:
                print("✗ 失败!")
                return False
            
            time.sleep(duration / 1000.0 + 0.2)  # 等待运动完成
        
        # 最后回到 90° 安全位置
        print(f"\n→ 回到安全位置 90° (用时{duration}ms)...", end=' ')
        if self.move_all_servos(90.0, duration):
            print("✓")
        else:
            print("✗ 失败!")
            return False
        
        print("\n" + "="*60)
        print(f"✓ 循环测试完成! 共完成 {cycles} 次循环")
        print("="*60)
        return True

def main():
    # 创建测试对象
    tester = ServoCycleTester(port='COM7', baudrate=115200)
    
    try:
        # 连接
        if not tester.connect():
            return 1
        
        # 使能舵机
        if not tester.enable_all_servos():
            return 1
        
        # 运行循环测试
        success = tester.run_cycle_test(cycles=20)
        
        return 0 if success else 1
        
    except KeyboardInterrupt:
        print("\n\n用户中断测试")
        return 1
    
    finally:
        tester.disconnect()

if __name__ == '__main__':
    sys.exit(main())


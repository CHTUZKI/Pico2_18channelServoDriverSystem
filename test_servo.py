#!/usr/bin/env python3
"""
18轴舵机控制系统 - 测试工具
用于测试USB通信和舵机控制功能
"""

import serial
import struct
import time
import argparse

class ServoController:
    """舵机控制器类"""
    
    # 命令定义
    CMD_MOVE_SINGLE = 0x01
    CMD_MOVE_ALL = 0x03
    CMD_GET_SINGLE = 0x10
    CMD_GET_ALL = 0x11
    CMD_ENABLE = 0x20
    CMD_DISABLE = 0x21
    CMD_SAVE_FLASH = 0x30
    CMD_LOAD_FLASH = 0x31
    CMD_PING = 0xFE
    CMD_ESTOP = 0xFF
    
    # 响应码
    RESP_OK = 0x00
    RESP_ERROR = 0x01
    
    def __init__(self, port, baudrate=115200):
        """初始化"""
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        
    def connect(self):
        """连接设备"""
        try:
            self.ser = serial.Serial(self.port, self.baudrate, timeout=1)
            time.sleep(0.5)  # 等待连接稳定
            print(f"已连接到 {self.port}, 波特率 {self.baudrate}")
            return True
        except Exception as e:
            print(f"连接失败: {e}")
            return False
    
    def disconnect(self):
        """断开连接"""
        if self.ser and self.ser.is_open:
            self.ser.close()
            print("已断开连接")
    
    def crc16_ccitt(self, data):
        """计算CRC-16 CCITT校验"""
        crc = 0xFFFF
        for byte in data:
            crc ^= (byte << 8)
            for _ in range(8):
                if crc & 0x8000:
                    crc = (crc << 1) ^ 0x1021
                else:
                    crc <<= 1
                crc &= 0xFFFF
        return crc
    
    def build_frame(self, cmd, data=b''):
        """构建协议帧"""
        frame = bytearray()
        frame.append(0xFF)  # 帧头1
        frame.append(0xFE)  # 帧头2
        frame.append(0x00)  # ID (广播)
        frame.append(cmd)   # 命令
        frame.append(len(data))  # 数据长度
        frame.extend(data)  # 数据
        
        # 计算CRC
        crc = self.crc16_ccitt(frame)
        frame.append((crc >> 8) & 0xFF)  # CRC高字节
        frame.append(crc & 0xFF)          # CRC低字节
        
        return bytes(frame)
    
    def send_command(self, cmd, data=b''):
        """发送命令"""
        if not self.ser or not self.ser.is_open:
            print("错误: 设备未连接")
            return None
        
        # 构建帧
        frame = self.build_frame(cmd, data)
        
        # 发送
        self.ser.write(frame)
        print(f"发送: {frame.hex(' ')}")
        
        # 接收响应
        time.sleep(0.1)
        if self.ser.in_waiting > 0:
            response = self.ser.read(self.ser.in_waiting)
            print(f"接收: {response.hex(' ')}")
            return response
        else:
            print("无响应")
            return None
    
    def ping(self):
        """心跳测试"""
        print("发送PING命令...")
        resp = self.send_command(self.CMD_PING)
        if resp:
            print("设备响应正常")
            return True
        return False
    
    def move_single(self, servo_id, angle, speed_ms=1000):
        """控制单个舵机"""
        print(f"控制舵机 {servo_id} 到 {angle}°, 速度 {speed_ms}ms")
        
        # 角度 × 100
        angle_raw = int(angle * 100)
        
        # 构建数据: [ID][角度高][角度低][速度高][速度低]
        data = bytearray()
        data.append(servo_id)
        data.append((angle_raw >> 8) & 0xFF)
        data.append(angle_raw & 0xFF)
        data.append((speed_ms >> 8) & 0xFF)
        data.append(speed_ms & 0xFF)
        
        return self.send_command(self.CMD_MOVE_SINGLE, bytes(data))
    
    def move_all(self, angle, speed_ms=1000):
        """控制所有舵机到相同角度"""
        print(f"控制所有舵机到 {angle}°, 速度 {speed_ms}ms")
        
        # 角度 × 100
        angle_raw = int(angle * 100)
        
        # 构建数据: 18个舵机的角度 + 速度
        data = bytearray()
        for i in range(18):
            data.append((angle_raw >> 8) & 0xFF)
            data.append(angle_raw & 0xFF)
        data.append((speed_ms >> 8) & 0xFF)
        data.append(speed_ms & 0xFF)
        
        return self.send_command(self.CMD_MOVE_ALL, bytes(data))
    
    def enable_servo(self, servo_id=0xFF):
        """使能舵机"""
        print(f"使能舵机 {servo_id if servo_id != 0xFF else '全部'}")
        data = bytes([servo_id])
        return self.send_command(self.CMD_ENABLE, data)
    
    def disable_servo(self, servo_id=0xFF):
        """禁用舵机"""
        print(f"禁用舵机 {servo_id if servo_id != 0xFF else '全部'}")
        data = bytes([servo_id])
        return self.send_command(self.CMD_DISABLE, data)
    
    def get_all_status(self):
        """查询所有舵机状态"""
        print("查询所有舵机状态...")
        return self.send_command(self.CMD_GET_ALL)
    
    def emergency_stop(self):
        """紧急停止"""
        print("紧急停止!")
        return self.send_command(self.CMD_ESTOP)

def main():
    """主函数"""
    parser = argparse.ArgumentParser(description='18轴舵机控制测试工具')
    parser.add_argument('--port', default='COM3', help='串口号 (Windows: COM3, Linux: /dev/ttyACM0)')
    parser.add_argument('--baudrate', type=int, default=115200, help='波特率')
    parser.add_argument('--ping', action='store_true', help='心跳测试')
    parser.add_argument('--single', nargs=3, metavar=('ID', 'ANGLE', 'SPEED'), 
                       help='单轴控制: ID 角度 速度(ms)')
    parser.add_argument('--all', nargs=2, metavar=('ANGLE', 'SPEED'), 
                       help='全轴控制: 角度 速度(ms)')
    parser.add_argument('--enable', action='store_true', help='使能所有舵机')
    parser.add_argument('--disable', action='store_true', help='禁用所有舵机')
    parser.add_argument('--query', action='store_true', help='查询状态')
    parser.add_argument('--estop', action='store_true', help='紧急停止')
    
    args = parser.parse_args()
    
    # 创建控制器
    controller = ServoController(args.port, args.baudrate)
    
    # 连接设备
    if not controller.connect():
        return
    
    try:
        # 执行命令
        if args.ping:
            controller.ping()
        
        elif args.single:
            servo_id = int(args.single[0])
            angle = float(args.single[1])
            speed = int(args.single[2])
            controller.enable_servo(servo_id)
            time.sleep(0.1)
            controller.move_single(servo_id, angle, speed)
        
        elif args.all:
            angle = float(args.all[0])
            speed = int(args.all[1])
            controller.enable_servo()
            time.sleep(0.1)
            controller.move_all(angle, speed)
        
        elif args.enable:
            controller.enable_servo()
        
        elif args.disable:
            controller.disable_servo()
        
        elif args.query:
            controller.get_all_status()
        
        elif args.estop:
            controller.emergency_stop()
        
        else:
            # 交互模式
            print("\n=== 交互模式 ===")
            print("1. 心跳测试")
            print("2. 使能所有舵机")
            print("3. 控制所有舵机到90°")
            print("4. 查询状态")
            print("5. 禁用所有舵机")
            print("0. 退出")
            
            while True:
                choice = input("\n选择操作: ")
                
                if choice == '1':
                    controller.ping()
                elif choice == '2':
                    controller.enable_servo()
                elif choice == '3':
                    controller.move_all(90.0, 1000)
                elif choice == '4':
                    controller.get_all_status()
                elif choice == '5':
                    controller.disable_servo()
                elif choice == '0':
                    break
                else:
                    print("无效选择")
    
    finally:
        controller.disconnect()

if __name__ == '__main__':
    main()


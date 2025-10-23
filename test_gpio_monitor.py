#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
GPIO监控测试脚本
测试Core1 GPIO监控器功能
"""

import serial
import time
import struct
import logging

# 配置日志
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s',
    handlers=[
        logging.FileHandler('gpio_monitor_test.log', mode='w', encoding='utf-8'),
        logging.StreamHandler()
    ]
)
logger = logging.getLogger('gpio_monitor_test')

class GPIOMonitorTester:
    def __init__(self, port='COM7', baudrate=115200):
        self.port = port
        self.baudrate = baudrate
        self.serial_conn = None
        
        # 协议常量
        self.HEADER1 = 0xFF
        self.HEADER2 = 0xFE
        
        # 命令定义
        self.CMD_ENABLE_SERVO = 0x20
        self.CMD_MOVE_SINGLE = 0x21
        self.CMD_MOVE_ALL = 0x03
        
        # 响应码
        self.RESP_OK = 0x00
        self.RESP_ERROR = 0xFF
    
    def connect(self):
        """连接串口"""
        try:
            self.serial_conn = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1.0,
                write_timeout=1.0
            )
            logger.info(f"串口连接成功: {self.port} @ {self.baudrate}")
            time.sleep(2)  # 等待单片机启动
            return True
        except Exception as e:
            logger.error(f"串口连接失败: {e}")
            return False
    
    def disconnect(self):
        """断开串口"""
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()
            logger.info("串口已断开")
    
    def crc16_ccitt(self, data: bytes) -> int:
        """计算CRC16-CCITT"""
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
    
    def build_frame(self, cmd: int, data: bytes = b'') -> bytes:
        """构建协议帧"""
        frame = bytearray()
        frame.append(self.HEADER1)
        frame.append(self.HEADER2)
        frame.append(0x00)  # ID
        frame.append(cmd)
        frame.append(len(data))
        frame.extend(data)
        
        # 计算CRC（从ID到数据结束）
        crc = self.crc16_ccitt(frame[2:])
        frame.append((crc >> 8) & 0xFF)
        frame.append(crc & 0xFF)
        
        return bytes(frame)
    
    def send_command(self, cmd: int, data: bytes = b'') -> bool:
        """发送命令"""
        if not self.serial_conn or not self.serial_conn.is_open:
            logger.error("串口未连接")
            return False
        
        frame = self.build_frame(cmd, data)
        
        try:
            self.serial_conn.write(frame)
            logger.info(f"发送命令: {frame.hex().upper()}")
            return True
        except Exception as e:
            logger.error(f"发送命令失败: {e}")
            return False
    
    def read_monitor_output(self, duration: int = 10):
        """读取监控输出"""
        logger.info(f"开始读取GPIO监控输出，持续{duration}秒...")
        
        start_time = time.time()
        while time.time() - start_time < duration:
            if self.serial_conn.in_waiting > 0:
                try:
                    data = self.serial_conn.read(self.serial_conn.in_waiting)
                    text = data.decode('utf-8', errors='ignore')
                    if text.strip():
                        print(text.strip(), end='')
                except Exception as e:
                    logger.error(f"读取数据失败: {e}")
            time.sleep(0.1)
        
        print("\n")
        logger.info("监控输出读取完成")
    
    def test_gpio_monitor(self):
        """测试GPIO监控功能"""
        logger.info("=" * 60)
        logger.info("GPIO监控器测试开始")
        logger.info("=" * 60)
        
        if not self.connect():
            return False
        
        try:
            # 1. 等待监控器启动
            logger.info("等待GPIO监控器启动...")
            time.sleep(3)
            
            # 2. 读取初始监控输出
            logger.info("读取初始监控状态...")
            self.read_monitor_output(5)
            
            # 3. 使能GPIO0舵机
            logger.info("使能GPIO0舵机...")
            self.send_command(self.CMD_ENABLE_SERVO, b'\x00')  # 使能舵机0
            time.sleep(1)
            
            # 4. 读取使能后的监控输出
            logger.info("读取使能后的监控状态...")
            self.read_monitor_output(5)
            
            # 5. 发送运动命令（GPIO0从90度到0度）
            logger.info("发送GPIO0运动命令（90° → 0°）...")
            angle_data = struct.pack('>H', 0)  # 0度
            duration_data = struct.pack('>H', 2000)  # 2秒
            move_data = angle_data + duration_data
            self.send_command(self.CMD_MOVE_SINGLE, b'\x00' + move_data)
            
            # 6. 读取运动期间的监控输出
            logger.info("读取运动期间的监控状态...")
            self.read_monitor_output(8)
            
            # 7. 发送运动命令（GPIO0从0度到180度）
            logger.info("发送GPIO0运动命令（0° → 180°）...")
            angle_data = struct.pack('>H', 18000)  # 180度
            duration_data = struct.pack('>H', 2000)  # 2秒
            move_data = angle_data + duration_data
            self.send_command(self.CMD_MOVE_SINGLE, b'\x00' + move_data)
            
            # 8. 读取运动期间的监控输出
            logger.info("读取运动期间的监控状态...")
            self.read_monitor_output(8)
            
            logger.info("GPIO监控器测试完成")
            
        except Exception as e:
            logger.error(f"测试过程中出错: {e}")
            return False
        
        finally:
            self.disconnect()
        
        return True

def main():
    """主函数"""
    print("GPIO监控器测试程序")
    print("=" * 40)
    
    # 创建测试器
    tester = GPIOMonitorTester()
    
    # 运行测试
    success = tester.test_gpio_monitor()
    
    if success:
        print("\n✅ 测试完成！请查看 gpio_monitor_test.log 文件获取详细日志。")
    else:
        print("\n❌ 测试失败！请检查连接和日志。")

if __name__ == "__main__":
    main()

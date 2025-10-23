#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
18通道舵机控制系统测试脚本
测试所有协议命令并记录报文
"""

import serial
import serial.tools.list_ports
import time
import struct
from typing import List, Tuple, Optional
from datetime import datetime


class ServoTester:
    """舵机系统测试器"""
    
    # 协议常量
    HEADER1 = 0xFF
    HEADER2 = 0xFE
    
    # 命令定义
    CMD_MOVE_SINGLE = 0x01  # 单轴位置控制
    CMD_MOVE_MULTI = 0x02   # 多轴选择性控制
    CMD_MOVE_ALL = 0x03     # 全轴批量控制
    CMD_GET_SINGLE = 0x10   # 查询单轴状态
    CMD_GET_ALL = 0x11      # 查询全轴状态
    CMD_ENABLE = 0x20       # 使能舵机
    CMD_DISABLE = 0x21      # 失能舵机
    CMD_SAVE_FLASH = 0x30   # 保存参数到Flash
    CMD_LOAD_FLASH = 0x31   # 从Flash加载参数
    CMD_PING = 0xFE         # 心跳/连接检测
    CMD_ESTOP = 0xFF        # 紧急停止
    
    # 响应码
    RESP_OK = 0x00
    RESP_ERROR = 0x01
    RESP_INVALID_CMD = 0x02
    RESP_INVALID_PARAM = 0x03
    RESP_CRC_ERROR = 0x04
    
    def __init__(self, port: str, baudrate: int = 115200):
        """初始化测试器"""
        self.port = port
        self.baudrate = baudrate
        self.serial = None
        self.log_file = "test_servo_log.txt"
        self.log_data = []
        
    def log(self, message: str):
        """记录日志"""
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]
        log_line = f"[{timestamp}] {message}"
        print(log_line)
        self.log_data.append(log_line)
        
    def save_log(self):
        """保存日志到文件"""
        with open(self.log_file, 'w', encoding='utf-8') as f:
            f.write("=" * 80 + "\n")
            f.write("18通道舵机控制系统测试报告\n")
            f.write(f"测试时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write("=" * 80 + "\n\n")
            f.write("\n".join(self.log_data))
            f.write("\n\n" + "=" * 80 + "\n")
            f.write("测试完成\n")
            f.write("=" * 80 + "\n")
        self.log(f"\n测试报告已保存到: {self.log_file}")
        
    @staticmethod
    def crc16_ccitt(data: bytes) -> int:
        """计算CRC-16 CCITT校验"""
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
        
    def build_frame(self, device_id: int, cmd: int, data: bytes = b'') -> bytes:
        """构建协议帧"""
        frame = bytearray()
        frame.append(self.HEADER1)
        frame.append(self.HEADER2)
        frame.append(device_id)
        frame.append(cmd)
        frame.append(len(data))
        frame.extend(data)
        
        # 计算CRC（从ID到数据结束）
        crc = self.crc16_ccitt(frame[2:])
        frame.append((crc >> 8) & 0xFF)
        frame.append(crc & 0xFF)
        
        return bytes(frame)
        
    def parse_response(self, data: bytes) -> Optional[Tuple[int, int, int, bytes]]:
        """解析响应帧（从混合数据中提取协议帧）"""
        # 在数据中查找帧头 FF FE
        for i in range(len(data) - 1):
            if data[i] == self.HEADER1 and data[i+1] == self.HEADER2:
                # 找到可能的帧头，尝试解析
                frame_start = i
                
                # 检查是否有足够的数据
                if len(data) - frame_start < 7:
                    continue
                    
                device_id = data[frame_start + 2]
                cmd = data[frame_start + 3]
                data_len = data[frame_start + 4]
                
                # 检查完整帧长度
                frame_len = 7 + data_len
                if len(data) - frame_start < frame_len:
                    continue
                    
                # 提取帧数据
                frame = data[frame_start:frame_start + frame_len]
                payload = frame[5:5+data_len]
                crc_received = (frame[5+data_len] << 8) | frame[6+data_len]
                
                # 验证CRC（从ID到数据结束）
                crc_calculated = self.crc16_ccitt(frame[2:5+data_len])
                
                if crc_calculated == crc_received:
                    # CRC验证通过，解析响应码
                    resp_code = payload[0] if len(payload) > 0 else 0xFF
                    resp_data = payload[1:] if len(payload) > 1 else b''
                    
                    return (device_id, cmd, resp_code, resp_data)
                else:
                    # CRC错误，继续搜索下一个帧头
                    continue
        
        # 没有找到有效帧
        return None
        
    def format_bytes(self, data: bytes) -> str:
        """格式化字节数据为十六进制字符串"""
        return ' '.join(f'{b:02X}' for b in data)
        
    def send_command(self, device_id: int, cmd: int, data: bytes = b'', 
                    wait_response: bool = True, timeout: float = 1.0) -> Optional[Tuple]:
        """发送命令并等待响应"""
        frame = self.build_frame(device_id, cmd, data)
        
        self.log(f"\n→ 发送: {self.format_bytes(frame)}")
        self.log(f"  命令: 0x{cmd:02X}, 设备ID: 0x{device_id:02X}, 数据长度: {len(data)}")
        
        self.serial.write(frame)
        
        if not wait_response:
            return None
            
        # 等待响应
        time.sleep(0.1)
        start_time = time.time()
        buffer = bytearray()
        
        while time.time() - start_time < timeout:
            if self.serial.in_waiting > 0:
                new_data = self.serial.read(self.serial.in_waiting)
                buffer.extend(new_data)
                
                # 打印原始接收数据（包括调试信息）
                self.log(f"← 原始接收: {self.format_bytes(new_data)}")
                
                # 尝试解析响应
                response = self.parse_response(buffer)
                if response:
                    self.log(f"← 解析成功: {self.format_bytes(buffer)}")
                    dev_id, cmd_resp, resp_code, resp_data = response
                    
                    resp_code_name = {
                        self.RESP_OK: "成功",
                        self.RESP_ERROR: "错误",
                        self.RESP_INVALID_CMD: "无效命令",
                        self.RESP_INVALID_PARAM: "无效参数",
                        self.RESP_CRC_ERROR: "CRC错误"
                    }.get(resp_code, f"未知({resp_code})")
                    
                    self.log(f"  响应码: {resp_code_name}")
                    if len(resp_data) > 0:
                        self.log(f"  响应数据: {self.format_bytes(resp_data)}")
                    
                    return response
                    
            time.sleep(0.01)
            
        # 超时后，如果有数据则打印
        if len(buffer) > 0:
            self.log(f"  ⚠️ 响应超时! 接收到的数据: {self.format_bytes(buffer)}")
        else:
            self.log("  ⚠️ 响应超时! 未接收到任何数据")
        return None
        
    def connect(self) -> bool:
        """连接串口"""
        try:
            self.serial = serial.Serial(self.port, self.baudrate, timeout=1)
            time.sleep(2)  # 等待设备就绪
            self.log(f"✓ 串口连接成功: {self.port} @ {self.baudrate}")
            
            # 清空接收缓冲区并显示
            if self.serial.in_waiting > 0:
                old_data = self.serial.read(self.serial.in_waiting)
                self.log(f"  清空接收缓冲区: {len(old_data)} 字节")
                # 打印前100字节的调试信息
                if len(old_data) > 0:
                    preview = old_data[:100]
                    self.log(f"  调试信息: {preview.decode('ascii', errors='ignore')}")
            
            return True
        except Exception as e:
            self.log(f"✗ 串口连接失败: {e}")
            return False
            
    def disconnect(self):
        """断开串口"""
        if self.serial and self.serial.is_open:
            self.serial.close()
            self.log("✓ 串口已断开")
            
    def test_ping(self):
        """测试1: PING命令"""
        self.log("\n" + "="*60)
        self.log("测试1: PING - 心跳/连接检测")
        self.log("="*60)
        
        response = self.send_command(0x00, self.CMD_PING)
        if response and response[2] == self.RESP_OK:
            self.log("✓ PING测试通过")
            if len(response[3]) >= 4:
                pong = response[3][:4]
                self.log(f"  PONG响应: {pong.decode('ascii', errors='ignore')}")
        else:
            self.log("✗ PING测试失败")
            
    def test_enable_single(self):
        """测试2: 使能单个舵机"""
        self.log("\n" + "="*60)
        self.log("测试2: ENABLE_SINGLE - 使能单个舵机")
        self.log("="*60)
        
        servo_id = 0
        data = struct.pack('B', servo_id)
        
        self.log(f"使能舵机 #{servo_id}...")
        response = self.send_command(0x00, self.CMD_ENABLE, data)
        
        if response and response[2] == self.RESP_OK:
            self.log(f"✓ 舵机 #{servo_id} 使能成功")
        else:
            self.log(f"✗ 舵机 #{servo_id} 使能失败")
            
    def test_enable_all(self):
        """测试3: 使能所有舵机"""
        self.log("\n" + "="*60)
        self.log("测试3: ENABLE_ALL - 使能所有舵机")
        self.log("="*60)
        
        data = struct.pack('B', 0xFF)  # 0xFF表示全部
        
        self.log("使能所有舵机...")
        response = self.send_command(0x00, self.CMD_ENABLE, data)
        
        if response and response[2] == self.RESP_OK:
            self.log("✓ 所有舵机使能成功")
        else:
            self.log("✗ 所有舵机使能失败")
            
    def test_move_single(self):
        """测试4: 单轴移动"""
        self.log("\n" + "="*60)
        self.log("测试4: MOVE_SINGLE - 单轴位置控制")
        self.log("="*60)
        
        servo_id = 0
        angle = 90.0  # 90度
        duration = 1000  # 1000ms
        
        # 角度转换为整数（×100）
        angle_int = int(angle * 100)
        data = struct.pack('>BHH', servo_id, angle_int, duration)
        
        self.log(f"舵机 #{servo_id} 移动到 {angle}° (用时{duration}ms)...")
        response = self.send_command(0x00, self.CMD_MOVE_SINGLE, data)
        
        if response and response[2] == self.RESP_OK:
            self.log(f"✓ 单轴移动命令发送成功")
            time.sleep(duration / 1000.0 + 0.5)  # 等待运动完成
        else:
            self.log(f"✗ 单轴移动命令失败")
            
    def test_move_all(self):
        """测试5: 全轴移动"""
        self.log("\n" + "="*60)
        self.log("测试5: MOVE_ALL - 全轴批量控制")
        self.log("="*60)
        
        # 18个舵机都移动到90度
        angles = [90.0] * 18
        duration = 1000  # 1000ms
        
        # 构建数据：18个角度（每个2字节）+ 时间（2字节）
        data = bytearray()
        for angle in angles:
            angle_int = int(angle * 100)
            data.extend(struct.pack('>H', angle_int))
        data.extend(struct.pack('>H', duration))
        
        self.log(f"所有舵机移动到 90° (用时{duration}ms)...")
        response = self.send_command(0x00, self.CMD_MOVE_ALL, bytes(data))
        
        if response and response[2] == self.RESP_OK:
            self.log(f"✓ 全轴移动命令发送成功")
            time.sleep(duration / 1000.0 + 0.5)  # 等待运动完成
        else:
            self.log(f"✗ 全轴移动命令失败")
            
    def test_get_single(self):
        """测试6: 查询单轴状态"""
        self.log("\n" + "="*60)
        self.log("测试6: GET_SINGLE - 查询单轴状态")
        self.log("="*60)
        
        servo_id = 0
        data = struct.pack('B', servo_id)
        
        self.log(f"查询舵机 #{servo_id} 状态...")
        response = self.send_command(0x00, self.CMD_GET_SINGLE, data)
        
        if response and response[2] == self.RESP_OK and len(response[3]) >= 4:
            resp_data = response[3]
            servo_id_resp = resp_data[0]
            angle_int = (resp_data[1] << 8) | resp_data[2]
            enabled = resp_data[3]
            angle = angle_int / 100.0
            
            self.log(f"✓ 查询成功:")
            self.log(f"  舵机ID: {servo_id_resp}")
            self.log(f"  当前角度: {angle}°")
            self.log(f"  使能状态: {'已使能' if enabled else '未使能'}")
        else:
            self.log(f"✗ 查询失败")
            
    def test_get_all(self):
        """测试7: 查询全轴状态"""
        self.log("\n" + "="*60)
        self.log("测试7: GET_ALL - 查询全轴状态")
        self.log("="*60)
        
        self.log("查询所有舵机状态...")
        response = self.send_command(0x00, self.CMD_GET_ALL)
        
        if response and response[2] == self.RESP_OK:
            resp_data = response[3]
            servo_count = len(resp_data) // 3
            
            self.log(f"✓ 查询成功 ({servo_count}个舵机):")
            for i in range(min(servo_count, 18)):
                offset = i * 3
                servo_id = resp_data[offset]
                angle_int = (resp_data[offset+1] << 8) | resp_data[offset+2]
                angle = angle_int / 100.0
                self.log(f"  舵机#{servo_id}: {angle}°")
        else:
            self.log(f"✗ 查询失败")
            
    def test_disable_single(self):
        """测试8: 失能单个舵机"""
        self.log("\n" + "="*60)
        self.log("测试8: DISABLE_SINGLE - 失能单个舵机")
        self.log("="*60)
        
        servo_id = 0
        data = struct.pack('B', servo_id)
        
        self.log(f"失能舵机 #{servo_id}...")
        response = self.send_command(0x00, self.CMD_DISABLE, data)
        
        if response and response[2] == self.RESP_OK:
            self.log(f"✓ 舵机 #{servo_id} 失能成功")
        else:
            self.log(f"✗ 舵机 #{servo_id} 失能失败")
            
    def test_save_flash(self):
        """测试9: 保存参数到Flash"""
        self.log("\n" + "="*60)
        self.log("测试9: SAVE_FLASH - 保存参数到Flash")
        self.log("="*60)
        
        self.log("保存参数到Flash...")
        response = self.send_command(0x00, self.CMD_SAVE_FLASH)
        
        if response and response[2] == self.RESP_OK:
            self.log("✓ 参数保存成功")
        else:
            self.log("✗ 参数保存失败")
            
    def test_load_flash(self):
        """测试10: 从Flash加载参数"""
        self.log("\n" + "="*60)
        self.log("测试10: LOAD_FLASH - 从Flash加载参数")
        self.log("="*60)
        
        self.log("从Flash加载参数...")
        response = self.send_command(0x00, self.CMD_LOAD_FLASH)
        
        if response and response[2] == self.RESP_OK:
            self.log("✓ 参数加载成功")
        else:
            self.log("✗ 参数加载失败")
            
    def test_cycle_motion(self, cycles: int = 10):
        """循环测试：所有轴 0° → 180° → 0°"""
        self.log("\n" + "="*60)
        self.log(f"循环运动测试 - {cycles}次循环 (0° → 180° → 0°)")
        self.log("="*60)
        
        duration = 2000  # 每次运动2秒
        
        for cycle in range(cycles):
            self.log(f"\n--- 第 {cycle + 1}/{cycles} 次循环 ---")
            
            # 第1步：所有舵机移动到 0°
            self.log(f"所有舵机移动到 0° (用时{duration}ms)...")
            angles_0 = [0.0] * 18
            data = bytearray()
            for angle in angles_0:
                angle_int = int(angle * 100)
                data.extend(struct.pack('>H', angle_int))
            data.extend(struct.pack('>H', duration))
            
            response = self.send_command(0x00, self.CMD_MOVE_ALL, bytes(data))
            if response and response[2] == self.RESP_OK:
                self.log(f"✓ 运动命令发送成功")
            else:
                self.log(f"✗ 运动命令失败")
                return
            
            time.sleep(duration / 1000.0 + 0.2)  # 等待运动完成
            
            # 第2步：所有舵机移动到 180°
            self.log(f"所有舵机移动到 180° (用时{duration}ms)...")
            angles_180 = [180.0] * 18
            data = bytearray()
            for angle in angles_180:
                angle_int = int(angle * 100)
                data.extend(struct.pack('>H', angle_int))
            data.extend(struct.pack('>H', duration))
            
            response = self.send_command(0x00, self.CMD_MOVE_ALL, bytes(data))
            if response and response[2] == self.RESP_OK:
                self.log(f"✓ 运动命令发送成功")
            else:
                self.log(f"✗ 运动命令失败")
                return
            
            time.sleep(duration / 1000.0 + 0.2)  # 等待运动完成
        
        # 最后回到 0°
        self.log(f"\n最后回到 0° (用时{duration}ms)...")
        angles_0 = [0.0] * 18
        data = bytearray()
        for angle in angles_0:
            angle_int = int(angle * 100)
            data.extend(struct.pack('>H', angle_int))
        data.extend(struct.pack('>H', duration))
        
        response = self.send_command(0x00, self.CMD_MOVE_ALL, bytes(data))
        if response and response[2] == self.RESP_OK:
            self.log(f"✓ 循环测试完成!")
        else:
            self.log(f"✗ 最后一步失败")
    
    def run_all_tests(self):
        """运行所有测试"""
        self.log("\n" + "="*80)
        self.log("开始18通道舵机控制系统循环运动测试")
        self.log("="*80)
        
        if not self.connect():
            self.log("无法连接设备，测试终止")
            return
            
        try:
            # 1. 使能所有舵机
            self.log("\n" + "="*60)
            self.log("步骤1: 使能所有舵机")
            self.log("="*60)
            
            data = struct.pack('B', 0xFF)  # 0xFF表示全部
            response = self.send_command(0x00, self.CMD_ENABLE, data)
            
            if response and response[2] == self.RESP_OK:
                self.log("✓ 所有舵机使能成功")
            else:
                self.log("✗ 所有舵机使能失败，测试终止")
                return
            
            time.sleep(0.5)
            
            # 2. 循环运动测试
            self.test_cycle_motion(cycles=10)
            
            self.log("\n" + "="*80)
            self.log("所有测试完成!")
            self.log("="*80)
            
        except KeyboardInterrupt:
            self.log("\n测试被用户中断")
        except Exception as e:
            self.log(f"\n测试异常: {e}")
        finally:
            self.disconnect()
            self.save_log()


def find_pico_port() -> Optional[str]:
    """自动查找Pico设备串口"""
    ports = serial.tools.list_ports.comports()
    for port in ports:
        # Windows下查找Pico2
        if 'USB' in port.description or 'Serial' in port.description:
            return port.device
    return None


def main():
    """主函数"""
    print("18通道舵机控制系统测试工具")
    print("=" * 60)
    
    # 自动查找串口
    port = find_pico_port()
    
    if not port:
        # 手动输入串口
        print("\n可用串口:")
        ports = serial.tools.list_ports.comports()
        for i, p in enumerate(ports):
            print(f"  {i+1}. {p.device} - {p.description}")
        
        if not ports:
            print("未找到可用串口!")
            return
            
        choice = input(f"\n请选择串口 (1-{len(ports)}) 或直接输入串口号 (如 COM7): ")
        
        if choice.isdigit() and 1 <= int(choice) <= len(ports):
            port = ports[int(choice)-1].device
        else:
            port = choice.strip()
    
    print(f"\n使用串口: {port}")
    
    # 创建测试器并运行
    tester = ServoTester(port)
    tester.run_all_tests()
    
    print(f"\n测试报告已保存到: {tester.log_file}")


if __name__ == "__main__":
    main()


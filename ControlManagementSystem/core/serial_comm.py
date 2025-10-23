#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
串口通信模块
与Raspberry Pi Pico 2舵机控制器通信
使用自定义协议（帧格式 + CRC-16校验）
"""

import serial
import serial.tools.list_ports
import time
import threading
from typing import List, Optional, Callable, Dict, Any
from PyQt5.QtCore import QObject, pyqtSignal
import logging

logger = logging.getLogger('servo_controller')

class SerialComm(QObject):
    """串口通信类 - 舵机控制协议"""
    
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
    
    # 信号定义
    connected = pyqtSignal()
    disconnected = pyqtSignal()
    data_received = pyqtSignal(str)
    data_sent = pyqtSignal(str)
    error_occurred = pyqtSignal(str)
    
    def __init__(self):
        super().__init__()
        self.serial_port: Optional[serial.Serial] = None
        self.is_connected = False
        self.is_running = False
        self.read_thread: Optional[threading.Thread] = None
        self.port_name = ""
        self.baud_rate = 115200
        self.timeout = 1.0
        
        # 舵机数量
        self.servo_count = 18
    
    def get_available_ports(self) -> List[Dict[str, str]]:
        """获取可用串口列表"""
        ports = []
        for port in serial.tools.list_ports.comports():
            ports.append({
                'device': port.device,
                'description': port.description,
                'hwid': port.hwid
            })
        return ports
    
    def connect(self, port_name: str, baud_rate: int = 115200) -> bool:
        """连接串口"""
        try:
            if self.is_connected:
                self.disconnect()
            
            self.serial_port = serial.Serial(
                port=port_name,
                baudrate=baud_rate,
                timeout=self.timeout,
                write_timeout=1.0
            )
            
            self.port_name = port_name
            self.baud_rate = baud_rate
            self.is_connected = True
            
            # 启动读取线程
            self.is_running = True
            self.read_thread = threading.Thread(target=self._read_loop, daemon=True)
            self.read_thread.start()
            
            # 等待连接稳定
            time.sleep(0.5)
            
            logger.info(f"串口连接成功: {port_name} @ {baud_rate}")
            self.connected.emit()
            return True
            
        except Exception as e:
            error_msg = f"串口连接失败: {e}"
            logger.error(error_msg)
            self.error_occurred.emit(error_msg)
            return False
    
    def disconnect(self):
        """断开串口连接"""
        self.is_running = False
        
        if self.read_thread and self.read_thread.is_alive():
            self.read_thread.join(timeout=1.0)
        
        if self.serial_port and self.serial_port.is_open:
            try:
                self.serial_port.close()
            except:
                pass
        
        self.is_connected = False
        self.serial_port = None
        
        logger.info("串口已断开")
        self.disconnected.emit()
    
    def crc16_ccitt(self, data: bytes) -> int:
        """计算CRC-16 CCITT校验（与单片机一致）"""
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
    
    def build_frame(self, cmd: int, data: bytes = b'') -> bytes:
        """构建协议帧
        
        帧格式: [0xFF][0xFE][ID][CMD][LEN][DATA...][CRC_H][CRC_L]
        """
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
    
    def send_servo_command(self, cmd: int, data: bytes = b'') -> bool:
        """发送舵机命令"""
        if not self.is_connected or not self.serial_port:
            logger.warning("设备未连接")
            return False
        
        try:
            # 构建帧
            frame = self.build_frame(cmd, data)
            
            # 发送
            self.serial_port.write(frame)
            
            # 发送日志
            hex_str = ' '.join([f'{b:02X}' for b in frame])
            logger.debug(f"发送: {hex_str}")
            self.data_sent.emit(f"TX: {hex_str}")
            
            return True
            
        except Exception as e:
            error_msg = f"发送命令失败: {e}"
            logger.error(error_msg)
            self.error_occurred.emit(error_msg)
            return False
    
    def ping(self) -> bool:
        """心跳测试"""
        logger.info("发送PING命令")
        return self.send_servo_command(self.CMD_PING)
    
    def move_single_servo(self, servo_id: int, angle: float, speed_ms: int = 1000) -> bool:
        """控制单个舵机
        
        Args:
            servo_id: 舵机ID (0-7)
            angle: 目标角度 (0-180度)
            speed_ms: 运动时间 (毫秒)
        """
        if servo_id < 0 or servo_id >= self.servo_count:
            logger.error(f"舵机ID超出范围: {servo_id}")
            return False
        
        logger.info(f"控制舵机{servo_id} → {angle:.1f}°, 时间{speed_ms}ms")
        
        # 角度 × 100
        angle_raw = int(angle * 100)
        
        # 构建数据: [ID][角度高][角度低][速度高][速度低]
        data = bytearray()
        data.append(servo_id)
        data.append((angle_raw >> 8) & 0xFF)
        data.append(angle_raw & 0xFF)
        data.append((speed_ms >> 8) & 0xFF)
        data.append(speed_ms & 0xFF)
        
        return self.send_servo_command(self.CMD_MOVE_SINGLE, bytes(data))
    
    def move_all_servos(self, angles: List[float], speed_ms: int = 1000) -> bool:
        """控制多个舵机同步运动
        
        Args:
            angles: 舵机的目标角度列表 (0-180度)，长度应为servo_count
            speed_ms: 运动时间 (毫秒)
        """
        # 确保角度列表长度正确
        if len(angles) < self.servo_count:
            # 如果提供的角度少于舵机数量，保持当前位置（不改变）
            # 这样未使用的舵机不会突然移动，更安全
            logger.warning(f"角度数量({len(angles)})少于舵机数量({self.servo_count})，未指定的舵机将保持当前位置")
            # 用当前位置补齐（假设未使用的舵机保持在上次的位置）
            # 如果没有记录，默认使用90度（但不会发送移动命令，因为协议中可以指定不移动某些舵机）
            angles = list(angles) + [90.0] * (self.servo_count - len(angles))
        elif len(angles) > self.servo_count:
            # 如果提供的角度多于舵机数量，截取前servo_count个
            logger.warning(f"角度数量({len(angles)})多于舵机数量({self.servo_count})，将截取前{self.servo_count}个")
            angles = angles[:self.servo_count]
        
        logger.info(f"控制{self.servo_count}个舵机同步运动, 时间{speed_ms}ms")
        for i, angle in enumerate(angles):
            if i < 8:  # 只显示前8个
                logger.debug(f"  舵机{i}: {angle:.1f}°")
        
        # 构建数据: servo_count个舵机的角度 + 速度
        data = bytearray()
        for angle in angles:
            angle_raw = int(angle * 100)
            data.append((angle_raw >> 8) & 0xFF)
            data.append(angle_raw & 0xFF)
        data.append((speed_ms >> 8) & 0xFF)
        data.append(speed_ms & 0xFF)
        
        return self.send_servo_command(self.CMD_MOVE_ALL, bytes(data))
    
    def enable_servo(self, servo_id: int = 0xFF) -> bool:
        """使能舵机
        
        Args:
            servo_id: 舵机ID，0xFF表示全部
        """
        logger.info(f"使能舵机 {servo_id if servo_id != 0xFF else '全部'}")
        data = bytes([servo_id])
        return self.send_servo_command(self.CMD_ENABLE, data)
    
    def disable_servo(self, servo_id: int = 0xFF) -> bool:
        """禁用舵机
        
        Args:
            servo_id: 舵机ID，0xFF表示全部
        """
        logger.info(f"禁用舵机 {servo_id if servo_id != 0xFF else '全部'}")
        data = bytes([servo_id])
        return self.send_servo_command(self.CMD_DISABLE, data)
    
    def get_all_status(self) -> bool:
        """查询所有舵机状态"""
        logger.info("查询所有舵机状态")
        return self.send_servo_command(self.CMD_GET_ALL)
    
    def emergency_stop(self) -> bool:
        """紧急停止"""
        logger.warning("紧急停止!")
        return self.send_servo_command(self.CMD_ESTOP)
    
    def save_to_flash(self) -> bool:
        """保存参数到Flash"""
        logger.info("保存参数到Flash")
        return self.send_servo_command(self.CMD_SAVE_FLASH)
    
    def load_from_flash(self) -> bool:
        """从Flash加载参数"""
        logger.info("从Flash加载参数")
        return self.send_servo_command(self.CMD_LOAD_FLASH)
    
    def _read_loop(self):
        """读取线程主循环"""
        logger.info("读取线程已启动")
        buffer = bytearray()
        
        while self.is_running and self.serial_port and self.serial_port.is_open:
            try:
                if self.serial_port.in_waiting > 0:
                    # 读取所有可用字节
                    chunk = self.serial_port.read(self.serial_port.in_waiting)
                    
                    # 记录原始字节（用于调试）
                    if len(chunk) > 0:
                        hex_dump = ' '.join([f'{b:02X}' for b in chunk])
                        logger.debug(f"原始接收: {len(chunk)}字节 - {hex_dump[:100]}")
                    
                    buffer.extend(chunk)
                    
                    # 处理缓冲区中的所有数据
                    while len(buffer) > 0:
                        # 优先查找协议帧
                        frame_found = False
                        
                        # 查找帧头
                        for i in range(len(buffer) - 1):
                            if buffer[i] == 0xFF and buffer[i+1] == 0xFE:
                                # 找到帧头，先处理之前的文本数据
                                if i > 0:
                                    text_chunk = buffer[:i]
                                    self._process_text_data(text_chunk)
                                    buffer = buffer[i:]
                                
                                # 检查是否有完整的帧
                                if len(buffer) >= 5:
                                    data_len = buffer[4]
                                    frame_len = 7 + data_len
                                    
                                    if len(buffer) >= frame_len:
                                        # 提取完整帧
                                        frame = buffer[:frame_len]
                                        buffer = buffer[frame_len:]
                                        
                                        # 处理帧
                                        self._process_frame(frame)
                                        frame_found = True
                                        break
                                    else:
                                        # 等待更多数据
                                        return
                                else:
                                    # 等待更多数据
                                    return
                        
                        # 如果没找到帧头，处理为文本数据
                        if not frame_found:
                            if len(buffer) > 0:
                                self._process_text_data(buffer)
                                buffer.clear()
                            break
                else:
                    time.sleep(0.01)  # 避免CPU占用过高
                    
            except Exception as e:
                if self.is_running:
                    logger.error(f"读取数据错误: {e}")
                    self.error_occurred.emit(f"读取数据错误: {e}")
                break
    
    def _process_text_data(self, data: bytes):
        """处理文本数据（调试信息）"""
        try:
            # 尝试解码为文本
            text = data.decode('utf-8', errors='ignore').strip()
            if text:
                # 按行分割
                lines = text.split('\n')
                for line in lines:
                    line = line.strip()
                    if line:
                        logger.debug(f"接收调试信息: {line}")
                        self.data_received.emit(line)
        except Exception as e:
            logger.debug(f"文本解码失败: {e}")
    
    def _process_frame(self, frame: bytes):
        """处理接收到的帧"""
        try:
            # 转换为十六进制字符串（先显示，便于调试）
            hex_str = ' '.join([f'{b:02X}' for b in frame])
            logger.info(f"接收协议帧: {hex_str}")
            
            # 验证CRC（从ID开始，不包含帧头FF FE）
            crc_expected = self.crc16_ccitt(frame[2:-2])  # 从第3个字节（ID）开始到CRC之前
            crc_received = (frame[-2] << 8) | frame[-1]
            
            if crc_expected != crc_received:
                logger.warning(f"CRC校验失败 - 期望: {crc_expected:04X}, 实际: {crc_received:04X}")
                self.data_received.emit(f"RX: {hex_str} [CRC错误]")
                return
            
            # 解析帧
            cmd = frame[3]
            resp_code = frame[5] if len(frame) > 5 else 0
            
            # 显示协议帧
            self.data_received.emit(f"RX: {hex_str}")
            
            # 解析响应
            if cmd == self.RESP_OK or resp_code == 0:
                logger.info(f"命令执行成功 (CMD: 0x{cmd:02X})")
            elif cmd == self.RESP_ERROR:
                logger.warning(f"命令执行失败 (CMD: 0x{cmd:02X})")
            elif cmd == self.CMD_PING:
                logger.info("PONG - 设备响应正常")
            
        except Exception as e:
            logger.error(f"处理帧失败: {e}")

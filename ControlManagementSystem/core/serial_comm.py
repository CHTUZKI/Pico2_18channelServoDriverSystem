#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
串口通信模块
与Raspberry Pi Pico 2舵机控制器通信
使用自定义协议（帧格式 + CRC-16校验）
支持18个舵机（编号0-17）
"""

import serial
import serial.tools.list_ports
import time
import threading
from typing import List, Optional, Callable, Dict, Any
from PyQt5.QtCore import QObject, pyqtSignal
import logging
from .logger import get_logger, get_serial_logger

# 应用日志：记录上位机操作
logger = get_logger()
# 串口日志：记录串口收发数据
serial_logger = get_serial_logger()

class SerialComm(QObject):
    """串口通信类 - 舵机控制协议"""
    
    # 命令定义
    CMD_MOVE_SINGLE = 0x01
    CMD_MOVE_ALL = 0x03
    CMD_MOVE_TRAPEZOID = 0x04        # 梯形速度运动
    CMD_TRAJ_ADD_POINT = 0x06        # 添加轨迹点
    CMD_TRAJ_START = 0x07            # 开始执行轨迹
    CMD_TRAJ_STOP = 0x08             # 停止轨迹
    CMD_TRAJ_CLEAR = 0x09            # 清空轨迹
    CMD_TRAJ_GET_INFO = 0x0A         # 查询轨迹信息
    CMD_GET_SINGLE = 0x10
    CMD_GET_ALL = 0x11
    CMD_ENABLE = 0x20
    CMD_DISABLE = 0x21
    CMD_SAVE_FLASH = 0x30
    CMD_LOAD_FLASH = 0x31
    CMD_SET_START_POSITIONS = 0x33
    
    # 运动缓冲区管理命令（Look-Ahead Planner）
    CMD_ADD_MOTION_BLOCK = 0x40      # 添加运动指令到缓冲区（位置模式）
    CMD_START_MOTION = 0x41          # 开始执行缓冲区指令
    CMD_STOP_MOTION = 0x42           # 停止执行
    CMD_PAUSE_MOTION = 0x43          # 暂停执行
    CMD_RESUME_MOTION = 0x44         # 恢复执行
    CMD_CLEAR_BUFFER = 0x45          # 清空缓冲区
    CMD_GET_BUFFER_STATUS = 0x46     # 查询缓冲区状态
    
    # 360度连续旋转舵机命令
    CMD_ADD_CONTINUOUS_MOTION = 0x50  # 添加速度控制块到缓冲区
    CMD_SERVO_360_SET_SPEED = 0x51   # 直接设置速度（即时命令）
    CMD_SERVO_360_SOFT_STOP = 0x52   # 软停止
    CMD_SERVO_360_SET_ACCEL = 0x53   # 设置加减速参数
    CMD_SERVO_360_CALIBRATE = 0x54   # 校准中点
    CMD_SERVO_360_GET_INFO = 0x55    # 查询360度舵机状态
    
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
        
        # 计算CRC（从ID到数据结束，不包括帧头）
        crc = self.crc16_ccitt(frame[2:])
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
            
            # 发送日志（详细记录）
            hex_str = ' '.join([f'{b:02X}' for b in frame])
            serial_logger.info(f"[TX] CMD=0x{cmd:02X} LEN={len(data)} 帧={hex_str}")
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
            servo_id: 舵机ID (0-17)
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
    
    # ==================== 运动缓冲区管理（新架构） ====================
    
    def add_motion_block(self, timestamp_ms: int, servo_id: int, angle: float, 
                        velocity: float, acceleration: float, deceleration: float = 0.0) -> bool:
        """
        添加运动指令到缓冲区
        
        Args:
            timestamp_ms: 执行时间戳（从start开始计时，单位ms）
            servo_id: 舵机ID (0-17)
            angle: 目标角度（度）
            velocity: 速度（度/秒）
            acceleration: 加速度（度/秒²）
            deceleration: 减速度（度/秒²，0表示与加速度相同）
        
        Returns:
            bool: 是否成功
        """
        # 数据格式：[timestamp_ms(4)] [servo_id(1)] [angle(2)] [velocity(2)] [accel(2)] [decel(2)]
        # total: 13字节
        
        # timestamp_ms (4字节, 小端序)
        data = bytearray()
        data.extend(timestamp_ms.to_bytes(4, 'little'))
        
        # servo_id (1字节)
        data.append(servo_id)
        
        # angle (2字节, 有符号, 0.01度精度)
        angle_raw = int(angle * 100)
        data.extend(angle_raw.to_bytes(2, 'little', signed=True))
        
        # velocity (2字节, 无符号, 0.1度/秒精度)
        vel_raw = int(velocity * 10)
        data.extend(vel_raw.to_bytes(2, 'little'))
        
        # acceleration (2字节, 无符号, 0.1度/秒²精度)
        accel_raw = int(acceleration * 10)
        data.extend(accel_raw.to_bytes(2, 'little'))
        
        # deceleration (2字节, 无符号, 0.1度/秒²精度)
        decel_raw = int(deceleration * 10) if deceleration > 0 else 0
        data.extend(decel_raw.to_bytes(2, 'little'))
        
        return self.send_servo_command(self.CMD_ADD_MOTION_BLOCK, bytes(data))
    
    def start_motion(self) -> bool:
        """开始执行缓冲区指令"""
        return self.send_servo_command(self.CMD_START_MOTION, bytes())
    
    def stop_motion(self) -> bool:
        """停止执行"""
        return self.send_servo_command(self.CMD_STOP_MOTION, bytes())
    
    def pause_motion(self) -> bool:
        """暂停执行"""
        return self.send_servo_command(self.CMD_PAUSE_MOTION, bytes())
    
    def resume_motion(self) -> bool:
        """恢复执行"""
        return self.send_servo_command(self.CMD_RESUME_MOTION, bytes())
    
    def clear_buffer(self) -> bool:
        """清空缓冲区"""
        return self.send_servo_command(self.CMD_CLEAR_BUFFER, bytes())
    
    def get_buffer_status(self) -> dict:
        """
        查询缓冲区状态
        
        Returns:
            dict: {'count': 已用, 'running': 是否运行, 'paused': 是否暂停, 'available': 可用空间}
        """
        response = self.send_servo_command(self.CMD_GET_BUFFER_STATUS, bytes())
        if response and len(response) >= 4:
            return {
                'count': response[0],
                'running': bool(response[1]),
                'paused': bool(response[2]),
                'available': response[3]
            }
        return {'count': 0, 'running': False, 'paused': False, 'available': 0}
    
    def save_to_flash(self) -> bool:
        """保存参数到Flash"""
        logger.info("保存参数到Flash")
        return self.send_servo_command(self.CMD_SAVE_FLASH)
    
    def load_from_flash(self) -> bool:
        """从Flash加载参数"""
        logger.info("从Flash加载参数")
        return self.send_servo_command(self.CMD_LOAD_FLASH)
    
    def move_servo_to(self, servo_id: int, angle: float, speed: int = 0) -> bool:
        """移动舵机到指定角度
        
        Args:
            servo_id: 舵机ID (0-17)
            angle: 目标角度 (0-180度)
            speed: 速度 (ms)，暂未使用
        
        Returns:
            bool: 是否发送成功
        """
        # 限制角度范围
        angle = max(0.0, min(180.0, angle))
        
        # 转换角度（角度×100，例如90.5度 = 9050）
        angle_raw = int(angle * 100)
        angle_high = (angle_raw >> 8) & 0xFF
        angle_low = angle_raw & 0xFF
        
        # 速度（暂未使用，设为0）
        speed_high = (speed >> 8) & 0xFF
        speed_low = speed & 0xFF
        
        # 数据格式：[ID][角度高][角度低][速度高][速度低]
        data = bytes([servo_id, angle_high, angle_low, speed_high, speed_low])
        
        logger.info(f"移动舵机{servo_id}到{angle:.1f}度")
        return self.send_servo_command(self.CMD_MOVE_SINGLE, data)
    
    def jog_servo(self, servo_id: int, current_angle: float, delta: int) -> tuple:
        """相对移动舵机（Jog）
        
        Args:
            servo_id: 舵机ID (0-17)
            current_angle: 当前角度
            delta: 相对角度变化 (例如 +10 或 -10)
        
        Returns:
            tuple: (是否发送成功, 新的目标角度)
        """
        target_angle = current_angle + delta
        # 限制角度范围
        target_angle = max(0.0, min(180.0, target_angle))
        
        success = self.move_servo_to(servo_id, target_angle)
        return (success, target_angle)
    
    def set_start_positions(self, angles: list) -> bool:
        """设置起始位置
        
        Args:
            angles: 18个舵机的角度列表 (0-180度)
        
        Returns:
            bool: 是否成功
        """
        if len(angles) != 18:
            logger.error(f"角度列表长度错误: {len(angles)}, 需要18个")
            return False
        
        logger.info("设置起始位置")
        
        # 构建数据：[angle0_H][angle0_L] ... [angle17_H][angle17_L]
        data = bytearray()
        for angle in angles:
            # 角度 × 100，例如90.5度 = 9050
            angle_x100 = int(angle * 100)
            if angle_x100 < 0:
                angle_x100 = 0
            if angle_x100 > 18000:
                angle_x100 = 18000
            
            data.append((angle_x100 >> 8) & 0xFF)  # 高字节
            data.append(angle_x100 & 0xFF)          # 低字节
        
        return self.send_servo_command(self.CMD_SET_START_POSITIONS, bytes(data))
    
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
                        serial_logger.debug(f"[RX RAW] {len(chunk)}字节 - {hex_dump[:100]}")
                    
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
                        serial_logger.info(f"[RX MCU] {line}")
                        self.data_received.emit(line)
        except Exception as e:
            logger.debug(f"文本解码失败: {e}")
    
    def _process_frame(self, frame: bytes):
        """处理接收到的帧"""
        try:
            # 转换为十六进制字符串（先显示，便于调试）
            hex_str = ' '.join([f'{b:02X}' for b in frame])
            
            # 验证CRC（从ID开始，不包含帧头FF FE）
            crc_expected = self.crc16_ccitt(frame[2:-2])  # 从第3个字节（ID）开始到CRC之前
            crc_received = (frame[-2] << 8) | frame[-1]
            
            if crc_expected != crc_received:
                serial_logger.warning(f"[RX] CRC错误! 帧={hex_str} 期望={crc_expected:04X} 实际={crc_received:04X}")
                self.data_received.emit(f"RX: {hex_str} [CRC错误]")
                return
            
            # 解析帧
            cmd = frame[3]
            resp_code = frame[5] if len(frame) > 5 else 0
            data_len = frame[4] if len(frame) > 4 else 0
            
            # 详细日志（RX数据）
            resp_str = {0x00: "OK", 0x01: "ERROR", 0x02: "INVALID_CMD",
                       0x03: "INVALID_PARAM", 0x04: "CRC_ERR", 0x05: "TIMEOUT", 0x06: "BUSY"}.get(resp_code, f"0x{resp_code:02X}")
            serial_logger.info(f"[RX] CMD=0x{cmd:02X} RESP={resp_str} LEN={data_len} 帧={hex_str}")
            
            # 显示协议帧
            self.data_received.emit(f"RX: {hex_str}")
            
            # 解析响应（详细说明）
            if resp_code == 0x00:
                logger.info(f"  └─ 响应: 成功 (OK)")
            elif resp_code == 0x01:
                logger.warning(f"  └─ 响应: 错误 (ERROR)")
            elif resp_code == 0x02:
                logger.warning(f"  └─ 响应: 无效命令 (INVALID_CMD)")
            elif resp_code == 0x03:
                logger.warning(f"  └─ 响应: 无效参数 (INVALID_PARAM)")
            elif cmd == self.CMD_PING:
                logger.info("  └─ 响应: PONG (设备响应正常)")
            
        except Exception as e:
            logger.error(f"处理帧失败: {e}")
    
    # ==================== 梯形速度和轨迹规划接口 ====================
    
    def move_servo_trapezoid(self, servo_id: int, angle: float, 
                            velocity: float = 30.0, 
                            acceleration: float = 60.0,
                            deceleration: float = 0.0) -> bool:
        """使用梯形速度曲线移动舵机
        
        Args:
            servo_id: 舵机ID (0-17)
            angle: 目标角度 (0-180度)
            velocity: 最大速度 (度/秒), 默认30
            acceleration: 加速度 (度/秒²), 默认60
            deceleration: 减速度 (度/秒²), 0表示使用加速度值
        
        Returns:
            bool: 是否发送成功
        """
        logger.info(f"[TRACE-PC] move_servo_trapezoid ENTER: servo_id={servo_id}, angle={angle}, v={velocity}, a={acceleration}, d={deceleration}")
        
        # 限制角度范围
        angle = max(0.0, min(180.0, angle))
        velocity = max(1.0, min(180.0, velocity))
        acceleration = max(1.0, min(500.0, acceleration))
        
        logger.info(f"[TRACE-PC] 参数限制后: angle={angle}, v={velocity}, a={acceleration}, d={deceleration}")
        
        # 转换参数（×100 / ×10）
        angle_raw = int(angle * 100)
        velocity_raw = int(velocity * 10)
        accel_raw = int(acceleration * 10)
        decel_raw = int(deceleration * 10) if deceleration > 0 else 0
        
        logger.info(f"[TRACE-PC] 参数转换: angle_raw={angle_raw}, velocity_raw={velocity_raw}, accel_raw={accel_raw}, decel_raw={decel_raw}")
        
        # 数据格式（9字节）：[ID][角度H][角度L][速度H][速度L][加速H][加速L][减速H][减速L]
        data = bytes([
            servo_id,
            (angle_raw >> 8) & 0xFF,
            angle_raw & 0xFF,
            (velocity_raw >> 8) & 0xFF,
            velocity_raw & 0xFF,
            (accel_raw >> 8) & 0xFF,
            accel_raw & 0xFF,
            (decel_raw >> 8) & 0xFF,
            decel_raw & 0xFF
        ])
        
        logger.info(f"[TRACE-PC] 构建数据包: {' '.join([f'{b:02X}' for b in data])}")
        logger.info(f"梯形速度移动舵机{servo_id}: 角度={angle:.1f}°, 速度={velocity:.1f}°/s, 加速度={acceleration:.1f}°/s²")
        
        result = self.send_servo_command(self.CMD_MOVE_TRAPEZOID, data)
        logger.info(f"[TRACE-PC] send_servo_command返回: {result}")
        
        return result
    
    def trajectory_add_point(self, servo_id: int, position: float,
                            velocity: float = 30.0,
                            acceleration: float = 60.0,
                            deceleration: float = 0.0,
                            dwell_time_ms: int = 0) -> bool:
        """添加轨迹点
        
        Args:
            servo_id: 舵机ID (0-17)
            position: 目标位置 (0-180度)
            velocity: 最大速度 (度/秒)
            acceleration: 加速度 (度/秒²)
            deceleration: 减速度 (度/秒²)
            dwell_time_ms: 停留时间 (毫秒)
        
        Returns:
            bool: 是否发送成功
        """
        # 限制参数范围
        position = max(0.0, min(180.0, position))
        velocity = max(1.0, min(180.0, velocity))
        acceleration = max(1.0, min(500.0, acceleration))
        dwell_time_ms = max(0, min(60000, dwell_time_ms))
        
        # 转换参数
        pos_raw = int(position * 100)
        velocity_raw = int(velocity * 10)
        accel_raw = int(acceleration * 10)
        decel_raw = int(deceleration * 10) if deceleration > 0 else 0
        
        # 数据格式（11字节）
        data = bytes([
            servo_id,
            (pos_raw >> 8) & 0xFF,
            pos_raw & 0xFF,
            (velocity_raw >> 8) & 0xFF,
            velocity_raw & 0xFF,
            (accel_raw >> 8) & 0xFF,
            accel_raw & 0xFF,
            (decel_raw >> 8) & 0xFF,
            decel_raw & 0xFF,
            (dwell_time_ms >> 8) & 0xFF,
            dwell_time_ms & 0xFF
        ])
        
        logger.info(f"添加轨迹点: 舵机{servo_id}, 位置={position:.1f}°, 停留={dwell_time_ms}ms")
        return self.send_servo_command(self.CMD_TRAJ_ADD_POINT, data)
    
    def trajectory_start(self, servo_id: int, loop: bool = False) -> bool:
        """开始执行轨迹
        
        Args:
            servo_id: 舵机ID (0-17)
            loop: 是否循环执行
        
        Returns:
            bool: 是否发送成功
        """
        # 数据格式（2字节）：[ID][loop]
        data = bytes([servo_id, 1 if loop else 0])
        logger.info(f"开始轨迹: 舵机{servo_id}, 循环={'是' if loop else '否'}")
        return self.send_servo_command(self.CMD_TRAJ_START, data)
    
    def trajectory_stop(self, servo_id: int) -> bool:
        """停止轨迹执行
        
        Args:
            servo_id: 舵机ID (0-17)
        
        Returns:
            bool: 是否发送成功
        """
        data = bytes([servo_id])
        logger.info(f"停止轨迹: 舵机{servo_id}")
        return self.send_servo_command(self.CMD_TRAJ_STOP, data)
    
    def trajectory_clear(self, servo_id: int) -> bool:
        """清空轨迹
        
        Args:
            servo_id: 舵机ID (0-17)
        
        Returns:
            bool: 是否发送成功
        """
        data = bytes([servo_id])
        logger.info(f"清空轨迹: 舵机{servo_id}")
        return self.send_servo_command(self.CMD_TRAJ_CLEAR, data)
    
    def trajectory_get_info(self, servo_id: int) -> bool:
        """查询轨迹信息
        
        Args:
            servo_id: 舵机ID (0-17)
        
        Returns:
            bool: 是否发送成功
        """
        data = bytes([servo_id])
        logger.info(f"查询轨迹信息: 舵机{servo_id}")
        return self.send_servo_command(self.CMD_TRAJ_GET_INFO, data)
    
    # ==================== 360度连续旋转舵机API ====================
    
    def add_continuous_motion(self, timestamp_ms: int, servo_id: int, 
                            speed_pct: int, accel_rate: int = 50, 
                            decel_rate: int = 0, duration_ms: int = 0) -> bool:
        """
        添加360度舵机速度控制块到缓冲区
        
        Args:
            timestamp_ms: 执行时间戳（从start开始计时，单位ms）
            servo_id: 舵机ID (0-17)
            speed_pct: 目标速度百分比 (-100 到 +100)
            accel_rate: 加速度 (%/秒，默认50)
            decel_rate: 减速度 (%/秒，0表示与加速度相同）
            duration_ms: 持续时间（毫秒，0表示持续到下一个块）
        
        Returns:
            bool: 是否成功
        """
        # 数据格式：[timestamp_ms(4)] [servo_id(1)] [speed(1,signed)] [accel(1)] [decel(1)] [duration_ms(2)]
        # total: 10字节
        
        # 限制速度范围
        speed_pct = max(-100, min(100, speed_pct))
        
        data = bytearray()
        
        # timestamp_ms (4字节, 小端序)
        data.extend(timestamp_ms.to_bytes(4, 'little'))
        
        # servo_id (1字节)
        data.append(servo_id)
        
        # speed_pct (1字节, 有符号)
        data.append(speed_pct & 0xFF)
        
        # accel_rate (1字节)
        data.append(accel_rate & 0xFF)
        
        # decel_rate (1字节)
        decel_val = decel_rate if decel_rate > 0 else 0
        data.append(decel_val & 0xFF)
        
        # duration_ms (2字节, 小端序)
        data.extend(duration_ms.to_bytes(2, 'little'))
        
        return self.send_servo_command(self.CMD_ADD_CONTINUOUS_MOTION, bytes(data))
    
    def servo_360_set_speed(self, servo_id: int, speed_pct: int) -> bool:
        """
        直接设置360度舵机速度（即时命令，不经过规划器）
        
        Args:
            servo_id: 舵机ID (0-17)
            speed_pct: 速度百分比 (-100 到 +100)
        
        Returns:
            bool: 是否发送成功
        """
        # 限制速度范围
        speed_pct = max(-100, min(100, speed_pct))
        
        # 数据格式：[servo_id(1)] [speed(1,signed)]
        data = bytes([servo_id, speed_pct & 0xFF])
        
        logger.info(f"360度舵机{servo_id}设置速度: {speed_pct}%")
        return self.send_servo_command(self.CMD_SERVO_360_SET_SPEED, data)
    
    def servo_360_soft_stop(self, servo_id: int = 0xFF) -> bool:
        """
        360度舵机软停止（平滑减速到0）
        
        Args:
            servo_id: 舵机ID (0-17, 0xFF=全部)
        
        Returns:
            bool: 是否发送成功
        """
        data = bytes([servo_id])
        logger.info(f"360度舵机软停止: {'全部' if servo_id == 0xFF else f'S{servo_id}'}")
        return self.send_servo_command(self.CMD_SERVO_360_SOFT_STOP, data)
    
    def servo_360_set_accel(self, servo_id: int, accel_rate: int, decel_rate: int = 0) -> bool:
        """
        设置360度舵机加减速参数
        
        Args:
            servo_id: 舵机ID (0-17)
            accel_rate: 加速度 (%/秒, 1-100)
            decel_rate: 减速度 (%/秒, 0表示与加速度相同)
        
        Returns:
            bool: 是否发送成功
        """
        decel_val = decel_rate if decel_rate > 0 else accel_rate
        data = bytes([servo_id, accel_rate & 0xFF, decel_val & 0xFF])
        
        logger.info(f"设置360度舵机{servo_id}加减速: accel={accel_rate}, decel={decel_val}")
        return self.send_servo_command(self.CMD_SERVO_360_SET_ACCEL, data)
    
    def servo_360_get_info(self, servo_id: int) -> dict:
        """
        查询360度舵机状态
        
        Args:
            servo_id: 舵机ID (0-17)
        
        Returns:
            dict: {'current_speed': 当前速度, 'target_speed': 目标速度, 
                   'enabled': 是否使能, 'moving': 是否运动中}
        """
        data = bytes([servo_id])
        response = self.send_servo_command(self.CMD_SERVO_360_GET_INFO, data)
        
        if response and len(response) >= 4:
            # 响应数据：[current_speed(1)] [target_speed(1)] [enabled(1)] [moving(1)]
            current_speed = int.from_bytes([response[0]], 'little', signed=True)
            target_speed = int.from_bytes([response[1]], 'little', signed=True)
            
            return {
                'current_speed': current_speed,
                'target_speed': target_speed,
                'enabled': bool(response[2]),
                'moving': bool(response[3])
            }
        
        return {'current_speed': 0, 'target_speed': 0, 'enabled': False, 'moving': False}

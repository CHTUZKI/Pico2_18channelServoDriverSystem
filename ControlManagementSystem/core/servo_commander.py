#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
舵机命令生成器
将时间轴数据转换为舵机控制命令序列
"""

from typing import List, Dict, Any, Tuple
from models.timeline_data import TimelineData, MotorTrack, LoopMode
from models.component import Component, ComponentType
import logging
import time

logger = logging.getLogger('motor_controller')

class ServoCommander:
    """舵机命令生成器 - 支持时间轴同步运动"""
    
    def __init__(self):
        self.servo_count = 18  # 18个舵机 (0-17)
        self.current_positions = [90.0] * 18  # 当前各舵机位置（度），初始化为90度
    
    def generate_command_sequence(self, timeline_data: TimelineData) -> List[Dict[str, Any]]:
        """生成舵机命令序列
        
        Returns:
            命令序列列表，每个命令格式:
            {
                'time': 时间点(秒),
                'type': 'motion' | 'delay',
                'servos': {servo_id: angle, ...},  # motion类型
                'duration': 延时时长(秒),  # delay类型
                'speed_ms': 运动时间(毫秒)  # motion类型
            }
        """
        logger.info("开始生成舵机命令序列")
        
        # 重置当前位置
        self.current_positions = [90.0] * 8
        
        # 收集所有事件
        all_events = []
        for track in timeline_data.tracks:
            for component in track.components:
                # 处理运动部件
                if component.type in [ComponentType.FORWARD_ROTATION, ComponentType.REVERSE_ROTATION]:
                    all_events.append({
                        'component': component,
                        'track': track,
                        'servo_id': track.motor_id,
                        'start_time': component.start_time,
                        'end_time': component.start_time + component.duration,
                        'type': 'motion'
                    })
                # 处理延时部件
                elif component.type == ComponentType.DELAY:
                    all_events.append({
                        'component': component,
                        'track': track,
                        'servo_id': track.motor_id,
                        'start_time': component.start_time,
                        'end_time': component.start_time + component.duration,
                        'type': 'delay'
                    })
                # 处理归零部件
                elif component.type == ComponentType.HOME:
                    all_events.append({
                        'component': component,
                        'track': track,
                        'servo_id': track.motor_id,
                        'start_time': component.start_time,
                        'end_time': component.start_time + component.duration,
                        'type': 'home'
                    })
        
        logger.info(f"找到 {len(all_events)} 个事件")
        
        # 按开始时间排序
        all_events.sort(key=lambda x: x['start_time'])
        
        # 按时间分组处理所有事件
        time_groups = {}
        for event in all_events:
            start_time = event['start_time']
            if start_time not in time_groups:
                time_groups[start_time] = []
            time_groups[start_time].append(event)
        
        logger.info(f"时间分组: {sorted(time_groups.keys())}")
        
        # 生成命令序列
        command_sequence = []
        
        for time_point in sorted(time_groups.keys()):
            events_at_time = time_groups[time_point]
            logger.info(f"处理时间 {time_point}s 的 {len(events_at_time)} 个事件")
            
            # 分离运动事件和延时事件
            motion_events = [e for e in events_at_time if e['type'] in ['motion', 'home']]
            delay_events = [e for e in events_at_time if e['type'] == 'delay']
            
            # 生成同步运动命令
            if motion_events:
                motion_cmd = self._generate_motion_command(motion_events, time_point)
                if motion_cmd:
                    command_sequence.append(motion_cmd)
                    logger.info(f"  生成运动命令: {len(motion_cmd['servos'])} 个舵机")
            
            # 生成延时命令
            for delay_event in delay_events:
                component = delay_event['component']
                delay_time = component.parameters.get('delay_time', 1.0)
                delay_cmd = {
                    'time': time_point,
                    'type': 'delay',
                    'duration': delay_time
                }
                command_sequence.append(delay_cmd)
                logger.info(f"  生成延时命令: {delay_time}秒")
        
        logger.info(f"命令序列生成完成，共{len(command_sequence)}条命令")
        return command_sequence
    
    def _generate_motion_command(self, events: List[Dict[str, Any]], time_point: float) -> Dict[str, Any]:
        """生成同步运动命令"""
        if not events:
            return None
        
        # 收集所有需要运动的舵机
        servo_angles = {}
        speed_ms = 1000  # 默认速度
        
        for event in events:
            component = event['component']
            servo_id = event['servo_id']
            
            if component.type == ComponentType.FORWARD_ROTATION:
                # 正转：绝对角度
                target_angle = component.parameters.get('target_angle', 90.0)
                speed_ms = component.parameters.get('speed_ms', 1000)
                
                servo_angles[servo_id] = target_angle
                logger.debug(f"    舵机{servo_id}正转: {target_angle}°")
                
            elif component.type == ComponentType.REVERSE_ROTATION:
                # 反转：绝对角度（负值已在参数中）
                target_angle = component.parameters.get('target_angle', -90.0)
                speed_ms = component.parameters.get('speed_ms', 1000)
                
                # 反转角度可能是负值，取绝对值
                if target_angle < 0:
                    target_angle = abs(target_angle)
                
                servo_angles[servo_id] = target_angle
                logger.debug(f"    舵机{servo_id}反转: {target_angle}°")
                
            elif component.type == ComponentType.HOME:
                # 归零：回到90度中位
                speed_ms = component.parameters.get('speed_ms', 1000)
                servo_angles[servo_id] = 90.0
                logger.debug(f"    舵机{servo_id}归零: 90°")
        
        if not servo_angles:
            return None
        
        # 更新当前位置
        for servo_id, angle in servo_angles.items():
            if 0 <= servo_id < self.servo_count:
                self.current_positions[servo_id] = angle
            else:
                logger.error(f"舵机ID超出范围: {servo_id}, 有效范围: 0-{self.servo_count-1}")
        
        return {
            'time': time_point,
            'type': 'motion',
            'servos': servo_angles,
            'speed_ms': speed_ms
        }
    
    def execute_sequence(self, serial_comm, sequence: List[Dict[str, Any]]) -> bool:
        """执行命令序列
        
        Args:
            serial_comm: 串口通信对象
            sequence: 命令序列
        
        Returns:
            是否执行成功
        """
        try:
            logger.info(f"开始执行命令序列，共{len(sequence)}条命令")
            
            # 先使能所有舵机
            logger.info("使能所有舵机")
            serial_comm.enable_servo(0xFF)
            time.sleep(0.2)
            
            for i, cmd in enumerate(sequence):
                logger.info(f"执行命令 {i+1}/{len(sequence)}")
                
                if cmd['type'] == 'motion':
                    # 运动命令
                    servos = cmd['servos']
                    speed_ms = cmd['speed_ms']
                    
                    # 准备所有舵机的角度（未指定的保持当前位置）
                    all_angles = self.current_positions.copy()
                    for servo_id, angle in servos.items():
                        if 0 <= servo_id < self.servo_count:
                            all_angles[servo_id] = angle
                        else:
                            logger.error(f"舵机ID超出范围: {servo_id}, 有效范围: 0-{self.servo_count-1}")
                    
                    # 发送命令
                    logger.info(f"  运动命令: 时间{speed_ms}ms，发送{len(all_angles)}个角度")
                    for servo_id, angle in servos.items():
                        logger.info(f"    舵机{servo_id}: {angle:.1f}°")
                    
                    if not serial_comm.move_all_servos(all_angles, speed_ms):
                        logger.error("发送运动命令失败")
                        return False
                    
                    # 等待运动完成（加上一点余量）
                    wait_time = speed_ms / 1000.0 + 0.1
                    logger.debug(f"  等待运动完成: {wait_time:.2f}秒")
                    time.sleep(wait_time)
                    
                elif cmd['type'] == 'delay':
                    # 延时命令
                    duration = cmd['duration']
                    logger.info(f"  延时: {duration}秒")
                    time.sleep(duration)
            
            logger.info("命令序列执行完成")
            return True
            
        except Exception as e:
            error_msg = f"执行命令序列失败: {e}"
            logger.error(error_msg)
            return False
    
    def generate_preview_text(self, timeline_data: TimelineData) -> str:
        """生成命令预览文本（用于显示）"""
        sequence = self.generate_command_sequence(timeline_data)
        
        lines = []
        lines.append("; 8轴舵机时间轴控制程序")
        lines.append("; 由舵机控制上位机自动生成")
        lines.append("")
        lines.append("; 协议: 自定义帧格式 + CRC-16校验")
        lines.append("; 帧格式: [0xFF][0xFE][ID][CMD][LEN][DATA][CRC_H][CRC_L]")
        lines.append("")
        
        for i, cmd in enumerate(sequence):
            lines.append(f"; === 命令 {i+1} ===")
            lines.append(f"; 时间: {cmd['time']:.2f}s")
            
            if cmd['type'] == 'motion':
                lines.append(f"; 类型: 运动命令")
                lines.append(f"; 速度: {cmd['speed_ms']}ms")
                for servo_id, angle in cmd['servos'].items():
                    lines.append(f";   舵机{servo_id}: {angle:.1f}°")
                lines.append(f"CMD: MOVE_ALL")
                
            elif cmd['type'] == 'delay':
                lines.append(f"; 类型: 延时")
                lines.append(f"; 时长: {cmd['duration']:.2f}s")
                lines.append(f"CMD: DELAY {cmd['duration']:.2f}s")
            
            lines.append("")
        
        lines.append("; 程序结束")
        
        return '\n'.join(lines)


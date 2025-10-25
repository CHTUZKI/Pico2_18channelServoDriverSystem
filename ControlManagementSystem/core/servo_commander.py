#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
舵机命令生成器
将时间轴数据转换为舵机控制命令序列
支持18个舵机（编号0-17）
"""

from typing import List, Dict, Any, Tuple
from models.timeline_data import TimelineData, MotorTrack, LoopMode
from models.component import Component, ComponentType
import logging
import time

logger = logging.getLogger('servo_controller')

class ServoCommander:
    """舵机命令生成器 - 支持18个舵机（编号0-17）时间轴同步运动"""
    
    def __init__(self):
        self.servo_count = 18  # 18个舵机（编号0-17）
        self.current_positions = [90.0] * 18  # 当前各舵机位置（度），初始化为90度
        self.should_stop = False  # 停止标志
    
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
        self.current_positions = [90.0] * 18
        
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
                    logger.info(f"  生成运动命令: {len(motion_cmd['servo_data'])} 个舵机")
            
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
        
        # 收集所有需要运动的舵机及其参数
        servo_data = {}  # {servo_id: {'angle': ..., 'component': ...}}
        speed_ms = 1000  # 默认速度
        
        for event in events:
            component = event['component']
            servo_id = event['servo_id']
            
            if component.type == ComponentType.FORWARD_ROTATION:
                # 正转：绝对角度
                target_angle = component.parameters.get('target_angle', 90.0)
                speed_ms = component.parameters.get('speed_ms', 1000)
                
                servo_data[servo_id] = {
                    'angle': target_angle,
                    'component': component
                }
                logger.debug(f"    舵机{servo_id}正转: {target_angle}°")
                
            elif component.type == ComponentType.REVERSE_ROTATION:
                # 反转：绝对角度（负值已在参数中）
                target_angle = component.parameters.get('target_angle', -90.0)
                speed_ms = component.parameters.get('speed_ms', 1000)
                
                # 反转角度可能是负值，取绝对值
                if target_angle < 0:
                    target_angle = abs(target_angle)
                
                servo_data[servo_id] = {
                    'angle': target_angle,
                    'component': component
                }
                logger.debug(f"    舵机{servo_id}反转: {target_angle}°")
                
            elif component.type == ComponentType.HOME:
                # 归零：回到90度中位
                speed_ms = component.parameters.get('speed_ms', 1000)
                servo_data[servo_id] = {
                    'angle': 90.0,
                    'component': component
                }
                logger.debug(f"    舵机{servo_id}归零: 90°")
        
        if not servo_data:
            return None
        
        # 更新当前位置
        for servo_id, data in servo_data.items():
            if 0 <= servo_id < self.servo_count:
                self.current_positions[servo_id] = data['angle']
            else:
                logger.error(f"舵机ID超出范围: {servo_id}, 有效范围: 0-{self.servo_count-1}")
        
        return {
            'time': time_point,
            'type': 'motion',
            'servo_data': servo_data,  # 改为包含component引用的字典
            'speed_ms': speed_ms
        }
    
    def execute_sequence(self, serial_comm, sequence: List[Dict[str, Any]], timeline_data=None) -> bool:
        """执行命令序列
        
        Args:
            serial_comm: 串口通信对象
            sequence: 命令序列
            timeline_data: 时间轴数据（用于检查循环模式）
        
        Returns:
            是否执行成功
        """
        try:
            # 检查是否有任何轨道设置了循环模式
            should_loop = False
            if timeline_data:
                for track in timeline_data.tracks:
                    if track.loop_mode == LoopMode.LOOP and len(track.components) > 0:
                        should_loop = True
                        break
            
            if should_loop:
                logger.info(f"检测到循环模式，命令序列将持续循环执行（共{len(sequence)}条命令）")
            else:
                logger.info(f"单次执行模式，共{len(sequence)}条命令")
            
            # 先使能所有舵机
            logger.info("使能所有舵机")
            serial_comm.enable_servo(0xFF)
            time.sleep(0.2)
            
            # 重置停止标志
            self.should_stop = False
            
            # 循环执行或单次执行
            loop_count = 0
            while True:
                # 检查停止标志
                if self.should_stop:
                    logger.info("检测到停止信号，退出执行")
                    return False
                
                loop_count += 1
                if should_loop:
                    logger.info(f"=== 开始第 {loop_count} 次循环 ===")
                
                for i, cmd in enumerate(sequence):
                    # 每个命令前检查停止标志
                    if self.should_stop:
                        logger.info("检测到停止信号，退出执行")
                        return False
                    logger.info(f"执行命令 {i+1}/{len(sequence)}")
                    
                    if cmd['type'] == 'motion':
                        # 运动命令
                        servo_data = cmd['servo_data']
                        speed_ms = cmd['speed_ms']
                        
                        # 按运动模式分组
                        time_based_servos = {}    # 基于时间的舵机
                        trapezoid_servos = []     # 梯形速度的舵机（需要逐个发送）
                        max_wait_time = 0.0
                        
                        logger.info("=" * 80)
                        logger.info(f"[执行命令 {i+1}/{len(sequence)}] 时间点={cmd['time']:.2f}s")
                        
                        for servo_id, data in servo_data.items():
                            angle = data['angle']
                            component = data['component']
                            motion_mode = component.parameters.get('motion_mode', 'time')
                            current_angle = self.current_positions[servo_id]
                            
                            if motion_mode == 'trapezoid':
                                # 梯形速度模式
                                velocity = component.parameters.get('velocity', 30.0)
                                acceleration = component.parameters.get('acceleration', 60.0)
                                deceleration = component.parameters.get('deceleration', 0.0)
                                
                                trapezoid_servos.append({
                                    'servo_id': servo_id,
                                    'angle': angle,
                                    'velocity': velocity,
                                    'acceleration': acceleration,
                                    'deceleration': deceleration
                                })
                                
                                # 估算梯形速度的运动时间
                                distance = abs(angle - current_angle)
                                # 简化估算：时间 ≈ 距离/平均速度 + 加减速时间
                                estimated_time = distance / (velocity * 0.7) + (velocity / acceleration)
                                max_wait_time = max(max_wait_time, estimated_time)
                                
                                logger.info(f"  舵机{servo_id}[梯形]: {current_angle:.1f}° → {angle:.1f}° (Δ={distance:.1f}°) v={velocity:.1f}°/s a={acceleration:.1f}°/s² 预计{estimated_time:.2f}s")
                            else:
                                # 基于时间模式
                                time_based_servos[servo_id] = angle
                                max_wait_time = max(max_wait_time, speed_ms / 1000.0)
                                distance = abs(angle - current_angle)
                                logger.info(f"  舵机{servo_id}[时间]: {current_angle:.1f}° → {angle:.1f}° (Δ={distance:.1f}°) 时间={speed_ms}ms")
                        
                        # 发送基于时间的批量命令
                        if time_based_servos:
                            all_angles = self.current_positions.copy()
                            for servo_id, angle in time_based_servos.items():
                                all_angles[servo_id] = angle
                            
                            logger.info(f"  发送基于时间批量命令: {len(time_based_servos)}个舵机, 时间{speed_ms}ms")
                            if not serial_comm.move_all_servos(all_angles, speed_ms):
                                logger.error("发送运动命令失败")
                                return False
                        
                        # 发送梯形速度命令（逐个）
                        for servo_info in trapezoid_servos:
                            if not serial_comm.move_servo_trapezoid(
                                servo_info['servo_id'],
                                servo_info['angle'],
                                servo_info['velocity'],
                                servo_info['acceleration'],
                                servo_info['deceleration']
                            ):
                                logger.error(f"发送梯形速度命令失败: 舵机{servo_info['servo_id']}")
                                return False
                        
                        # 更新当前位置
                        for servo_id, data in servo_data.items():
                            if 0 <= servo_id < self.servo_count:
                                self.current_positions[servo_id] = data['angle']
                        
                        # 等待运动完成（取最长时间 + 余量）
                        wait_time = max_wait_time + 0.2
                        logger.debug(f"  等待运动完成: {wait_time:.2f}秒")
                        time.sleep(wait_time)
                        
                    elif cmd['type'] == 'delay':
                        # 延时命令
                        duration = cmd['duration']
                        logger.info(f"  延时: {duration}秒")
                        time.sleep(duration)
                
                # 如果是单次模式，执行完一遍就退出
                if not should_loop:
                    logger.info("命令序列执行完成（单次模式）")
                    break
                
                # 循环模式：继续下一轮
                logger.info(f"=== 第 {loop_count} 次循环完成，继续... ===")
            
            return True
            
        except Exception as e:
            error_msg = f"执行命令序列失败: {e}"
            logger.error(error_msg)
            return False
    
    def stop_execution(self):
        """停止命令序列执行"""
        self.should_stop = True
        logger.info("发送停止信号")
    
    def generate_preview_text(self, timeline_data: TimelineData) -> str:
        """生成命令预览文本（用于显示）"""
        sequence = self.generate_command_sequence(timeline_data)
        
        lines = []
        lines.append("; 18通道舵机时间轴控制程序 (舵机编号0-17)")
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
                servo_data = cmd.get('servo_data', {})
                for servo_id, data in servo_data.items():
                    angle = data['angle']
                    component = data['component']
                    motion_mode = component.parameters.get('motion_mode', 'time')
                    
                    if motion_mode == 'trapezoid':
                        velocity = component.parameters.get('velocity', 30.0)
                        acceleration = component.parameters.get('acceleration', 60.0)
                        lines.append(f";   舵机{servo_id}: {angle:.1f}° [梯形速度: v={velocity:.1f}°/s, a={acceleration:.1f}°/s²]")
                    else:
                        lines.append(f";   舵机{servo_id}: {angle:.1f}° [基于时间: {cmd['speed_ms']}ms]")
                
                lines.append(f"CMD: MOVE_MIXED")
                
            elif cmd['type'] == 'delay':
                lines.append(f"; 类型: 延时")
                lines.append(f"; 时长: {cmd['duration']:.2f}s")
                lines.append(f"CMD: DELAY {cmd['duration']:.2f}s")
            
            lines.append("")
        
        lines.append("; 程序结束")
        
        return '\n'.join(lines)


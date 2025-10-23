#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
G代码生成器
将时间轴数据转换为grblHAL可执行的G代码
基于时间轴的同步运动生成
"""

from typing import List, Dict, Any, Tuple
from models.timeline_data import TimelineData, MotorTrack, LoopMode
from models.component import Component, ComponentType
import logging

logger = logging.getLogger('motor_controller')

class GCodeGenerator:
    """G代码生成器 - 支持时间轴同步运动"""
    
    def __init__(self, mm_per_degree: float = 0.03556):
        self.axis_names = ['X', 'Y', 'Z', 'A', 'B', 'C', 'U', 'V']
        self.default_speed = 1000.0  # 默认速度（度/分钟）
        self.current_positions = [0.0] * 8  # 当前各轴位置（度）
        self.mm_per_degree = mm_per_degree  # 角度到mm的转换系数
    
    def set_conversion_factor(self, mm_per_degree: float):
        """设置角度到mm的转换系数"""
        self.mm_per_degree = mm_per_degree
        logger.info(f"设置转换系数: {mm_per_degree:.5f} mm/度")
        
    def generate_gcode(self, timeline_data: TimelineData) -> str:
        """生成完整的G代码"""
        logger.info("开始生成G代码")
        
        # 重置所有轴的当前位置为0 - 每次生成时重新开始
        self.current_positions = [0.0] * 8
        
        gcode_lines = []
        
        # 1. 初始化代码
        gcode_lines.extend(self._generate_initialization())
        
        # 2. 设置零点
        gcode_lines.extend(self._generate_home_position())
        
        # 3. 生成运动代码
        motion_gcode = self._generate_motion_commands(timeline_data)
        gcode_lines.extend(motion_gcode)
        
        # 4. 结束代码 - 检查是否有任何轨道设置为循环模式
        has_loop = any(track.loop_mode == LoopMode.LOOP for track in timeline_data.tracks)
        gcode_lines.extend(self._generate_ending(has_loop))
        
        gcode_text = '\n'.join(gcode_lines)
        logger.info(f"G代码生成完成，共{len(gcode_lines)}行")
        
        return gcode_text
    
    def _generate_initialization(self) -> List[str]:
        """生成初始化代码"""
        return [
            "; 8轴电机时间轴控制程序",
            "; 由MotorController上位机自动生成",
            "",
            "; 初始化: 公制单位",
            "G21",
            "; 选择XY平面",
            "G17",
            "; 切换到相对坐标模式(增量模式) - 所有移动都相对当前位置",
            "G91",
            ""
        ]
    
    def _generate_home_position(self) -> List[str]:
        """生成归零代码 - 不再使用G92,使用G91相对运动"""
        return [
            "; 使用G91相对运动模式,不需要设置零点",
            "; (所有移动都是相对于当前位置的增量)",
            ""
        ]
    
    def _generate_motion_commands(self, timeline_data: TimelineData) -> List[str]:
        """生成运动指令 - 基于时间轴的同步运动"""
        gcode_lines = []
        
        # 重置当前位置（累积位置）
        self.current_positions = [0.0] * 8
        
        # 收集所有事件(包括运动和延时)并按时间排序
        all_events = []
        for track in timeline_data.tracks:
            for component in track.components:
                # 处理运动部件
                if component.type in [ComponentType.FORWARD_ROTATION, ComponentType.REVERSE_ROTATION]:
                    all_events.append({
                        'component': component,
                        'track': track,
                        'motor_id': track.motor_id,
                        'start_time': component.start_time,
                        'end_time': component.start_time + component.duration,
                        'type': 'motion'
                    })
                # 处理延时部件 - 需要加入时间分组
                elif component.type == ComponentType.DELAY:
                    all_events.append({
                        'component': component,
                        'track': track,
                        'motor_id': track.motor_id,
                        'start_time': component.start_time,
                        'end_time': component.start_time + component.duration,
                        'type': 'delay'
                    })
        
        logger.info(f"找到 {len(all_events)} 个事件 (运动+延时)")
        
        # 按开始时间排序
        all_events.sort(key=lambda x: x['start_time'])
        
        # 按时间分组处理所有事件
        time_groups = {}
        for event in all_events:
            start_time = event['start_time']
            if start_time not in time_groups:
                time_groups[start_time] = []
            time_groups[start_time].append(event)
        
        logger.info(f"时间分组: {list(time_groups.keys())}")
        
        # 按时间顺序生成G代码
        for time in sorted(time_groups.keys()):
            events_at_time = time_groups[time]
            logger.info(f"处理时间 {time}s 的 {len(events_at_time)} 个事件")
            
            # 分离运动事件和延时事件
            motion_events = [e for e in events_at_time if e['type'] == 'motion']
            delay_events = [e for e in events_at_time if e['type'] == 'delay']
            
            # 生成同步运动指令
            if motion_events:
                motion_commands = self._generate_synchronized_motion(motion_events, time)
                gcode_lines.extend(motion_commands)
                logger.info(f"  生成 {len(motion_commands)} 行运动G代码")
            
            # 生成G4延时指令 (由grblHAL处理)
            if delay_events:
                for delay_event in delay_events:
                    component = delay_event['component']
                    delay_time = component.parameters.get('delay_time', 1.0)
                    # grblHAL的G4 P参数单位是秒,不是毫秒!
                    gcode_lines.append(f"; 延时 {delay_time}s")
                    gcode_lines.append(f"G4 P{delay_time}")
                    logger.info(f"  生成延时指令: G4 P{delay_time} ({delay_time}秒)")
        
        return gcode_lines
    
    def _generate_synchronized_motion(self, events: List[Dict[str, Any]], time: float) -> List[str]:
        """生成同步运动指令"""
        gcode_lines = []
        
        if not events:
            return gcode_lines
        
        # 收集所有需要运动的轴
        axis_movements = {}
        feed_rate = self.default_speed
        
        logger.info(f"    生成同步运动，{len(events)} 个事件")
        
        for event in events:
            component = event['component']
            motor_id = event['motor_id']
            axis_name = self.axis_names[motor_id]
            
            if component.type == ComponentType.FORWARD_ROTATION:
                target_angle = component.parameters.get('target_angle', 90.0)
                speed = component.parameters.get('speed', self.default_speed)
                
                # G91模式: 直接使用相对角度,不需要累积
                axis_movements[axis_name] = target_angle
                feed_rate = speed
                
                logger.info(f"    {axis_name}轴正转: +{target_angle}° (相对移动)")
                
            elif component.type == ComponentType.REVERSE_ROTATION:
                target_angle = component.parameters.get('target_angle', -90.0)
                speed = component.parameters.get('speed', self.default_speed)
                
                # G91模式: 直接使用相对角度,不需要累积
                axis_movements[axis_name] = target_angle
                feed_rate = speed
                
                logger.info(f"    {axis_name}轴反转: {target_angle}° (相对移动)")
        
        # 生成G1指令 (G91相对模式)
        if axis_movements:
            # 构建G1指令 - 将相对角度转换为mm
            g1_parts = ['G1']
            for axis, relative_angle_deg in axis_movements.items():
                # 相对角度转换为mm: mm = 度 × mm_per_degree
                relative_distance_mm = relative_angle_deg * self.mm_per_degree
                g1_parts.append(f"{axis}{relative_distance_mm:.3f}")
                logger.debug(f"    转换: {axis} {relative_angle_deg:+.2f}° -> {relative_distance_mm:+.3f}mm (相对)")
            
            # 速度也需要转换: mm/min = (度/min) × (mm/度)
            feed_rate_mm = feed_rate * self.mm_per_degree
            g1_parts.append(f"F{feed_rate_mm:.1f}")
            
            gcode_line = ' '.join(g1_parts)
            gcode_lines.append(gcode_line)
            
            # 添加注释
            axis_list = ', '.join([f"{axis}{pos:.1f}°" for axis, pos in axis_movements.items()])
            gcode_lines.append(f"; 时间{time}s: {axis_list} (速度{feed_rate}°/min)")
            gcode_lines.append("")  # 空行分隔
        
        return gcode_lines
    
    def _generate_ending(self, is_loop: bool = False) -> List[str]:
        """生成结束代码
        
        Args:
            is_loop: 是否循环模式 (grblHAL不支持M99,由上位机控制循环)
                True - 使用M30,上位机会自动重新发送
                False - 使用M30,程序结束
        """
        logger.info(f"生成{'循环' if is_loop else '单次'}模式结束代码 (M30)")
        return [
            "",
            f"; {'循环模式 - M30结束后由上位机自动重新发送' if is_loop else '单次模式 - 程序结束并复位'}",
            "M30",  # 程序结束并复位 (grblHAL不支持M99)
            ""
        ]
    
    def generate_for_track(self, track: MotorTrack) -> List[str]:
        """生成单个轨道的G代码（用于调试和测试）"""
        gcode_lines = []
        current_position = 0.0
        
        gcode_lines.append(f"; === {track.name} ===")
        
        for component in track.components:
            if component.type == ComponentType.FORWARD_ROTATION:
                target_angle = component.parameters.get('target_angle', 90.0)
                speed = component.parameters.get('speed', self.default_speed)
                current_position += target_angle
                gcode_lines.append(f"; 正转 {target_angle}°")
                gcode_lines.append(f"G1 {self.axis_names[track.motor_id]}{current_position:.2f} F{speed:.0f}")
                
            elif component.type == ComponentType.REVERSE_ROTATION:
                target_angle = component.parameters.get('target_angle', -90.0)
                speed = component.parameters.get('speed', self.default_speed)
                current_position += target_angle
                gcode_lines.append(f"; 反转 {target_angle}°")
                gcode_lines.append(f"G1 {self.axis_names[track.motor_id]}{current_position:.2f} F{speed:.0f}")
                
            elif component.type == ComponentType.HOME:
                speed = component.parameters.get('speed', self.default_speed)
                current_position = 0.0
                gcode_lines.append(f"; 归零")
                gcode_lines.append(f"G1 {self.axis_names[track.motor_id]}0 F{speed:.0f}")
                
            elif component.type == ComponentType.STOP:
                gcode_lines.append(f"; 停止 {self.axis_names[track.motor_id]}轴")
                
            elif component.type == ComponentType.DELAY:
                delay_time = component.parameters.get('delay_time', 1.0)
                gcode_lines.append(f"; 延时 {delay_time}s")
                gcode_lines.append(f"G4 P{int(delay_time * 1000)}")
        
        return gcode_lines
    
    def validate_gcode(self, gcode: str) -> bool:
        """验证G代码语法"""
        try:
            lines = gcode.split('\n')
            for line in lines:
                line = line.strip()
                if line and not line.startswith(';'):
                    # 基本语法检查
                    if line.startswith('G'):
                        parts = line.split()
                        if len(parts) < 2:
                            return False
            return True
        except:
            return False
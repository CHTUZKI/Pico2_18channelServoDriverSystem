#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
部件数据模型
支持18个舵机（编号0-17）
"""

from enum import Enum
from typing import Dict, Any, Optional
import uuid

class ComponentType(Enum):
    """部件类型枚举"""
    FORWARD_ROTATION = "正转"
    REVERSE_ROTATION = "反转"
    DELAY = "延时"
    HOME = "归零"
    STOP = "停止"

class Component:
    """部件基类"""
    
    def __init__(self, component_type: ComponentType, motor_id: int = 0):
        self.id = str(uuid.uuid4())
        self.type = component_type
        self.motor_id = motor_id  # 舵机ID (0-17)
        self.start_time = 0.0  # 起始时间（秒）
        self.duration = 1.0    # 持续时间（秒）
        self.parameters = {}   # 参数字典
        self.selected = False  # 是否被选中
        
    def get_end_time(self) -> float:
        """获取结束时间"""
        return self.start_time + self.duration
    
    def set_position(self, start_time: float, duration: float):
        """设置位置和时长"""
        self.start_time = start_time
        self.duration = duration
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        return {
            'id': self.id,
            'type': self.type.value,
            'motor_id': self.motor_id,
            'start_time': self.start_time,
            'duration': self.duration,
            'parameters': self.parameters
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'Component':
        """从字典创建实例"""
        component = cls(ComponentType(data['type']), data['motor_id'])
        component.id = data['id']
        component.start_time = data['start_time']
        component.duration = data['duration']
        component.parameters = data['parameters']
        return component

class ForwardRotationComponent(Component):
    """正转部件"""
    
    def __init__(self, motor_id: int = 0, target_angle: float = 90.0, duration: float = 1.0, speed_ms: int = 1000):
        super().__init__(ComponentType.FORWARD_ROTATION, motor_id)
        self.parameters = {
            'target_angle': target_angle,  # 目标角度（度，0-180）
            'speed_ms': speed_ms,  # 运动时间（毫秒）
            
            # 运动模式和梯形速度参数
            'motion_mode': 'time',     # 运动模式：'time'(基于时间) 或 'trapezoid'(梯形速度)
            'velocity': 30.0,          # 最大速度（度/秒）
            'acceleration': 60.0,      # 加速度（度/秒²）
            'deceleration': 0.0        # 减速度（度/秒²，0表示使用加速度值）
        }

class ReverseRotationComponent(Component):
    """反转部件"""
    
    def __init__(self, motor_id: int = 0, target_angle: float = 90.0, duration: float = 1.0, speed_ms: int = 1000):
        super().__init__(ComponentType.REVERSE_ROTATION, motor_id)
        self.parameters = {
            'target_angle': target_angle,  # 目标角度（度，0-180）
            'speed_ms': speed_ms,  # 运动时间（毫秒）
            
            # 运动模式和梯形速度参数
            'motion_mode': 'time',     # 运动模式：'time'(基于时间) 或 'trapezoid'(梯形速度)
            'velocity': 30.0,          # 最大速度（度/秒）
            'acceleration': 60.0,      # 加速度（度/秒²）
            'deceleration': 0.0        # 减速度（度/秒²，0表示使用加速度值）
        }

class DelayComponent(Component):
    """延时部件"""
    
    def __init__(self, motor_id: int = 0, delay_time: float = 1.0):
        super().__init__(ComponentType.DELAY, motor_id)
        self.parameters = {
            'delay_time': delay_time  # 延时时长（秒）
        }

class HomeComponent(Component):
    """归零部件"""
    
    def __init__(self, motor_id: int = 0, speed_ms: int = 1000):
        super().__init__(ComponentType.HOME, motor_id)
        self.parameters = {
            'speed_ms': speed_ms  # 运动时间（毫秒）
        }

class StopComponent(Component):
    """停止部件"""
    
    def __init__(self, motor_id: int = 0):
        super().__init__(ComponentType.STOP, motor_id)
        self.parameters = {}

def create_component(component_type: ComponentType, motor_id: int = 0, **kwargs) -> Component:
    """工厂函数：根据类型创建部件"""
    duration = kwargs.get('duration', 1.0)  # 获取持续时间，默认1秒
    speed_ms = kwargs.get('speed_ms', 1000)  # 获取运动时间（毫秒），默认1000ms
    
    if component_type == ComponentType.FORWARD_ROTATION:
        return ForwardRotationComponent(motor_id, kwargs.get('target_angle', 90.0), duration, speed_ms)
    elif component_type == ComponentType.REVERSE_ROTATION:
        return ReverseRotationComponent(motor_id, kwargs.get('target_angle', 90.0), duration, speed_ms)
    elif component_type == ComponentType.DELAY:
        return DelayComponent(motor_id, kwargs.get('delay_time', 1.0))
    elif component_type == ComponentType.HOME:
        return HomeComponent(motor_id, speed_ms)
    elif component_type == ComponentType.STOP:
        return StopComponent(motor_id)
    else:
        raise ValueError(f"未知的部件类型: {component_type}")


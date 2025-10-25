#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
时间轴数据模型
支持18个舵机（编号0-17）
"""

from typing import List, Dict, Any, Optional
from dataclasses import dataclass
from enum import Enum
from .component import Component

class LoopMode(Enum):
    """循环模式枚举"""
    SINGLE = "单次"
    LOOP = "循环"

@dataclass
class MotorTrack:
    """电机轨道数据"""
    motor_id: int
    name: str  # 电机名称 (X/Y/Z/A/B/C/U/V)
    loop_mode: LoopMode = LoopMode.SINGLE
    components: List[Component] = None
    
    def __post_init__(self):
        if self.components is None:
            self.components = []
    
    def add_component(self, component: Component):
        """添加部件到轨道"""
        component.motor_id = self.motor_id
        self.components.append(component)
    
    def remove_component(self, component_id: str) -> bool:
        """从轨道移除部件"""
        for i, comp in enumerate(self.components):
            if comp.id == component_id:
                del self.components[i]
                return True
        return False
    
    def get_component_at_time(self, time: float) -> Optional[Component]:
        """获取指定时间的部件"""
        for comp in self.components:
            if comp.start_time <= time < comp.get_end_time():
                return comp
        return None
    
    def get_components_in_range(self, start_time: float, end_time: float) -> List[Component]:
        """获取时间范围内的所有部件"""
        result = []
        for comp in self.components:
            if (comp.start_time < end_time and comp.get_end_time() > start_time):
                result.append(comp)
        return result
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        return {
            'motor_id': self.motor_id,
            'name': self.name,
            'loop_mode': self.loop_mode.value,
            'components': [comp.to_dict() for comp in self.components]
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'MotorTrack':
        """从字典创建实例"""
        track = cls(
            motor_id=data['motor_id'],
            name=data['name'],
            loop_mode=LoopMode(data['loop_mode'])
        )
        
        # 重新创建部件对象
        for comp_data in data['components']:
            from .component import create_component, ComponentType
            comp_type = ComponentType(comp_data['type'])
            component = create_component(comp_type, comp_data['motor_id'])
            component.id = comp_data['id']
            component.start_time = comp_data['start_time']
            component.duration = comp_data['duration']
            component.parameters = comp_data['parameters']
            track.components.append(component)
        
        return track

class TimelineData:
    """时间轴数据管理器"""
    
    def __init__(self):
        self.tracks: List[MotorTrack] = []
        self.time_scale = 1.0  # 时间缩放比例
        self.time_unit = "秒"   # 时间单位
        self.total_duration = 0.0  # 总时长
        self.selected_components: List[str] = []  # 选中的部件ID列表
        
        # 初始化18个舵机轨道
        self._initialize_tracks()
    
    def _initialize_tracks(self):
        """初始化18个舵机轨道（编号0-17）"""
        for i in range(18):
            track = MotorTrack(motor_id=i, name=f'舵机{i}')
            self.tracks.append(track)
    
    def get_track(self, motor_id: int) -> Optional[MotorTrack]:
        """获取指定电机轨道"""
        if 0 <= motor_id < len(self.tracks):
            return self.tracks[motor_id]
        return None
    
    def add_component(self, component: Component, track_index: int):
        """添加部件到指定轨道"""
        if 0 <= track_index < len(self.tracks):
            self.tracks[track_index].add_component(component)
            self._update_total_duration()
    
    def remove_component(self, component_id: str) -> bool:
        """移除指定部件"""
        for track in self.tracks:
            if track.remove_component(component_id):
                self._update_total_duration()
                return True
        return False
    
    def get_component(self, component_id: str) -> Optional[Component]:
        """获取指定ID的部件"""
        for track in self.tracks:
            for comp in track.components:
                if comp.id == component_id:
                    return comp
        return None
    
    def get_all_components(self) -> List[Component]:
        """获取所有部件"""
        all_components = []
        for track in self.tracks:
            all_components.extend(track.components)
        return all_components
    
    def get_components_at_time(self, time: float) -> List[Component]:
        """获取指定时间的所有部件"""
        components = []
        for track in self.tracks:
            comp = track.get_component_at_time(time)
            if comp:
                components.append(comp)
        return components
    
    def clear_track(self, track_index: int):
        """清空指定轨道"""
        if 0 <= track_index < len(self.tracks):
            self.tracks[track_index].components.clear()
            self._update_total_duration()
    
    def clear_all(self):
        """清空所有轨道"""
        for track in self.tracks:
            track.components.clear()
        self._update_total_duration()
    
    def _update_total_duration(self):
        """更新总时长"""
        max_duration = 0.0
        for track in self.tracks:
            for comp in track.components:
                end_time = comp.get_end_time()
                if end_time > max_duration:
                    max_duration = end_time
        self.total_duration = max_duration
    
    def get_timeline_events(self) -> List[Dict[str, Any]]:
        """获取时间轴事件列表（按时间排序）"""
        events = []
        
        for track in self.tracks:
            for comp in track.components:
                # 开始事件
                events.append({
                    'time': comp.start_time,
                    'type': 'start',
                    'component': comp,
                    'track': track
                })
                # 结束事件
                events.append({
                    'time': comp.get_end_time(),
                    'type': 'end',
                    'component': comp,
                    'track': track
                })
        
        # 按时间排序
        events.sort(key=lambda x: x['time'])
        return events
    
    def to_dict(self) -> Dict[str, Any]:
        """转换为字典"""
        return {
            'tracks': [track.to_dict() for track in self.tracks],
            'time_scale': self.time_scale,
            'time_unit': self.time_unit,
            'total_duration': self.total_duration
        }
    
    @classmethod
    def from_dict(cls, data: Dict[str, Any]) -> 'TimelineData':
        """从字典创建实例"""
        timeline = cls()
        timeline.time_scale = data.get('time_scale', 1.0)
        timeline.time_unit = data.get('time_unit', '秒')
        timeline.total_duration = data.get('total_duration', 0.0)
        
        # 重新创建轨道
        timeline.tracks.clear()
        for track_data in data['tracks']:
            track = MotorTrack.from_dict(track_data)
            timeline.tracks.append(track)
        
        return timeline


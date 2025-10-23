#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
时间轴组件
主要的时间轴可视化控件
"""

from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QHBoxLayout, QScrollArea, 
                             QLabel, QFrame, QSlider, QSpinBox, QComboBox)
from PyQt5.QtCore import Qt, pyqtSignal, QTimer, QRect
from PyQt5.QtGui import QPainter, QColor, QPen, QFont, QBrush
from ui.motor_track import MotorTrack
from models.timeline_data import TimelineData, LoopMode
from models.component import ComponentType, create_component
import logging

logger = logging.getLogger('motor_controller')

class TimeRuler(QWidget):
    """时间标尺控件"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.time_scale = 100.0  # 像素/秒
        self.time_offset = 0.0   # 时间偏移，0表示从0秒开始
        self.total_duration = 0.0
        self.current_time = 0.0
        self.time_unit = "秒"
        
        self.setFixedHeight(30)
        self.setStyleSheet("""
            QWidget {
                background-color: #f0f0f0;
                border-bottom: 1px solid #ccc;
            }
        """)
    
    def set_time_scale(self, scale: float):
        """设置时间缩放"""
        self.time_scale = scale
        self.time_offset = 0.0  # 确保时间偏移为0
        self.update()
    
    def resizeEvent(self, event):
        """重写resize事件，确保与轨道容器宽度同步"""
        super().resizeEvent(event)
        # 通知父组件更新轨道容器宽度
        if hasattr(self.parent(), 'sync_track_width'):
            self.parent().sync_track_width()
    
    def set_time_offset(self, offset: float):
        """设置时间偏移"""
        self.time_offset = offset
        self.update()
    
    def set_total_duration(self, duration: float):
        """设置总时长"""
        self.total_duration = duration
        self.update()
    
    def set_current_time(self, time: float):
        """设置当前时间"""
        self.current_time = time
        self.update()
    
    def paintEvent(self, event):
        """绘制事件"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        rect = self.rect()
        
        # 绘制背景
        painter.fillRect(rect, QColor("#f0f0f0"))
        
        # 绘制时间刻度
        self._draw_time_marks(painter, rect)
        
        # 绘制当前时间线
        self._draw_current_time_line(painter, rect)
    
    def _draw_time_marks(self, painter: QPainter, rect: QRect):
        """绘制时间刻度"""
        painter.setPen(QPen(QColor("#666"), 1))
        painter.setFont(QFont("Arial", 8))
        
        # 计算刻度间隔
        pixel_per_second = self.time_scale
        if pixel_per_second > 0:
            # 根据缩放级别调整刻度间隔
            if pixel_per_second > 200:
                interval = 0.1  # 0.1秒间隔
            elif pixel_per_second > 100:
                interval = 0.5  # 0.5秒间隔
            elif pixel_per_second > 50:
                interval = 1.0  # 1秒间隔
            elif pixel_per_second > 20:
                interval = 2.0  # 2秒间隔
            else:
                interval = 5.0  # 5秒间隔
            
            # 舵机标签宽度偏移（需要与轨道对齐）
            label_width = 80  # 舵机标签宽度
            track_start_x = label_width  # 轨道开始位置
            
            # 绘制刻度线
            start_time = 0.0  # 从0秒开始
            end_time = start_time + ((rect.width() - track_start_x) / pixel_per_second)
            
            time = start_time
            while time <= end_time:
                x = track_start_x + int(time * pixel_per_second)  # 从轨道开始位置计算
                if track_start_x <= x <= rect.width():
                    # 绘制刻度线
                    painter.drawLine(x, rect.height() - 15, x, rect.height())
                    
                    # 绘制时间标签
                    if time == 0:
                        time_text = f"{int(time)}{self.time_unit}"  # 0秒显示为整数
                        # 0秒标签放在轨道开始位置
                        text_rect = QRect(track_start_x, 0, 25, 15)
                    else:
                        time_text = f"{time:.1f}{self.time_unit}"
                        text_rect = QRect(x - 20, 0, 40, 15)
                    painter.drawText(text_rect, Qt.AlignLeft, time_text)  # 左对齐
                
                time += interval
    
    def _draw_current_time_line(self, painter: QPainter, rect: QRect):
        """绘制当前时间线"""
        if self.current_time >= 0:  # 从0秒开始
            # 舵机标签宽度偏移（需要与轨道对齐）
            label_width = 80  # 舵机标签宽度
            track_start_x = label_width  # 轨道开始位置
            x = track_start_x + int(self.current_time * self.time_scale)  # 从轨道开始位置计算
            if track_start_x <= x <= rect.width():
                painter.setPen(QPen(QColor("#FF0000"), 2))
                painter.drawLine(x, 0, x, rect.height())

class TimelineWidget(QWidget):
    """时间轴主控件"""
    
    # 信号定义
    component_dropped = pyqtSignal(ComponentType, int, float)  # 部件类型, 舵机ID, 时间
    component_selected = pyqtSignal(str)  # 部件ID
    component_moved = pyqtSignal(str, float, float)  # 部件ID, 新起始时间, 新持续时间
    component_deleted = pyqtSignal(str)  # 部件ID
    loop_mode_changed = pyqtSignal(int, LoopMode)  # 舵机ID, 循环模式
    time_changed = pyqtSignal(float)  # 当前时间改变
    servo_enable_clicked = pyqtSignal(int)  # 舵机使能点击信号，传递舵机ID
    
    def __init__(self, parent=None, config_manager=None):
        super().__init__(parent)
        self.timeline_data = TimelineData()
        self.motor_tracks = {}  # 舵机ID -> MotorTrack
        self.time_scale = 100.0  # 像素/秒
        self.time_offset = 0.0   # 时间偏移
        self.current_time = 0.0
        self.selected_component_id = ""
        self.is_move_mode = False  # 移动模式标志
        self.moving_component_id = ""  # 正在移动的部件ID
        self.config_manager = config_manager  # 配置管理器
        
        self.init_ui()
        self._create_motor_tracks()
    
    def init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout()
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)
        
        # 时间标尺
        self.time_ruler = TimeRuler()
        self.time_ruler.set_time_scale(self.time_scale)  # 设置初始缩放
        self.time_ruler.set_time_offset(0.0)  # 确保从0秒开始
        layout.addWidget(self.time_ruler)
        
        # 滚动区域
        self.scroll_area = QScrollArea()
        self.scroll_area.setWidgetResizable(True)
        self.scroll_area.setHorizontalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.scroll_area.setVerticalScrollBarPolicy(Qt.ScrollBarAsNeeded)
        self.scroll_area.setContentsMargins(0, 0, 0, 0)  # 确保没有边距
        
        # 轨道容器
        self.tracks_widget = QWidget()
        self.tracks_layout = QVBoxLayout()
        self.tracks_layout.setContentsMargins(0, 0, 0, 0)
        self.tracks_layout.setSpacing(3)  # 恢复合适的轨道间距
        self.tracks_widget.setLayout(self.tracks_layout)
        
        self.scroll_area.setWidget(self.tracks_widget)
        layout.addWidget(self.scroll_area)
        
        # 同步轨道宽度
        self.sync_track_width()
        
        # 控制面板
        self._create_control_panel(layout)
        
        self.setLayout(layout)
        self.setStyleSheet("""
            QWidget {
                background-color: white;
            }
        """)
    
    def sync_track_width(self):
        """同步轨道容器宽度与时间标尺"""
        if hasattr(self, 'time_ruler') and hasattr(self, 'tracks_widget'):
            # 确保轨道容器宽度与时间标尺一致
            self.tracks_widget.setMinimumWidth(self.time_ruler.width())
            self.tracks_widget.setMaximumWidth(self.time_ruler.width())
    
    def _create_control_panel(self, parent_layout):
        """创建控制面板"""
        control_frame = QFrame()
        control_frame.setFixedHeight(40)
        control_frame.setStyleSheet("""
            QFrame {
                background-color: #f5f5f5;
                border-top: 1px solid #ddd;
            }
        """)
        
        control_layout = QHBoxLayout()
        control_layout.setContentsMargins(10, 5, 10, 5)
        
        # 时间单位选择
        unit_label = QLabel("时间单位:")
        control_layout.addWidget(unit_label)
        
        self.unit_combo = QComboBox()
        self.unit_combo.addItems(["秒", "毫秒", "分钟"])
        self.unit_combo.currentTextChanged.connect(self._on_unit_changed)
        control_layout.addWidget(self.unit_combo)
        
        # 缩放控制
        scale_label = QLabel("缩放:")
        control_layout.addWidget(scale_label)
        
        self.scale_slider = QSlider(Qt.Horizontal)
        self.scale_slider.setRange(10, 500)
        self.scale_slider.setValue(100)
        self.scale_slider.valueChanged.connect(self._on_scale_changed)
        control_layout.addWidget(self.scale_slider)
        
        # 当前时间显示
        time_label = QLabel("当前时间:")
        control_layout.addWidget(time_label)
        
        self.time_spinbox = QSpinBox()
        self.time_spinbox.setRange(0, 999999)
        self.time_spinbox.setSuffix(" 秒")
        self.time_spinbox.valueChanged.connect(self._on_time_changed)
        control_layout.addWidget(self.time_spinbox)
        
        # 总时长显示
        duration_label = QLabel("总时长:")
        control_layout.addWidget(duration_label)
        
        self.duration_label = QLabel("0.0 秒")
        control_layout.addWidget(self.duration_label)
        
        control_layout.addStretch()
        control_frame.setLayout(control_layout)
        parent_layout.addWidget(control_frame)
    
    def _create_motor_tracks(self):
        """创建舵机轨道"""
        for track in self.timeline_data.tracks:
            motor_track = MotorTrack(track.motor_id, track.name)
            motor_track.component_dropped.connect(self.component_dropped.emit)
            motor_track.component_selected.connect(self._on_component_selected)
            motor_track.component_moved.connect(self._on_component_moved)
            motor_track.component_resized.connect(self._on_component_resized)
            motor_track.component_deleted.connect(self._on_component_deleted)
            motor_track.loop_mode_changed.connect(self._on_loop_mode_changed)
            motor_track.servo_enable_clicked.connect(self.servo_enable_clicked.emit)
            
            self.motor_tracks[track.motor_id] = motor_track
            self.tracks_layout.addWidget(motor_track)
    
    def set_timeline_data(self, timeline_data: TimelineData):
        """设置时间轴数据"""
        self.timeline_data = timeline_data
        self._update_all_tracks()
        self._update_duration_display()
    
    def _update_all_tracks(self):
        """更新所有轨道"""
        for track in self.timeline_data.tracks:
            if track.motor_id in self.motor_tracks:
                motor_track = self.motor_tracks[track.motor_id]
                
                # 同步时间缩放
                motor_track.set_time_scale(self.time_scale)
                
                # 清除现有部件
                for component_id in list(motor_track.components.keys()):
                    motor_track.remove_component(component_id)
                
                # 添加新部件
                for component in track.components:
                    motor_track.add_component(component)
                
                # 更新循环模式
                loop_mode = LoopMode.SINGLE if track.loop_mode == LoopMode.SINGLE else LoopMode.LOOP
                motor_track.loop_combo.setCurrentText(loop_mode.value)
    
    def _update_duration_display(self):
        """更新时长显示"""
        duration = self.timeline_data.total_duration
        self.duration_label.setText(f"{duration:.1f} {self.time_ruler.time_unit}")
        self.time_ruler.set_total_duration(duration)
    
    def add_component(self, component_type: ComponentType, motor_id: int, start_time: float):
        """添加部件"""
        # 从配置管理器获取默认值
        if self.config_manager:
            default_duration = self.config_manager.get_default_duration('rotation')
            default_forward_angle = self.config_manager.get_default_angle('forward')
            default_reverse_angle = self.config_manager.get_default_angle('reverse')
            default_speed = self.config_manager.get_default_speed('rotation')
        else:
            default_duration = 5.0
            default_forward_angle = 90.0
            default_reverse_angle = -90.0
            default_speed = 1000.0
        
        # 创建部件 - 使用配置的默认值
        if component_type == ComponentType.FORWARD_ROTATION:
            component = create_component(component_type, motor_id, duration=default_duration, target_angle=default_forward_angle, speed=default_speed)
        elif component_type == ComponentType.REVERSE_ROTATION:
            component = create_component(component_type, motor_id, duration=default_duration, target_angle=default_reverse_angle, speed=default_speed)
        else:
            component = create_component(component_type, motor_id, duration=default_duration)
        
        # 强制设置起始时间为0，让部件从最左侧开始
        component.start_time = 0.0
        
        # 确保部件不重叠
        component = self._resolve_component_overlap(component, motor_id)
        
        # 添加到时间轴数据
        logger.info(f"添加部件到TimelineData前: 舵机{motor_id}轨道有{len(self.timeline_data.get_track(motor_id).components)}个部件")
        self.timeline_data.add_component(component, motor_id)
        logger.info(f"添加部件到TimelineData后: 舵机{motor_id}轨道有{len(self.timeline_data.get_track(motor_id).components)}个部件")
        logger.info(f"添加部件到TimelineData: {component.type.value}, 舵机{motor_id}, 参数: {component.parameters}")
        
        # 添加到轨道显示
        if motor_id in self.motor_tracks:
            self.motor_tracks[motor_id].add_component(component)
        
        # 更新显示
        self._update_duration_display()
        
        # 调试：检查TimelineData中的部件数量
        track = self.timeline_data.get_track(motor_id)
        if track:
            logger.info(f"舵机{motor_id}轨道现在有{len(track.components)}个部件")
        
        logger.info(f"添加部件: {component_type.value} 到舵机{motor_id+1} 时间{component.start_time:.2f}s")
    
    def _resolve_component_overlap(self, new_component, motor_id):
        """解决部件重叠问题"""
        track = self.timeline_data.get_track(motor_id)
        if not track:
            return new_component
        
        # 获取该轨道的所有部件（排除新部件本身）
        existing_components = [comp for comp in track.components if comp.id != new_component.id]
        
        # 按时间排序
        existing_components.sort(key=lambda x: x.start_time)
        
        # 找到最后一个部件的位置，将新部件放在其后
        if existing_components:
            last_component = existing_components[-1]
            # 对齐到时间刻度的最小单位（1秒）
            new_start_time = last_component.get_end_time()
            new_component.start_time = round(new_start_time)  # 四舍五入到最近的整数秒
        else:
            # 如果没有现有部件，从时间0开始
            new_component.start_time = 0.0
        
        return new_component
    
    def _components_overlap(self, comp1, comp2):
        """检查两个部件是否重叠"""
        return not (comp1.get_end_time() <= comp2.start_time or 
                   comp1.start_time >= comp2.get_end_time())
    
    def remove_component(self, component_id: str):
        """移除部件"""
        # 从时间轴数据移除
        self.timeline_data.remove_component(component_id)
        
        # 从所有轨道移除
        for motor_track in self.motor_tracks.values():
            motor_track.remove_component(component_id)
        
        # 更新显示
        self._update_duration_display()
        
        logger.info(f"移除部件: {component_id}")
    
    def _on_component_selected(self, component_id: str):
        """部件被选中"""
        self.selected_component_id = component_id
        
        # 清除其他轨道的选择
        for motor_track in self.motor_tracks.values():
            motor_track.clear_selection()
        
        # 选中当前部件
        component = self.timeline_data.get_component(component_id)
        if component and component.motor_id in self.motor_tracks:
            self.motor_tracks[component.motor_id].select_component(component_id)
        
        self.component_selected.emit(component_id)
    
    def _on_component_moved(self, component_id: str, new_start_time: float, new_duration: float):
        """部件被移动"""
        component = self.timeline_data.get_component(component_id)
        if component:
            component.start_time = new_start_time
            component.duration = new_duration
            self._update_duration_display()
            self.component_moved.emit(component_id, new_start_time, new_duration)
    
    def _on_component_resized(self, component_id: str, start_time: float, duration: float):
        """部件调整大小"""
        component = self.timeline_data.get_component(component_id)
        if component:
            component.start_time = start_time
            component.duration = duration
            self._update_duration_display()
            self.component_resized.emit(component_id, start_time, duration)
    
    def _on_component_deleted(self, component_id: str):
        """部件被删除"""
        self.remove_component(component_id)
        self.component_deleted.emit(component_id)
    
    def _on_loop_mode_changed(self, motor_id: int, loop_mode: LoopMode):
        """循环模式改变"""
        track = self.timeline_data.get_track(motor_id)
        if track:
            track.loop_mode = loop_mode
        self.loop_mode_changed.emit(motor_id, loop_mode)
    
    def _on_unit_changed(self, unit: str):
        """时间单位改变"""
        self.time_ruler.time_unit = unit
        self.time_ruler.update()
    
    def _on_scale_changed(self, value: int):
        """缩放改变"""
        self.time_scale = value
        self.time_ruler.set_time_scale(self.time_scale)
        
        # 更新所有轨道
        for motor_track in self.motor_tracks.values():
            motor_track.set_time_scale(self.time_scale)
    
    def _on_time_changed(self, time: int):
        """当前时间改变"""
        self.current_time = time
        self.time_ruler.set_current_time(self.current_time)
        self.time_changed.emit(self.current_time)
    
    def get_selected_component(self):
        """获取选中的部件"""
        if self.selected_component_id:
            return self.timeline_data.get_component(self.selected_component_id)
        return None
    
    def clear_selection(self):
        """清除选择"""
        self.selected_component_id = ""
        for motor_track in self.motor_tracks.values():
            motor_track.clear_selection()
    
    def get_all_components(self):
        """获取所有部件"""
        return self.timeline_data.get_all_components()
    
    def delete_component(self, component_id: str):
        """删除部件"""
        # 从数据模型中删除
        component = self.timeline_data.get_component(component_id)
        if not component:
            return
        
        # 找到包含此部件的轨道
        for motor_id, motor_track in self.motor_tracks.items():
            if motor_track.remove_component(component_id):
                logger.info(f"从舵机{motor_id + 1}删除部件: {component.type.value}")
                break
        
        # 清除选择
        self.clear_selection()
    
    def move_component_to_track(self, component_id: str, target_motor_id: int, new_start_time: float):
        """移动部件到其他轨道"""
        # 获取部件
        component = self.timeline_data.get_component(component_id)
        if not component:
            logger.warning(f"找不到部件: {component_id}")
            return
        
        # 找到并删除原轨道中的部件
        source_motor_id = None
        for motor_id, motor_track in self.motor_tracks.items():
            if motor_track.has_component(component_id):
                source_motor_id = motor_id
                motor_track.remove_component(component_id)
                break
        
        if source_motor_id is None:
            logger.warning(f"找不到部件所在轨道: {component_id}")
            return
        
        # 更新部件的起始时间
        component.start_time = new_start_time
        
        # 添加到目标轨道
        target_track = self.motor_tracks.get(target_motor_id)
        if target_track:
            # 解决重叠问题
            component = self._resolve_component_overlap(component, target_motor_id)
            target_track.add_component_widget(component)
            logger.info(f"部件从舵机{source_motor_id + 1}移动到舵机{target_motor_id + 1}")
        else:
            logger.warning(f"目标轨道不存在: {target_motor_id}")
    
    def enter_move_mode(self, component_id: str):
        """进入移动模式"""
        self.is_move_mode = True
        self.moving_component_id = component_id
        
        # 为所有轨道启用移动模式
        for motor_track in self.motor_tracks.values():
            motor_track.set_move_mode(True, component_id)
        
        logger.info(f"进入移动模式: {component_id}")
    
    def exit_move_mode(self):
        """退出移动模式"""
        self.is_move_mode = False
        moving_id = self.moving_component_id
        self.moving_component_id = ""
        
        # 为所有轨道禁用移动模式
        for motor_track in self.motor_tracks.values():
            motor_track.set_move_mode(False, "")
        
        logger.info(f"退出移动模式: {moving_id}")
    
    def get_motion_table_data(self) -> dict:
        """获取运动逻辑表数据"""
        motion_data = {}
        
        for motor_id in range(18):
            track = self.timeline_data.get_track(motor_id)
            if track:
                # 获取轨道信息
                track_data = {
                    'name': track.name,
                    'loop_mode': track.loop_mode.value,
                    'components': []
                }
                
                # 获取所有部件信息
                for component in track.components:
                    comp_data = {
                        'type': component.type.value,
                        'start_time': component.start_time,
                        'duration': component.duration,
                        'parameters': component.parameters.copy()
                    }
                    track_data['components'].append(comp_data)
                
                # 按起始时间排序
                track_data['components'].sort(key=lambda x: x['start_time'])
                motion_data[motor_id] = track_data
            else:
                # 空轨道
                motion_data[motor_id] = {
                    'name': f'舵机{motor_id + 1}',
                    'loop_mode': '单次',
                    'components': []
                }
        
        return motion_data


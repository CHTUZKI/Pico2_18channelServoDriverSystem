#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
舵机轨道组件
时间轴中的单个舵机轨道（舵机编号0-17）
"""

from PyQt5.QtWidgets import (QWidget, QHBoxLayout, QLabel, QComboBox, 
                             QFrame, QMenu, QAction, QDialog, QMessageBox, QPushButton, QVBoxLayout)
from PyQt5.QtCore import Qt, pyqtSignal, QRect, QPoint, QMimeData
from PyQt5.QtGui import QPainter, QColor, QPen, QBrush, QFont, QDragEnterEvent, QDropEvent
from models.component import Component, ComponentType, create_component
from models.timeline_data import LoopMode
import logging

logger = logging.getLogger('servo_controller')

class ComponentWidget(QWidget):
    """部件可视化控件"""
    
    # 信号定义
    component_selected = pyqtSignal(str)  # 部件ID
    component_moved = pyqtSignal(str, float, float)  # 部件ID, 新起始时间, 新持续时间
    component_updated = pyqtSignal(str, object)  # 部件ID, 更新后的部件对象
    component_resize_start = pyqtSignal(str, float)  # 部件ID, 鼠标X位置
    component_resize_end = pyqtSignal(str, float)   # 部件ID, 鼠标X位置
    component_deleted = pyqtSignal(str)  # 部件ID - 删除信号
    
    def __init__(self, component: Component, parent=None):
        super().__init__(parent)
        self.component = component
        self.is_selected = False
        self.is_resizing = False
        self.is_move_mode = False  # 移动模式标志
        self.resize_handle = None  # 'left' 或 'right'
        self.drag_start_pos = QPoint()
        self.drag_start_time = 0.0
        self.drag_start_duration = 0.0
        
        self.setFixedHeight(45)
        self.setStyleSheet(self._get_style_sheet())
        
        # 设置工具提示
        self._update_tooltip()
    
    def _get_style_sheet(self) -> str:
        """获取样式表"""
        colors = {
            ComponentType.FORWARD_ROTATION: "#4CAF50",
            ComponentType.REVERSE_ROTATION: "#F44336", 
            ComponentType.DELAY: "#FF9800",
            ComponentType.HOME: "#2196F3",
            ComponentType.STOP: "#9C27B0"
        }
        
        color = colors.get(self.component.type, "#757575")
        border_color = self._darken_color(color) if self.is_selected else color
        
        return f"""
        QWidget {{
            background-color: {color};
            border: 2px solid {border_color};
            border-radius: 4px;
            color: white;
            font-weight: bold;
            font-size: 10px;
        }}
        """
    
    def _darken_color(self, color: str) -> str:
        """使颜色变暗"""
        color_map = {
            "#4CAF50": "#2E7D32",
            "#F44336": "#C62828",
            "#FF9800": "#E65100",
            "#2196F3": "#0D47A1", 
            "#9C27B0": "#4A148C"
        }
        return color_map.get(color, color)
    
    def _update_tooltip(self):
        """更新工具提示"""
        tooltip = f"{self.component.type.value}\n"
        tooltip += f"起始时间: {self.component.start_time:.2f}s\n"
        tooltip += f"持续时间: {self.component.duration:.2f}s\n"
        
        if self.component.type in [ComponentType.FORWARD_ROTATION, ComponentType.REVERSE_ROTATION]:
            angle = self.component.parameters.get('target_angle', 0)
            tooltip += f"目标角度: {angle:.1f}°"
        elif self.component.type == ComponentType.DELAY:
            delay = self.component.parameters.get('delay_time', 0)
            tooltip += f"延时时长: {delay:.2f}s"
        
        self.setToolTip(tooltip)
    
    def set_selected(self, selected: bool):
        """设置选中状态"""
        self.is_selected = selected
        self.setStyleSheet(self._get_style_sheet())
        self.update()
    
    def set_move_mode(self, enabled: bool):
        """设置移动模式"""
        self.is_move_mode = enabled
        if enabled:
            # 进入移动模式，改变光标和样式
            self.setCursor(Qt.SizeHorCursor)
            self.setStyleSheet(self._get_move_style_sheet())
        else:
            # 退出移动模式
            self.setCursor(Qt.ArrowCursor)
            self.setStyleSheet(self._get_style_sheet())
        self.update()
    
    def _get_move_style_sheet(self) -> str:
        """获取移动模式下的样式表"""
        return f"""
        QWidget {{
            background-color: #FF9800;
            border: 3px dashed #F57C00;
            border-radius: 3px;
            color: white;
        }}
        """
    
    def paintEvent(self, event):
        """绘制事件"""
        painter = QPainter(self)
        painter.setRenderHint(QPainter.Antialiasing)
        
        # 绘制背景
        rect = self.rect()
        painter.fillRect(rect, QColor(self._get_background_color()))
        
        # 绘制边框
        pen = QPen(QColor(self._get_border_color()), 2)
        painter.setPen(pen)
        painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), 4, 4)
        
        # 绘制文本
        painter.setPen(QColor("white"))
        painter.setFont(QFont("Arial", 9, QFont.Bold))
        painter.drawText(rect, Qt.AlignCenter, self.component.type.value)
        
        # 绘制调整手柄（如果被选中）
        if self.is_selected:
            self._draw_resize_handles(painter, rect)
    
    def _get_background_color(self) -> str:
        """获取背景颜色"""
        colors = {
            ComponentType.FORWARD_ROTATION: "#4CAF50",
            ComponentType.REVERSE_ROTATION: "#F44336",
            ComponentType.DELAY: "#FF9800", 
            ComponentType.HOME: "#2196F3",
            ComponentType.STOP: "#9C27B0"
        }
        return colors.get(self.component.type, "#757575")
    
    def _get_border_color(self) -> str:
        """获取边框颜色"""
        if self.is_selected:
            return "#FFD700"  # 金色边框表示选中
        return self._darken_color(self._get_background_color())
    
    def _draw_resize_handles(self, painter: QPainter, rect: QRect):
        """绘制调整手柄"""
        # 延时部件不显示调整手柄
        if self.component.type.value == "延时":
            return
            
        handle_width = 6
        handle_color = QColor("#FFD700")
        disabled_color = QColor("#CCCCCC")  # 禁用状态的颜色
        
        # 左调整手柄
        if self._can_resize_left():
            left_handle = QRect(rect.left(), rect.top(), handle_width, rect.height())
            painter.fillRect(left_handle, handle_color)
        else:
            left_handle = QRect(rect.left(), rect.top(), handle_width, rect.height())
            painter.fillRect(left_handle, disabled_color)
        
        # 右调整手柄
        if self._can_resize_right():
            right_handle = QRect(rect.right() - handle_width, rect.top(), handle_width, rect.height())
            painter.fillRect(right_handle, handle_color)
        else:
            right_handle = QRect(rect.right() - handle_width, rect.top(), handle_width, rect.height())
            painter.fillRect(right_handle, disabled_color)
    
    def mousePressEvent(self, event):
        """鼠标按下事件"""
        if event.button() == Qt.LeftButton:
            self.drag_start_pos = event.pos()
            self.drag_start_time = self.component.start_time
            self.drag_start_duration = self.component.duration
            self.resize_start_x = event.globalX()
            
            # 如果在移动模式，只支持拖动
            if self.is_move_mode:
                # 移动模式：只支持拖动，不选中，不调整大小
                pass
            # 延时部件不支持拖拽调整大小，只支持编辑参数
            elif self.component.type.value == "延时":
                # 延时部件只支持选中，不支持调整大小
                self.component_selected.emit(self.component.id)
            else:
                # 其他部件支持调整大小
                rect = self.rect()
                if rect.left() <= event.x() <= rect.left() + 6:
                    # 检查左侧是否可以调整
                    if self._can_resize_left():
                        self.is_resizing = True
                        self.resize_handle = 'left'
                elif rect.right() - 6 <= event.x() <= rect.right():
                    # 检查右侧是否可以调整
                    if self._can_resize_right():
                        self.is_resizing = True
                        self.resize_handle = 'right'
                else:
                    # 选中部件
                    self.component_selected.emit(self.component.id)
        
        super().mousePressEvent(event)
    
    def mouseMoveEvent(self, event):
        """鼠标移动事件"""
        # 移动模式：只支持拖动，吸附到时间刻度
        if self.is_move_mode and (event.buttons() & Qt.LeftButton):
            self._handle_move_mode_drag(event)
        # 延时部件不支持拖拽调整大小
        elif self.component.type.value == "延时":
            # 延时部件只支持移动，不支持调整大小
            if event.buttons() & Qt.LeftButton and not self.is_resizing:
                self._handle_drag(event)
            else:
                # 延时部件不显示调整手柄光标
                self.setCursor(Qt.ArrowCursor)
        else:
            # 其他部件支持调整大小
            if self.is_resizing and (event.buttons() & Qt.LeftButton):
                # 调整大小 - 实时更新
                self._handle_resize(event)
            elif event.buttons() & Qt.LeftButton and not self.is_resizing:
                # 移动部件
                self._handle_drag(event)
            else:
                # 更新鼠标光标
                self._update_cursor(event.pos())
        
        super().mouseMoveEvent(event)
    
    def _handle_resize(self, event):
        """处理调整大小"""
        if not hasattr(self, 'resize_handle') or not self.resize_handle:
            return
            
        # 计算调整的像素距离
        delta_x = event.globalX() - self.resize_start_x
        
        # 获取时间轴组件
        timeline_widget = self.parent().parent().parent()
        if not hasattr(timeline_widget, 'time_scale'):
            return
            
        delta_time = delta_x / timeline_widget.time_scale
        
        if self.resize_handle == 'left':
            # 左侧调整：改变起始时间和持续时间
            new_start_time = self.drag_start_time + delta_time
            new_duration = self.drag_start_duration - delta_time
            new_start_time = max(0, new_start_time)
            new_duration = max(0.1, new_duration)  # 最小0.1秒
            
            # 发送移动信号
            self.component_moved.emit(self.component.id, new_start_time, new_duration)
        elif self.resize_handle == 'right':
            # 右侧调整：只改变持续时间
            new_duration = self.drag_start_duration + delta_time
            new_duration = max(0.1, new_duration)  # 最小0.1秒
            
            # 发送调整大小信号
            self.component_resized.emit(self.component.id, self.drag_start_time, new_duration)
    
    def _update_cursor(self, pos):
        """更新鼠标光标"""
        rect = self.rect()
        if rect.left() <= pos.x() <= rect.left() + 6:
            # 左侧调整手柄
            if self._can_resize_left():
                self.setCursor(Qt.SizeHorCursor)
            else:
                self.setCursor(Qt.ForbiddenCursor)
        elif rect.right() - 6 <= pos.x() <= rect.right():
            # 右侧调整手柄
            if self._can_resize_right():
                self.setCursor(Qt.SizeHorCursor)
            else:
                self.setCursor(Qt.ForbiddenCursor)
        else:
            # 其他区域
            self.setCursor(Qt.ArrowCursor)
    
    def mouseReleaseEvent(self, event):
        """鼠标释放事件"""
        if event.button() == Qt.LeftButton:
            self.is_resizing = False
            self.resize_handle = None
        
        super().mouseReleaseEvent(event)
    
    def _can_resize_left(self):
        """检查左侧是否可以调整大小"""
        # 获取轨道组件
        track_widget = self.parent()
        if not hasattr(track_widget, 'components'):
            return False
        
        # 检查左侧是否有其他部件
        current_x = self.x()
        for comp_id, comp_widget in track_widget.components.items():
            if comp_widget == self:
                continue
            comp_x = comp_widget.x()
            comp_width = comp_widget.width()
            comp_end = comp_x + comp_width
            
            # 如果左侧有部件紧挨着，不能调整
            if comp_end >= current_x - 1:  # 1像素容差
                return False
        
        # 检查是否顶到轨道最左侧
        if current_x <= 2:  # 2像素容差
            return False
            
        return True
    
    def _can_resize_right(self):
        """检查右侧是否可以调整大小"""
        # 获取轨道组件
        track_widget = self.parent()
        if not hasattr(track_widget, 'components'):
            return True  # 如果没有轨道信息，默认可以调整
        
        # 检查右侧是否有其他部件
        current_x = self.x()
        current_width = self.width()
        current_end = current_x + current_width
        
        for comp_id, comp_widget in track_widget.components.items():
            if comp_widget == self:
                continue
            comp_x = comp_widget.x()
            
            # 如果右侧有部件紧挨着，不能调整
            if comp_x <= current_end + 1:  # 1像素容差
                return False
        
        return True
    
    def _handle_drag(self, event):
        """处理拖拽"""
        # 获取时间轴组件
        timeline_widget = self.parent().parent().parent()
        if not hasattr(timeline_widget, 'time_scale'):
            return
            
        # 计算新的起始时间
        delta_x = event.pos().x() - self.drag_start_pos.x()
        delta_time = delta_x / timeline_widget.time_scale
        
        new_start_time = self.drag_start_time + delta_time
        new_duration = self.drag_start_duration
        
        # 限制在有效范围内
        new_start_time = max(0, new_start_time)
        
        # 发送移动信号
        self.component_moved.emit(self.component.id, new_start_time, new_duration)
    
    def _handle_move_mode_drag(self, event):
        """处理移动模式下的拖拽，吸附到时间刻度，避免覆盖"""
        # 获取父轨道组件
        track_widget = self.parent()
        if not hasattr(track_widget, 'parent'):
            return
        
        motor_track = track_widget.parent()
        if not hasattr(motor_track, 'time_scale'):
            return
        
        # 计算新的起始时间
        delta_x = event.pos().x() - self.drag_start_pos.x()
        delta_time = delta_x / motor_track.time_scale
        
        new_start_time = self.drag_start_time + delta_time
        
        # 吸附到最近的时间刻度（整秒）
        new_start_time = round(new_start_time)
        
        # 限制在有效范围内
        new_start_time = max(0, new_start_time)
        
        # 检查是否与其他部件重叠
        new_start_time = self._find_valid_position(new_start_time, motor_track)
        
        # 更新部件位置（实时）
        self.component.start_time = new_start_time
        
        # 更新显示位置
        new_x = int(new_start_time * motor_track.time_scale)
        new_width = int(self.component.duration * motor_track.time_scale)
        self.setGeometry(new_x, self.y(), new_width, self.height())
    
    def _find_valid_position(self, target_time: float, motor_track) -> float:
        """查找有效位置，避免与其他部件重叠"""
        # 获取当前轨道的所有部件（除了自己）
        all_components = []
        for comp_id, widget in motor_track.components.items():
            if comp_id != self.component.id:
                all_components.append(widget.component)
        
        # 按起始时间排序
        all_components.sort(key=lambda x: x.start_time)
        
        # 检查目标位置是否与其他部件重叠
        target_end = target_time + self.component.duration
        
        for comp in all_components:
            comp_start = comp.start_time
            comp_end = comp.get_end_time()
            
            # 检查是否重叠
            if not (target_end <= comp_start or target_time >= comp_end):
                # 重叠了，调整位置
                if target_time < comp_start:
                    # 目标在部件之前，向前吸附
                    target_time = comp_start - self.component.duration
                    target_time = max(0, round(target_time))
                else:
                    # 目标在部件之后，向后吸附
                    target_time = comp_end
                    target_time = round(target_time)
                
                # 递归检查新位置
                return self._find_valid_position(target_time, motor_track)
        
        return target_time
    
    def contextMenuEvent(self, event):
        """右键菜单事件"""
        menu = QMenu(self)
        
        # 编辑参数
        edit_action = QAction("编辑参数", self)
        edit_action.triggered.connect(lambda: self._edit_parameters())
        menu.addAction(edit_action)
        
        # 删除部件
        delete_action = QAction("删除", self)
        delete_action.triggered.connect(lambda: self._delete_component())
        menu.addAction(delete_action)
        
        menu.exec_(event.globalPos())
    
    def _edit_parameters(self):
        """编辑参数"""
        # 发送编辑信号
        self.component_selected.emit(self.component.id)
        
        # 打开编辑对话框
        from ui.dialogs import ComponentEditDialog
        dialog = ComponentEditDialog(self.component, self)
        
        # 连接部件更新信号
        dialog.component_updated.connect(self._on_component_updated_from_dialog)
        
        if dialog.exec_() == QDialog.Accepted:
            # 对话框的accept()方法已经更新了component并发送了信号
            # 这里只需要更新显示
            self._update_tooltip()
    
    def _on_component_updated_from_dialog(self, component: Component):
        """处理从对话框更新的部件"""
        self.component = component
        self._update_tooltip()
        
        # 发送更新信号
        self.component_updated.emit(self.component.id, self.component)
    
    def _delete_component(self):
        """删除部件"""
        # 创建自定义样式的确认对话框
        from PyQt5.QtWidgets import QMessageBox
        msg_box = QMessageBox(self)
        msg_box.setWindowTitle("确认删除")
        msg_box.setText(f"确定要删除{self.component.type.value}部件吗？")
        msg_box.setIcon(QMessageBox.Question)
        msg_box.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        msg_box.setDefaultButton(QMessageBox.No)
        
        # 设置按钮文本为中文
        msg_box.button(QMessageBox.Yes).setText("是")
        msg_box.button(QMessageBox.No).setText("否")
        
        # 设置样式 - 白色背景,更大的字体和按钮
        msg_box.setStyleSheet("""
            QMessageBox {
                background-color: white;
                min-width: 350px;
                min-height: 150px;
            }
            QMessageBox QLabel {
                color: #333;
                font-size: 16px;
                padding: 20px;
                background-color: white;
            }
            QPushButton {
                background-color: #4CAF50;
                color: white;
                border: none;
                padding: 12px 30px;
                font-size: 15px;
                font-weight: bold;
                border-radius: 6px;
                min-width: 80px;
                min-height: 40px;
            }
            QPushButton:hover {
                background-color: #45a049;
            }
            QPushButton:pressed {
                background-color: #3d8b40;
            }
            QPushButton:default {
                background-color: #2196F3;
            }
            QPushButton:default:hover {
                background-color: #0b7dda;
            }
        """)
        
        reply = msg_box.exec_()
        if reply == QMessageBox.Yes:
            self.component_deleted.emit(self.component.id)

class MotorTrack(QWidget):
    """电机轨道控件"""
    
    # 信号定义
    component_dropped = pyqtSignal(ComponentType, int, float)  # 部件类型, 电机ID, 时间
    component_selected = pyqtSignal(str)  # 部件ID
    component_moved = pyqtSignal(str, float, float)  # 部件ID, 新起始时间, 新持续时间
    component_resized = pyqtSignal(str, float, float)  # 部件ID, 起始时间, 持续时间
    component_updated = pyqtSignal(str, object)  # 部件ID, 更新后的部件对象
    component_deleted = pyqtSignal(str)  # 部件ID
    loop_mode_changed = pyqtSignal(int, LoopMode)  # 电机ID, 循环模式
    servo_enable_clicked = pyqtSignal(int)  # 舵机使能点击信号，传递舵机ID
    jog_plus_clicked = pyqtSignal(int)  # Jog+点击信号，传递舵机ID
    jog_minus_clicked = pyqtSignal(int)  # Jog-点击信号，传递舵机ID
    
    def __init__(self, motor_id: int, motor_name: str, parent=None):
        super().__init__(parent)
        self.motor_id = motor_id
        self.motor_name = motor_name
        self.is_enabled = False  # 舵机使能状态
        self.components = {}  # 部件ID -> ComponentWidget
        self.time_scale = 100.0  # 像素/秒
        self.time_offset = 0.0   # 时间偏移，0表示从0秒开始
        self.is_move_mode = False  # 移动模式标志
        self.moving_component_id = ""  # 正在移动的部件ID
        
        self.init_ui()
        self.setAcceptDrops(True)
        self._update_enable_state()
    
    def init_ui(self):
        """初始化UI"""
        layout = QHBoxLayout()
        layout.setContentsMargins(0, 4, 4, 4)  # 恢复合适的边距
        layout.setSpacing(4)  # 恢复合适的间距
        
        # 电机标签（可点击使能）
        self.motor_label = QLabel(self.motor_name)
        self.motor_label.setFixedWidth(120)  # 恢复合适的宽度
        self.motor_label.setCursor(Qt.PointingHandCursor)  # 设置手型光标
        self.motor_label.mousePressEvent = self._on_motor_label_clicked
        self.motor_label.setStyleSheet("""
            QLabel {
                font-weight: bold;
                color: #333;
                padding: 4px;
                background-color: #e0e0e0;
                border: 1px solid #ccc;
                border-radius: 3px;
                margin-right: 2px;
                font-size: 12px;
            }
            QLabel:hover {
                background-color: #d0d0d0;
            }
        """)
        layout.addWidget(self.motor_label)
        
        # 轨道区域
        self.track_widget = QWidget()
        self.track_widget.setStyleSheet("""
            QWidget {
                background-color: #f9f9f9;
                border: 1px solid #ddd;
                border-radius: 3px;
                margin-left: 0px;
                padding-left: 0px;
            }
        """)
        # 设置轨道区域的最小宽度，确保有足够空间
        self.track_widget.setMinimumWidth(500)
        layout.addWidget(self.track_widget)
        
        # 右侧控制区域容器（固定宽度以避免遮挡）
        control_widget = QWidget()
        control_widget.setFixedWidth(100)  # 增加宽度确保完整显示
        control_layout = QVBoxLayout()
        control_layout.setContentsMargins(2, 0, 2, 0)  # 左右各留2px边距
        control_layout.setSpacing(2)
        
        # 循环模式选择
        self.loop_combo = QComboBox()
        self.loop_combo.addItem("单次", LoopMode.SINGLE)
        self.loop_combo.addItem("循环", LoopMode.LOOP)
        self.loop_combo.setFixedWidth(80)
        self.loop_combo.currentTextChanged.connect(self._on_loop_mode_changed)
        control_layout.addWidget(self.loop_combo)
        
        # Jog按钮水平布局
        jog_layout = QHBoxLayout()
        jog_layout.setContentsMargins(0, 0, 0, 0)
        jog_layout.setSpacing(4)
        jog_layout.setAlignment(Qt.AlignLeft)  # 左对齐
        
        # Jog- 按钮
        self.jog_minus_btn = QPushButton("◀")
        self.jog_minus_btn.setFixedSize(32, 24)
        self.jog_minus_btn.setToolTip("手动前进10°")
        self.jog_minus_btn.setStyleSheet("""
            QPushButton {
                background-color: #FF9800;
                color: white;
                border: none;
                border-radius: 3px;
                font-weight: bold;
                font-size: 14px;
            }
            QPushButton:hover {
                background-color: #F57C00;
            }
            QPushButton:pressed {
                background-color: #E65100;
            }
            QPushButton:disabled {
                background-color: #BDBDBD;
                color: #757575;
            }
        """)
        self.jog_minus_btn.clicked.connect(self._on_jog_minus_clicked)
        self.jog_minus_btn.setEnabled(False)  # 初始禁用
        jog_layout.addWidget(self.jog_minus_btn)
        
        # Jog+ 按钮
        self.jog_plus_btn = QPushButton("▶")
        self.jog_plus_btn.setFixedSize(32, 24)
        self.jog_plus_btn.setToolTip("手动后退10°")
        self.jog_plus_btn.setStyleSheet("""
            QPushButton {
                background-color: #4CAF50;
                color: white;
                border: none;
                border-radius: 3px;
                font-weight: bold;
                font-size: 14px;
            }
            QPushButton:hover {
                background-color: #45a049;
            }
            QPushButton:pressed {
                background-color: #388E3C;
            }
            QPushButton:disabled {
                background-color: #BDBDBD;
                color: #757575;
            }
        """)
        self.jog_plus_btn.clicked.connect(self._on_jog_plus_clicked)
        self.jog_plus_btn.setEnabled(False)  # 初始禁用
        jog_layout.addWidget(self.jog_plus_btn)
        
        jog_layout.addStretch()  # 添加弹性空间，将按钮推到左侧
        control_layout.addLayout(jog_layout)
        control_widget.setLayout(control_layout)
        layout.addWidget(control_widget)
        
        self.setLayout(layout)
        
        # 设置轨道的最小高度，确保运动部件有足够空间
        self.setMinimumHeight(60)
    
    def _on_motor_label_clicked(self, event):
        """舵机标签点击事件"""
        if event.button() == Qt.LeftButton:
            # 发送舵机使能点击信号
            self.servo_enable_clicked.emit(self.motor_id)
    
    def _on_jog_plus_clicked(self):
        """Jog+按钮点击事件"""
        self.jog_plus_clicked.emit(self.motor_id)
    
    def _on_jog_minus_clicked(self):
        """Jog-按钮点击事件"""
        self.jog_minus_clicked.emit(self.motor_id)
    
    def _update_enable_state(self):
        """更新使能状态显示"""
        if self.is_enabled:
            # 使能状态：绿色
            self.motor_label.setStyleSheet("""
                QLabel {
                    font-weight: bold;
                    color: white;
                    padding: 4px;
                    background-color: #4CAF50;
                    border: 1px solid #45a049;
                    border-radius: 3px;
                    margin-right: 2px;
                    font-size: 12px;
                }
                QLabel:hover {
                    background-color: #45a049;
                }
            """)
            # 启用jog按钮
            self.jog_plus_btn.setEnabled(True)
            self.jog_minus_btn.setEnabled(True)
        else:
            # 禁用状态：灰色
            self.motor_label.setStyleSheet("""
                QLabel {
                    font-weight: bold;
                    color: #333;
                    padding: 4px;
                    background-color: #e0e0e0;
                    border: 1px solid #ccc;
                    border-radius: 3px;
                    margin-right: 2px;
                    font-size: 12px;
                }
                QLabel:hover {
                    background-color: #d0d0d0;
                }
            """)
            # 禁用jog按钮
            self.jog_plus_btn.setEnabled(False)
            self.jog_minus_btn.setEnabled(False)
    
    def set_enable_state(self, enabled: bool):
        """设置使能状态"""
        self.is_enabled = enabled
        self._update_enable_state()
    
    def _on_loop_mode_changed(self, text: str):
        """循环模式改变"""
        loop_mode = LoopMode.SINGLE if text == "单次" else LoopMode.LOOP
        self.loop_mode_changed.emit(self.motor_id, loop_mode)
    
    def add_component(self, component: Component):
        """添加部件到轨道"""
        component_widget = ComponentWidget(component, self.track_widget)
        component_widget.component_selected.connect(self.component_selected.emit)
        component_widget.component_moved.connect(self.component_moved.emit)
        component_widget.component_updated.connect(self._on_component_updated)
        component_widget.component_resize_start.connect(self._on_component_resize_start)
        component_widget.component_resize_end.connect(self._on_component_resize_end)
        component_widget.component_deleted.connect(self.component_deleted.emit)
        
        self.components[component.id] = component_widget
        
        # 直接设置部件位置，确保顶到最左侧
        self._place_component_at_start(component_widget)
        self._update_component_tooltips()
    
    def _place_component_at_start(self, widget):
        """将部件放置在轨道开始位置"""
        component = widget.component
        width = max(int(component.duration * self.time_scale), 20)
        
        # 计算基于时间的x位置（0秒对应轨道左侧）
        x = int(component.start_time * self.time_scale)
        x = max(0, x)  # 确保不超出左边界
        
        # 找到最后一个部件的位置，将新部件放在其后
        last_x = 0
        for comp_id, comp_widget in self.components.items():
            if comp_widget == widget:
                continue
            comp = comp_widget.component
            comp_x = int(comp.start_time * self.time_scale)
            comp_width = max(int(comp.duration * self.time_scale), 20)
            comp_end = comp_x + comp_width
            last_x = max(last_x, comp_end)
        
        # 如果新部件的时间位置与现有部件重叠，放在最后
        if x <= last_x:
            x = last_x + 1  # 1像素间距
        
        widget.setGeometry(x, 8, width, 30)  # 稍微下移，避免与标签重叠
        widget.show()
    
    def remove_component(self, component_id: str):
        """从轨道移除部件"""
        if component_id in self.components:
            self.components[component_id].deleteLater()
            del self.components[component_id]
    
    def update_component(self, component: Component):
        """更新部件"""
        if component.id in self.components:
            self.components[component.id].component = component
            self.components[component.id]._update_tooltip()
            self._update_component_positions()
    
    def _update_component_positions(self):
        """更新部件位置"""
        for component_id, widget in self.components.items():
            component = widget.component
            x = int(component.start_time * self.time_scale)
            width = int(component.duration * self.time_scale)
            
            # 确保部件至少20像素宽
            width = max(width, 20)
            
            # 确保x坐标不为负数，且从轨道最左侧开始（完全顶到左边）
            x = max(0, x)  # 完全顶到左边，不留边距
            
            # 检查是否与现有部件重叠，如果重叠则自动调整位置
            x = self._find_non_overlapping_position(x, width, component_id)
            
            widget.setGeometry(x, 8, width, 30)  # 稍微下移，避免与标签重叠
            widget.show()
    
    def _find_non_overlapping_position(self, preferred_x, width, exclude_id):
        """找到不重叠的位置"""
        # 获取所有其他部件的位置
        other_components = [(comp_id, comp_widget) for comp_id, comp_widget in self.components.items() 
                           if comp_id != exclude_id]
        
        # 按起始时间排序（使用Component对象，不是ComponentWidget）
        other_components.sort(key=lambda x: x[1].component.start_time)
        
        # 检查首选位置是否可用
        if self._is_position_available(preferred_x, width, other_components):
            return preferred_x
        
        # 寻找下一个可用位置 - 紧挨着放置
        for comp_id, comp_widget in other_components:
            comp = comp_widget.component
            comp_x = int(comp.start_time * self.time_scale)
            comp_width = max(int(comp.duration * self.time_scale), 20)
            comp_end = comp_x + comp_width
            
            # 尝试在现有部件之后紧挨着放置（1像素间距）
            new_x = comp_end + 1
            if self._is_position_available(new_x, width, other_components):
                return new_x
        
        # 如果都重叠，就放在首选位置（用户需要手动调整）
        return preferred_x
    
    def _is_position_available(self, x, width, other_components):
        """检查位置是否可用"""
        end_x = x + width
        
        for comp_id, comp_widget in other_components:
            comp = comp_widget.component
            comp_x = int(comp.start_time * self.time_scale)
            comp_width = max(int(comp.duration * self.time_scale), 20)
            comp_end = comp_x + comp_width
            
            # 检查是否重叠
            if not (end_x <= comp_x or x >= comp_end):
                return False
        
        return True
    
    def _update_component_tooltips(self):
        """更新部件工具提示"""
        for widget in self.components.values():
            widget._update_tooltip()
    
    def _on_component_resize_start(self, component_id: str, global_x: int):
        """部件开始调整大小"""
        if component_id in self.components:
            widget = self.components[component_id]
            widget.resize_start_x = global_x
            widget.resize_start_duration = widget.component.duration
    
    def _on_component_resize_end(self, component_id: str, global_x: int):
        """部件结束调整大小"""
        if component_id in self.components:
            widget = self.components[component_id]
            if hasattr(widget, 'resize_start_x') and hasattr(widget, 'resize_handle'):
                # 计算调整的像素距离
                delta_x = global_x - widget.resize_start_x
                
                # 获取时间轴组件
                timeline_widget = self.parent().parent()
                if hasattr(timeline_widget, 'time_scale'):
                    delta_time = delta_x / timeline_widget.time_scale
                    
                    if widget.resize_handle == 'left':
                        # 左侧调整：改变起始时间和持续时间
                        new_start_time = widget.component.start_time + delta_time
                        new_duration = widget.component.duration - delta_time
                        new_start_time = max(0, round(new_start_time))  # 对齐到整数秒
                        new_duration = max(1.0, round(new_duration))  # 最小1秒，对齐到整数秒
                        
                        # 发送移动信号
                        self.component_moved.emit(component_id, new_start_time, new_duration)
                    elif widget.resize_handle == 'right':
                        # 右侧调整：只改变持续时间
                        new_duration = widget.component.duration + delta_time
                        new_duration = max(1.0, round(new_duration))  # 最小1秒，对齐到整数秒
                        
                        # 发送调整大小信号
                        self.component_resized.emit(component_id, widget.component.start_time, new_duration)
    
    def _on_component_updated(self, component_id: str, updated_component: Component):
        """部件参数更新"""
        if component_id in self.components:
            # 更新部件数据
            self.components[component_id].component = updated_component
            self.components[component_id]._update_tooltip()
            
            # 发送更新信号到时间轴组件
            self.component_updated.emit(component_id, updated_component)
    
    def _on_duration_changed(self, component_id: str, new_duration: float):
        """处理部件持续时间变化，推动后续部件"""
        if component_id not in self.components:
            return
        
        # 获取当前部件
        current_component = self.components[component_id].component
        old_duration = current_component.duration
        duration_delta = new_duration - old_duration
        
        if abs(duration_delta) < 0.01:  # 变化太小，忽略
            return
        
        # 更新当前部件的持续时间
        current_component.duration = new_duration
        
        # 获取所有部件，按起始时间排序
        all_components = [comp.component for comp in self.components.values()]
        all_components.sort(key=lambda x: x.start_time)
        
        # 找到当前部件在列表中的位置
        current_index = -1
        for i, comp in enumerate(all_components):
            if comp.id == component_id:
                current_index = i
                break
        
        if current_index == -1:
            return
        
        # 重新计算当前部件的起始时间，确保不与前面的部件重叠
        if current_index > 0:
            prev_component = all_components[current_index - 1]
            old_start_time = current_component.start_time
            current_component.start_time = round(prev_component.get_end_time())
        else:
            current_component.start_time = 0.0
        
        # 推动后续部件，对齐到时间刻度的最小单位（1秒）
        for i in range(current_index + 1, len(all_components)):
            old_start_time = all_components[i].start_time
            new_start_time = all_components[i].start_time + duration_delta
            all_components[i].start_time = round(new_start_time)  # 四舍五入到最近的整数秒
        
        # 更新所有部件的位置
        self._update_component_positions()
        
        # 更新所有被推动的部件的显示
        for i in range(current_index + 1, len(all_components)):
            comp = all_components[i]
            if comp.id in self.components:
                self.components[comp.id].component = comp
                self.components[comp.id]._update_tooltip()
        
        # 更新当前部件的显示
        if component_id in self.components:
            self.components[component_id].component = current_component
            self.components[component_id]._update_tooltip()
        
        # 发送更新信号
        self.component_updated.emit(component_id, current_component)
    
    def set_time_scale(self, scale: float):
        """设置时间缩放"""
        self.time_scale = scale
        self._update_component_positions()
    
    def set_time_offset(self, offset: float):
        """设置时间偏移"""
        self.time_offset = offset
        self._update_component_positions()
    
    def dragEnterEvent(self, event: QDragEnterEvent):
        """拖拽进入事件"""
        if event.mimeData().hasFormat("application/x-component-type"):
            event.acceptProposedAction()
        else:
            event.ignore()
    
    def dropEvent(self, event: QDropEvent):
        """拖拽放下事件"""
        if event.mimeData().hasFormat("application/x-component-type"):
            # 获取部件类型
            from ui.component_palette import ComponentPalette
            palette = ComponentPalette()
            component_type = palette.get_component_type_from_mime_data(event.mimeData())
            
            # 计算时间位置
            drop_x = event.pos().x()
            drop_time = (drop_x / self.time_scale) + self.time_offset
            
            # 发送信号
            self.component_dropped.emit(component_type, self.motor_id, drop_time)
            event.acceptProposedAction()
        else:
            event.ignore()
    
    def get_component_at_position(self, x: int) -> str:
        """获取指定位置的部件ID"""
        for component_id, widget in self.components.items():
            if widget.geometry().contains(x, widget.geometry().center().y()):
                return component_id
        return ""
    
    def select_component(self, component_id: str):
        """选中指定部件"""
        for comp_id, widget in self.components.items():
            widget.set_selected(comp_id == component_id)
    
    def clear_selection(self):
        """清除选择"""
        for widget in self.components.values():
            widget.set_selected(False)
    
    def has_component(self, component_id: str) -> bool:
        """检查是否包含指定部件"""
        return component_id in self.components
    
    def set_move_mode(self, enabled: bool, component_id: str):
        """设置移动模式"""
        self.is_move_mode = enabled
        self.moving_component_id = component_id
        
        if enabled and component_id in self.components:
            # 高亮正在移动的部件
            widget = self.components[component_id]
            widget.set_move_mode(True)
        elif not enabled:
            # 退出移动模式，恢复所有部件
            for widget in self.components.values():
                widget.set_move_mode(False)

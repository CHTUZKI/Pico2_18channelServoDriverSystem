#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
部件面板
左侧可拖拽的部件选择面板
"""

from PyQt5.QtWidgets import (QWidget, QVBoxLayout, QLabel, QFrame, 
                             QScrollArea, QPushButton, QGroupBox)
from PyQt5.QtCore import Qt, pyqtSignal, QMimeData
from PyQt5.QtGui import QDrag
from PyQt5.QtGui import QPainter, QColor, QFont, QDragEnterEvent, QDropEvent
from models.component import ComponentType, create_component
import logging

logger = logging.getLogger('motor_controller')

class ComponentButton(QPushButton):
    """可拖拽的部件按钮"""
    
    def __init__(self, component_type: ComponentType, parent=None):
        super().__init__(parent)
        self.component_type = component_type
        self.setText(component_type.value)
        self.setFixedSize(80, 40)
        self.setStyleSheet(self._get_style_sheet())
        
    def _get_style_sheet(self) -> str:
        """获取样式表"""
        colors = {
            ComponentType.FORWARD_ROTATION: "#4CAF50",  # 绿色
            ComponentType.REVERSE_ROTATION: "#F44336",  # 红色
            ComponentType.DELAY: "#FF9800",             # 橙色
            ComponentType.HOME: "#2196F3",              # 蓝色
            ComponentType.STOP: "#9C27B0"               # 紫色
        }
        
        color = colors.get(self.component_type, "#757575")
        
        return f"""
        QPushButton {{
            background-color: {color};
            color: white;
            border: 2px solid {color};
            border-radius: 8px;
            font-weight: bold;
            font-size: 12px;
        }}
        QPushButton:hover {{
            background-color: {self._lighten_color(color)};
            border-color: {self._lighten_color(color)};
        }}
        QPushButton:pressed {{
            background-color: {self._darken_color(color)};
            border-color: {self._darken_color(color)};
        }}
        """
    
    def _lighten_color(self, color: str) -> str:
        """使颜色变亮"""
        # 简单的颜色变亮逻辑
        color_map = {
            "#4CAF50": "#66BB6A",
            "#F44336": "#EF5350", 
            "#FF9800": "#FFB74D",
            "#2196F3": "#42A5F5",
            "#9C27B0": "#BA68C8"
        }
        return color_map.get(color, color)
    
    def _darken_color(self, color: str) -> str:
        """使颜色变暗"""
        # 简单的颜色变暗逻辑
        color_map = {
            "#4CAF50": "#388E3C",
            "#F44336": "#D32F2F",
            "#FF9800": "#F57C00", 
            "#2196F3": "#1976D2",
            "#9C27B0": "#7B1FA2"
        }
        return color_map.get(color, color)
    
    def mousePressEvent(self, event):
        """鼠标按下事件"""
        if event.button() == Qt.LeftButton:
            self.drag_start_position = event.pos()
        super().mousePressEvent(event)
    
    def mouseMoveEvent(self, event):
        """鼠标移动事件"""
        if not (event.buttons() & Qt.LeftButton):
            return
        
        # 设置拖拽阈值（像素）
        drag_threshold = 10
        if ((event.pos() - self.drag_start_position).manhattanLength() < drag_threshold):
            return
        
        # 开始拖拽
        self._start_drag()
    
    def _start_drag(self):
        """开始拖拽操作"""
        try:
            # 创建拖拽数据
            mime_data = QMimeData()
            mime_data.setText(self.component_type.value)
            mime_data.setData("application/x-component-type", 
                            self.component_type.value.encode())
            
            # 创建拖拽对象
            drag = QDrag(self)
            drag.setMimeData(mime_data)
            
            # 设置拖拽图标
            drag.setPixmap(self.grab())
            drag.setHotSpot(self.drag_start_position)
            
            # 执行拖拽
            drop_action = drag.exec_(Qt.CopyAction)
            
            logger.debug(f"部件拖拽完成: {self.component_type.value}")
            
        except Exception as e:
            logger.error(f"拖拽操作失败: {e}")

class ComponentPalette(QWidget):
    """部件面板主控件"""
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.init_ui()
    
    def init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout()
        layout.setContentsMargins(5, 5, 5, 5)
        layout.setSpacing(10)
        
        # 标题
        title_label = QLabel("部件面板")
        title_label.setAlignment(Qt.AlignCenter)
        title_label.setStyleSheet("""
            QLabel {
                font-size: 14px;
                font-weight: bold;
                color: #333;
                padding: 5px;
            }
        """)
        layout.addWidget(title_label)
        
        # 分隔线
        line = QFrame()
        line.setFrameShape(QFrame.HLine)
        line.setFrameShadow(QFrame.Sunken)
        layout.addWidget(line)
        
        # 创建部件按钮组
        self._create_component_groups(layout)
        
        # 添加弹性空间
        layout.addStretch()
        
        self.setLayout(layout)
        self.setFixedWidth(120)
        self.setStyleSheet("""
            QWidget {
                background-color: #f5f5f5;
                border-right: 1px solid #ddd;
            }
        """)
    
    def _create_component_groups(self, layout):
        """创建部件按钮组"""
        # 运动部件组
        motion_group = QGroupBox("运动部件")
        motion_layout = QVBoxLayout()
        motion_layout.setSpacing(5)
        
        # 正转按钮
        forward_btn = ComponentButton(ComponentType.FORWARD_ROTATION)
        forward_btn.setToolTip("正转部件\n拖拽到时间轴使用")
        motion_layout.addWidget(forward_btn)
        
        # 反转按钮
        reverse_btn = ComponentButton(ComponentType.REVERSE_ROTATION)
        reverse_btn.setToolTip("反转部件\n拖拽到时间轴使用")
        motion_layout.addWidget(reverse_btn)
        
        # 归零按钮
        home_btn = ComponentButton(ComponentType.HOME)
        home_btn.setToolTip("归零部件\n将电机归零到初始位置")
        motion_layout.addWidget(home_btn)
        
        # 停止按钮
        stop_btn = ComponentButton(ComponentType.STOP)
        stop_btn.setToolTip("停止部件\n停止电机运动")
        motion_layout.addWidget(stop_btn)
        
        motion_group.setLayout(motion_layout)
        motion_group.setStyleSheet("""
            QGroupBox {
                font-weight: bold;
                border: 1px solid #ccc;
                border-radius: 5px;
                margin-top: 10px;
                padding-top: 10px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
            }
        """)
        layout.addWidget(motion_group)
        
        # 控制部件组
        control_group = QGroupBox("控制部件")
        control_layout = QVBoxLayout()
        control_layout.setSpacing(5)
        
        # 延时按钮
        delay_btn = ComponentButton(ComponentType.DELAY)
        delay_btn.setToolTip("延时部件\n可拖拽调整时长")
        control_layout.addWidget(delay_btn)
        
        control_group.setLayout(control_layout)
        control_group.setStyleSheet("""
            QGroupBox {
                font-weight: bold;
                border: 1px solid #ccc;
                border-radius: 5px;
                margin-top: 10px;
                padding-top: 10px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
            }
        """)
        layout.addWidget(control_group)
    
    def get_component_type_from_mime_data(self, mime_data: QMimeData) -> ComponentType:
        """从MIME数据获取部件类型"""
        try:
            if mime_data.hasFormat("application/x-component-type"):
                data = mime_data.data("application/x-component-type").data().decode()
                return ComponentType(data)
            elif mime_data.hasText():
                text = mime_data.text()
                for comp_type in ComponentType:
                    if comp_type.value == text:
                        return comp_type
        except Exception as e:
            logger.error(f"解析部件类型失败: {e}")
        
        return ComponentType.FORWARD_ROTATION  # 默认类型


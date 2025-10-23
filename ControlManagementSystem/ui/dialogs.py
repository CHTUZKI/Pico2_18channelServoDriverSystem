#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
对话框组件
包含各种设置和编辑对话框
"""

from PyQt5.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QFormLayout,
                             QLabel, QPushButton, QSpinBox, QDoubleSpinBox, 
                             QComboBox, QGroupBox, QTabWidget, QWidget,
                             QMessageBox, QLineEdit, QTextEdit, QCheckBox)
from PyQt5.QtCore import Qt, pyqtSignal
from PyQt5.QtGui import QFont
from models.component import Component, ComponentType
import logging

logger = logging.getLogger('servo_controller')

class ComponentEditDialog(QDialog):
    """部件编辑对话框"""
    
    component_updated = pyqtSignal(object)  # 部件更新信号
    
    def __init__(self, component: Component, parent=None):
        super().__init__(parent)
        self.component = component
        self.init_ui()
    
    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle(f"编辑{self.component.type.value}")
        self.resize(400, 300)
        
        layout = QVBoxLayout()
        
        # 舵机选择
        motor_layout = QFormLayout()
        self.motor_label = QLabel(f"舵机 {self.component.motor_id + 1}")
        motor_layout.addRow("舵机:", self.motor_label)
        
        layout.addLayout(motor_layout)
        
        # 根据部件类型创建不同的控件
        if self.component.type in [ComponentType.FORWARD_ROTATION, ComponentType.REVERSE_ROTATION]:
            self._create_rotation_controls(layout)
        elif self.component.type == ComponentType.HOME:
            self._create_home_controls(layout)
        elif self.component.type == ComponentType.DELAY:
            self._create_delay_controls(layout)
        
        # 按钮
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        
        ok_btn = QPushButton("确定")
        ok_btn.clicked.connect(self.accept)
        cancel_btn = QPushButton("取消")
        cancel_btn.clicked.connect(self.reject)
        
        button_layout.addWidget(ok_btn)
        button_layout.addWidget(cancel_btn)
        layout.addLayout(button_layout)
        
        self.setLayout(layout)
    
    def _create_rotation_controls(self, layout):
        """创建旋转控件"""
        group = QGroupBox("运动参数")
        form_layout = QFormLayout()
        
        # 目标角度
        self.angle_spin = QDoubleSpinBox()
        self.angle_spin.setRange(-180.0, 180.0)
        self.angle_spin.setValue(self.component.parameters.get('target_angle', 90.0))
        self.angle_spin.setDecimals(1)
        self.angle_spin.setSuffix("°")
        form_layout.addRow("目标角度:", self.angle_spin)
        
        # 运动时间
        self.speed_ms_spin = QSpinBox()
        self.speed_ms_spin.setRange(100, 10000)
        self.speed_ms_spin.setValue(self.component.parameters.get('speed_ms', 1000))
        self.speed_ms_spin.setSuffix(" ms")
        form_layout.addRow("运动时间:", self.speed_ms_spin)
        
        group.setLayout(form_layout)
        layout.addWidget(group)
    
    def _create_home_controls(self, layout):
        """创建归零控件"""
        group = QGroupBox("归零参数")
        form_layout = QFormLayout()
        
        # 归零角度
        self.home_angle_spin = QDoubleSpinBox()
        self.home_angle_spin.setRange(0.0, 180.0)
        self.home_angle_spin.setValue(self.component.parameters.get('home_angle', 90.0))
        self.home_angle_spin.setDecimals(1)
        self.home_angle_spin.setSuffix("°")
        form_layout.addRow("归零角度:", self.home_angle_spin)
        
        # 运动时间
        self.speed_ms_spin = QSpinBox()
        self.speed_ms_spin.setRange(100, 10000)
        self.speed_ms_spin.setValue(self.component.parameters.get('speed_ms', 1000))
        self.speed_ms_spin.setSuffix(" ms")
        form_layout.addRow("运动时间:", self.speed_ms_spin)
        
        group.setLayout(form_layout)
        layout.addWidget(group)
    
    def _create_delay_controls(self, layout):
        """创建延时控件"""
        group = QGroupBox("延时参数")
        form_layout = QFormLayout()
        
        # 延时时间
        self.delay_spin = QDoubleSpinBox()
        self.delay_spin.setRange(0.1, 60.0)
        self.delay_spin.setValue(self.component.parameters.get('delay_time', 1.0))
        self.delay_spin.setDecimals(1)
        self.delay_spin.setSuffix(" s")
        form_layout.addRow("延时时间:", self.delay_spin)
        
        group.setLayout(form_layout)
        layout.addWidget(group)
    
    def accept(self):
        """确定按钮"""
        # 更新部件参数
        if self.component.type in [ComponentType.FORWARD_ROTATION, ComponentType.REVERSE_ROTATION]:
            self.component.parameters['target_angle'] = self.angle_spin.value()
            self.component.parameters['speed_ms'] = self.speed_ms_spin.value()
        elif self.component.type == ComponentType.HOME:
            self.component.parameters['home_angle'] = self.home_angle_spin.value()
            self.component.parameters['speed_ms'] = self.speed_ms_spin.value()
        elif self.component.type == ComponentType.DELAY:
            self.component.parameters['delay_time'] = self.delay_spin.value()
        
        self.component_updated.emit(self.component)
        super().accept()
            

class SerialSettingsDialog(QDialog):
    """串口设置对话框"""
    
    settings_changed = pyqtSignal(dict)
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.init_ui()
    
    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle("串口设置")
        self.resize(400, 200)
        
        layout = QVBoxLayout()
        
        # 串口参数
        form_layout = QFormLayout()
        
        # 波特率
        self.baudrate_combo = QComboBox()
        self.baudrate_combo.addItems(['9600', '19200', '38400', '57600', '115200', '230400'])
        self.baudrate_combo.setCurrentText('115200')
        form_layout.addRow("波特率:", self.baudrate_combo)
        
        # 超时时间
        self.timeout_spin = QDoubleSpinBox()
        self.timeout_spin.setRange(0.1, 10.0)
        self.timeout_spin.setValue(1.0)
        self.timeout_spin.setDecimals(1)
        self.timeout_spin.setSuffix(" s")
        form_layout.addRow("超时时间:", self.timeout_spin)
        
        layout.addLayout(form_layout)
        
        # 按钮
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        
        ok_btn = QPushButton("确定")
        ok_btn.clicked.connect(self.accept)
        cancel_btn = QPushButton("取消")
        cancel_btn.clicked.connect(self.reject)
        
        button_layout.addWidget(ok_btn)
        button_layout.addWidget(cancel_btn)
        layout.addLayout(button_layout)
        
        self.setLayout(layout)
    
    def get_settings(self) -> dict:
        """获取设置"""
        return {
            'baudrate': int(self.baudrate_combo.currentText()),
            'timeout': self.timeout_spin.value()
        }
    
    def set_settings(self, settings: dict):
        """设置参数"""
        if 'baudrate' in settings:
            self.baudrate_combo.setCurrentText(str(settings['baudrate']))
        if 'timeout' in settings:
            self.timeout_spin.setValue(settings['timeout'])
    
    def accept(self):
        """确定按钮"""
        settings = self.get_settings()
        self.settings_changed.emit(settings)
        super().accept()


class ServoSettingsDialog(QDialog):
    """舵机参数设置对话框"""
    
    settings_changed = pyqtSignal(dict)
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.init_ui()
    
    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle("舵机参数设置")
        self.resize(500, 300)
        
        # 应用白色背景样式
        self.setStyleSheet("""
            QDialog {
                background-color: white;
            }
            QGroupBox {
                font-weight: bold;
                border: 2px solid #cccccc;
                border-radius: 5px;
                margin-top: 1ex;
                padding-top: 10px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
            }
        """)
        
        layout = QVBoxLayout()
        
        # 说明标签
        title_label = QLabel("舵机参数设置")
        title_label.setStyleSheet("font-size: 16px; font-weight: bold; color: #333; margin-bottom: 10px;")
        layout.addWidget(title_label)
        
        info_label = QLabel(
            "设置舵机控制的基本参数，包括角度范围、运动时间等。\n"
            "这些参数将影响舵机运动的默认行为。"
        )
        info_label.setStyleSheet("color: #666; font-size: 12px; margin: 5px 0;")
        info_label.setWordWrap(True)
        layout.addWidget(info_label)
        
        # 舵机参数组
        servo_group = QGroupBox("舵机参数")
        servo_layout = QFormLayout()
        
        # 舵机数量
        self.servo_count_spin = QSpinBox()
        self.servo_count_spin.setRange(1, 32)
        self.servo_count_spin.setValue(18)
        self.servo_count_spin.setSuffix(" 个")
        servo_layout.addRow("舵机数量:", self.servo_count_spin)
        
        # 角度范围
        self.angle_min_spin = QDoubleSpinBox()
        self.angle_min_spin.setRange(0.0, 180.0)
        self.angle_min_spin.setValue(0.0)
        self.angle_min_spin.setDecimals(1)
        self.angle_min_spin.setSuffix("°")
        servo_layout.addRow("最小角度:", self.angle_min_spin)
        
        self.angle_max_spin = QDoubleSpinBox()
        self.angle_max_spin.setRange(0.0, 180.0)
        self.angle_max_spin.setValue(180.0)
        self.angle_max_spin.setDecimals(1)
        self.angle_max_spin.setSuffix("°")
        servo_layout.addRow("最大角度:", self.angle_max_spin)
        
        # 默认运动时间
        self.default_speed_ms_spin = QSpinBox()
        self.default_speed_ms_spin.setRange(100, 10000)
        self.default_speed_ms_spin.setValue(1000)
        self.default_speed_ms_spin.setSuffix(" ms")
        servo_layout.addRow("默认运动时间:", self.default_speed_ms_spin)
        
        servo_group.setLayout(servo_layout)
        layout.addWidget(servo_group)
        
        # 按钮
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        
        ok_btn = QPushButton("确定")
        ok_btn.clicked.connect(self.accept)
        ok_btn.setStyleSheet("""
            QPushButton {
                background-color: #4CAF50;
                color: white;
                border: none;
                padding: 8px 16px;
                border-radius: 4px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #45a049;
            }
        """)
        
        cancel_btn = QPushButton("取消")
        cancel_btn.clicked.connect(self.reject)
        cancel_btn.setStyleSheet("""
            QPushButton {
                background-color: #f44336;
                color: white;
                border: none;
                padding: 8px 16px;
                border-radius: 4px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #da190b;
            }
        """)
        
        button_layout.addWidget(ok_btn)
        button_layout.addWidget(cancel_btn)
        layout.addLayout(button_layout)
        
        self.setLayout(layout)
        
    def set_settings(self, settings: dict):
        """设置参数值"""
        if 'servo_count' in settings:
            self.servo_count_spin.setValue(settings['servo_count'])
        if 'angle_min' in settings:
            self.angle_min_spin.setValue(settings['angle_min'])
        if 'angle_max' in settings:
            self.angle_max_spin.setValue(settings['angle_max'])
        if 'default_speed_ms' in settings:
            self.default_speed_ms_spin.setValue(settings['default_speed_ms'])
    
    def get_settings(self) -> dict:
        """获取参数值"""
        return {
            'servo_count': self.servo_count_spin.value(),
            'angle_min': self.angle_min_spin.value(),
            'angle_max': self.angle_max_spin.value(),
            'default_speed_ms': self.default_speed_ms_spin.value()
        }
    
    def accept(self):
        """确定按钮"""
        settings = self.get_settings()
        self.settings_changed.emit(settings)
        super().accept()


class MoveComponentDialog(QDialog):
    """移动部件对话框"""
    
    def __init__(self, component: Component, parent=None):
        super().__init__(parent)
        self.component = component
        self.init_ui()
    
    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle("移动部件")
        self.resize(300, 150)
        
        layout = QVBoxLayout()
        
        # 目标舵机
        form_layout = QFormLayout()
        self.motor_combo = QComboBox()
        for i in range(18):
            self.motor_combo.addItem(f"舵机 {i + 1}")
        self.motor_combo.setCurrentIndex(self.component.motor_id)
        form_layout.addRow("目标舵机:", self.motor_combo)
        
        # 起始时间
        self.start_time_spin = QDoubleSpinBox()
        self.start_time_spin.setRange(0.0, 3600.0)
        self.start_time_spin.setValue(self.component.start_time)
        self.start_time_spin.setDecimals(2)
        self.start_time_spin.setSuffix(" s")
        form_layout.addRow("起始时间:", self.start_time_spin)
        
        layout.addLayout(form_layout)
        
        # 按钮
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        
        ok_btn = QPushButton("确定")
        ok_btn.clicked.connect(self.accept)
        cancel_btn = QPushButton("取消")
        cancel_btn.clicked.connect(self.reject)
        
        button_layout.addWidget(ok_btn)
        button_layout.addWidget(cancel_btn)
        layout.addLayout(button_layout)
        
        self.setLayout(layout)
    
    def get_motor_id(self) -> int:
        """获取目标舵机ID"""
        return self.motor_combo.currentIndex()
    
    def get_start_time(self) -> float:
        """获取起始时间"""
        return self.start_time_spin.value()


class MotionTableDialog(QDialog):
    """舵机运动表对话框"""
    
    def __init__(self, motion_data: dict, parent=None):
        super().__init__(parent)
        self.motion_data = motion_data
        self.init_ui()
    
    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle("舵机运动表")
        self.resize(800, 600)
        
        layout = QVBoxLayout()
        
        # 运动表内容
        self.text_edit = QTextEdit()
        self.text_edit.setReadOnly(True)
        self.text_edit.setPlainText(self._format_motion_table())
        layout.addWidget(self.text_edit)
        
        # 按钮
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        
        close_btn = QPushButton("关闭")
        close_btn.clicked.connect(self.accept)
        button_layout.addWidget(close_btn)
        
        layout.addLayout(button_layout)
        self.setLayout(layout)
    
    def _format_motion_table(self) -> str:
        """格式化运动表"""
        text = "舵机运动逻辑表\n"
        text += "=" * 50 + "\n\n"
        
        # 自动检测舵机数量（根据motion_data的键）
        servo_count = max(self.motion_data.keys()) + 1 if self.motion_data else 18
        
        for motor_id in range(servo_count):
            track_data = self.motion_data.get(motor_id, {})
            motor_name = track_data.get('name', f'舵机{motor_id + 1}')
            components = track_data.get('components', [])
            
            text += f"【{motor_name}】\n"
            if components:
                for comp in components:
                    start_time = comp.get('start_time', 0)
                    comp_type = comp.get('type', '')
                    params = comp.get('parameters', {})
                    
                    if 'target_angle' in params:
                        angle = params['target_angle']
                        speed_ms = params.get('speed_ms', 1000)
                        text += f"  {start_time:.2f}s: {comp_type} 角度{angle}° 运动时间{speed_ms}ms\n"
                    elif 'delay_time' in params:
                        delay = params['delay_time']
                        text += f"  {start_time:.2f}s: 延时 {delay}s\n"
            else:
                text += "  无运动\n"
            text += "\n"
        
        return text


class DefaultParametersDialog(QDialog):
    """默认参数配置对话框"""
    
    def __init__(self, config_manager, parent=None):
        super().__init__(parent)
        self.config_manager = config_manager
        self.init_ui()
    
    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle("默认参数配置")
        self.resize(600, 400)
        
        layout = QVBoxLayout()
        
        # 说明标签
        title_label = QLabel("默认参数配置")
        title_label.setStyleSheet("font-size: 16px; font-weight: bold; color: #333; margin-bottom: 10px;")
        layout.addWidget(title_label)
        
        info_label = QLabel(
            "配置系统的默认参数，这些参数将影响新创建的项目和部件。"
        )
        info_label.setStyleSheet("color: #666; font-size: 12px; margin: 5px 0;")
        info_label.setWordWrap(True)
        layout.addWidget(info_label)
        
        # 标签页
        self.tab_widget = QTabWidget()
        
        # 舵机参数标签页
        self.servo_tab = self._create_servo_tab()
        self.tab_widget.addTab(self.servo_tab, "舵机参数")
        
        # 运动参数标签页
        self.motion_tab = self._create_motion_tab()
        self.tab_widget.addTab(self.motion_tab, "运动参数")
        
        layout.addWidget(self.tab_widget)
        
        # 按钮
        button_layout = QHBoxLayout()
        button_layout.addStretch()
        
        ok_btn = QPushButton("确定")
        ok_btn.clicked.connect(self.accept)
        ok_btn.setStyleSheet("""
            QPushButton {
                background-color: #4CAF50;
                color: white;
                border: none;
                padding: 8px 16px;
                border-radius: 4px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #45a049;
            }
        """)
        
        cancel_btn = QPushButton("取消")
        cancel_btn.clicked.connect(self.reject)
        cancel_btn.setStyleSheet("""
            QPushButton {
                background-color: #f44336;
                color: white;
                border: none;
                padding: 8px 16px;
                border-radius: 4px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #da190b;
            }
        """)
        
        button_layout.addWidget(ok_btn)
        button_layout.addWidget(cancel_btn)
        layout.addLayout(button_layout)
        
        self.setLayout(layout)
    
        # 加载当前配置
        self._load_config()
    
    def _create_servo_tab(self) -> QWidget:
        """创建舵机参数标签页"""
        widget = QWidget()
        layout = QFormLayout()
        layout.setSpacing(15)
        
        # 舵机数量
        self.servo_count_spin = QSpinBox()
        self.servo_count_spin.setRange(1, 32)
        self.servo_count_spin.setValue(18)
        self.servo_count_spin.setSuffix(" 个")
        layout.addRow("舵机数量:", self.servo_count_spin)
        
        # 角度范围
        self.angle_min_spin = QDoubleSpinBox()
        self.angle_min_spin.setRange(0.0, 180.0)
        self.angle_min_spin.setValue(0.0)
        self.angle_min_spin.setDecimals(1)
        self.angle_min_spin.setSuffix("°")
        layout.addRow("最小角度:", self.angle_min_spin)
        
        self.angle_max_spin = QDoubleSpinBox()
        self.angle_max_spin.setRange(0.0, 180.0)
        self.angle_max_spin.setValue(180.0)
        self.angle_max_spin.setDecimals(1)
        self.angle_max_spin.setSuffix("°")
        layout.addRow("最大角度:", self.angle_max_spin)
        
        # 默认运动时间
        self.default_speed_ms_spin = QSpinBox()
        self.default_speed_ms_spin.setRange(100, 10000)
        self.default_speed_ms_spin.setValue(1000)
        self.default_speed_ms_spin.setSuffix(" ms")
        layout.addRow("默认运动时间:", self.default_speed_ms_spin)
        
        widget.setLayout(layout)
        return widget
    
    def _create_motion_tab(self) -> QWidget:
        """创建运动参数标签页"""
        widget = QWidget()
        layout = QFormLayout()
        layout.setSpacing(15)
        
        # 默认角度
        self.forward_angle_spin = QDoubleSpinBox()
        self.forward_angle_spin.setRange(-180.0, 180.0)
        self.forward_angle_spin.setValue(90.0)
        self.forward_angle_spin.setDecimals(1)
        self.forward_angle_spin.setSuffix("°")
        layout.addRow("正转默认角度:", self.forward_angle_spin)
        
        self.reverse_angle_spin = QDoubleSpinBox()
        self.reverse_angle_spin.setRange(-180.0, 180.0)
        self.reverse_angle_spin.setValue(-90.0)
        self.reverse_angle_spin.setDecimals(1)
        self.reverse_angle_spin.setSuffix("°")
        layout.addRow("反转默认角度:", self.reverse_angle_spin)
        
        self.home_angle_spin = QDoubleSpinBox()
        self.home_angle_spin.setRange(0.0, 180.0)
        self.home_angle_spin.setValue(90.0)
        self.home_angle_spin.setDecimals(1)
        self.home_angle_spin.setSuffix("°")
        layout.addRow("归零默认角度:", self.home_angle_spin)
        
        # 默认持续时间
        self.rotation_duration_spin = QDoubleSpinBox()
        self.rotation_duration_spin.setRange(0.1, 60.0)
        self.rotation_duration_spin.setValue(5.0)
        self.rotation_duration_spin.setDecimals(1)
        self.rotation_duration_spin.setSuffix(" s")
        layout.addRow("旋转默认持续时间:", self.rotation_duration_spin)
        
        self.delay_duration_spin = QDoubleSpinBox()
        self.delay_duration_spin.setRange(0.1, 60.0)
        self.delay_duration_spin.setValue(1.0)
        self.delay_duration_spin.setDecimals(1)
        self.delay_duration_spin.setSuffix(" s")
        layout.addRow("延时默认持续时间:", self.delay_duration_spin)
        
        widget.setLayout(layout)
        return widget
    
    def _load_config(self):
        """加载当前配置"""
        config = self.config_manager.config
        
        # 舵机参数
        servo_config = config.get('servo', {})
        self.servo_count_spin.setValue(servo_config.get('servo_count', 18))
        self.angle_min_spin.setValue(servo_config.get('angle_min', 0.0))
        self.angle_max_spin.setValue(servo_config.get('angle_max', 180.0))
        self.default_speed_ms_spin.setValue(servo_config.get('default_speed_ms', 1000))
        
        # 运动参数
        motion_config = config.get('default_motion', {})
        self.forward_angle_spin.setValue(motion_config.get('forward_angle', 90.0))
        self.reverse_angle_spin.setValue(motion_config.get('reverse_angle', -90.0))
        self.home_angle_spin.setValue(motion_config.get('home_angle', 90.0))
        
        duration_config = config.get('default_duration', {})
        self.rotation_duration_spin.setValue(duration_config.get('rotation', 5.0))
        self.delay_duration_spin.setValue(duration_config.get('delay', 1.0))
    
    def accept(self):
        """确定按钮"""
        # 保存配置
        config = self.config_manager.config
        
        # 舵机参数
        config['servo'] = {
            'servo_count': self.servo_count_spin.value(),
            'angle_min': self.angle_min_spin.value(),
            'angle_max': self.angle_max_spin.value(),
            'default_speed_ms': self.default_speed_ms_spin.value()
        }
        
        # 运动参数
        config['default_motion'] = {
            'forward_angle': self.forward_angle_spin.value(),
            'reverse_angle': self.reverse_angle_spin.value(),
            'home_angle': self.home_angle_spin.value()
        }
        
        # 持续时间参数
        config['default_duration'] = {
                'rotation': self.rotation_duration_spin.value(),
                'delay': self.delay_duration_spin.value()
            }
            
        # 保存配置
        self.config_manager.save()
        
        super().accept()

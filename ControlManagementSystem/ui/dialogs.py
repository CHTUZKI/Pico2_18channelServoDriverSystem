#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
对话框组件
各种参数设置和配置对话框
"""

from PyQt5.QtWidgets import (QDialog, QVBoxLayout, QHBoxLayout, QFormLayout,
                             QLabel, QLineEdit, QSpinBox, QDoubleSpinBox, 
                             QComboBox, QPushButton, QGroupBox, QCheckBox,
                             QTextEdit, QTabWidget, QWidget, QMessageBox, QFileDialog,
                             QTableWidget, QTableWidgetItem, QHeaderView)
from PyQt5.QtCore import Qt, pyqtSignal
from PyQt5.QtGui import QFont
from models.component import Component, ComponentType
from datetime import datetime
import serial.tools.list_ports
import logging

logger = logging.getLogger('motor_controller')

class ComponentEditDialog(QDialog):
    """部件参数编辑对话框"""
    
    # 信号定义
    component_updated = pyqtSignal(Component)  # 部件更新信号
    duration_changed = pyqtSignal(str, float)  # 部件ID, 新持续时间
    
    def __init__(self, component: Component, parent=None):
        super().__init__(parent)
        self.component = component
        self.init_ui()
        # 创建参数控件
        self._create_parameter_controls()
        # 在创建所有控件之后加载数据
        self.start_time_spin.setValue(self.component.start_time)
        self.duration_spin.setValue(self.component.duration)
    
    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle(f"编辑{self.component.type.value}部件")
        self.setModal(True)
        self.resize(500, 400)  # 增大窗口尺寸
        
        # 设置白色背景和更大字体
        self.setStyleSheet("""
            QDialog {
                background-color: white;
                font-size: 14px;
            }
            QGroupBox {
                font-size: 16px;
                font-weight: bold;
                color: #333;
                border: 2px solid #ddd;
                border-radius: 8px;
                margin-top: 10px;
                padding-top: 10px;
                background-color: white;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
                background-color: white;
            }
            QLabel {
                font-size: 14px;
                color: #333;
                padding: 5px;
                background-color: white;
            }
            QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
                font-size: 14px;
                padding: 8px;
                border: 2px solid #ddd;
                border-radius: 4px;
                background-color: white;
            }
            QLineEdit:focus, QSpinBox:focus, QDoubleSpinBox:focus, QComboBox:focus {
                border-color: #4CAF50;
                background-color: white;
            }
            QPushButton {
                font-size: 14px;
                padding: 10px 20px;
                border: 2px solid #4CAF50;
                border-radius: 6px;
                background-color: #4CAF50;
                color: white;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #45a049;
            }
            QPushButton:pressed {
                background-color: #3d8b40;
            }
        """)
        
        layout = QVBoxLayout()
        layout.setSpacing(15)  # 增加间距
        
        # 基本信息
        info_group = QGroupBox("基本信息")
        info_group.setStyleSheet("""
            QGroupBox {
                background-color: white;
                border: 2px solid #ddd;
                border-radius: 8px;
                margin-top: 10px;
                padding-top: 10px;
                font-size: 16px;
                font-weight: bold;
                color: #333;
            }
        """)
        info_layout = QFormLayout()
        info_layout.setSpacing(12)  # 增加表单行间距
        info_layout.setLabelAlignment(Qt.AlignRight)  # 标签右对齐
        
        self.type_label = QLabel(self.component.type.value)
        self.type_label.setStyleSheet("font-weight: bold; color: #333; font-size: 14px; background-color: white;")
        info_layout.addRow("部件类型:", self.type_label)
        
        self.motor_label = QLabel(f"舵机{self.component.motor_id}")
        self.motor_label.setStyleSheet("font-size: 14px; color: #333; background-color: white;")
        info_layout.addRow("所属舵机:", self.motor_label)
        
        self.start_time_spin = QDoubleSpinBox()
        self.start_time_spin.setRange(0, 999999)
        self.start_time_spin.setSuffix(" 秒")
        self.start_time_spin.setDecimals(2)
        self.start_time_spin.setReadOnly(True)  # 设为只读
        self.start_time_spin.setStyleSheet("""
            QDoubleSpinBox {
                background-color: #f5f5f5;
                border: 2px solid #ddd;
                color: #666;
            }
        """)
        info_layout.addRow("起始时间:", self.start_time_spin)
        
        self.duration_spin = QDoubleSpinBox()
        self.duration_spin.setRange(0.1, 999999)
        self.duration_spin.setSuffix(" 秒 (不影响实际运行时长)")
        self.duration_spin.setDecimals(2)
        self.duration_spin.setReadOnly(False)  # 设为可编辑
        self.duration_spin.setStyleSheet("""
            QDoubleSpinBox {
                color: #333;
                background-color: white;
                border: 2px solid #ddd;
            }
        """)
        self.duration_spin.valueChanged.connect(self._on_duration_changed)  # 连接值变化信号
        info_layout.addRow("持续时间:", self.duration_spin)
        
        info_group.setLayout(info_layout)
        layout.addWidget(info_group)
        
        # 参数设置
        self.params_group = QGroupBox("参数设置")
        self.params_group.setStyleSheet("""
            QGroupBox {
                background-color: white;
                border: 2px solid #ddd;
                border-radius: 8px;
                margin-top: 10px;
                padding-top: 10px;
                font-size: 16px;
                font-weight: bold;
                color: #333;
            }
        """)
        self.params_layout = QFormLayout()
        self.params_layout.setSpacing(12)  # 增加表单行间距
        self.params_layout.setLabelAlignment(Qt.AlignRight)  # 标签右对齐
        self.params_group.setLayout(self.params_layout)
        layout.addWidget(self.params_group)
        
        # 按钮
        button_layout = QHBoxLayout()
        button_layout.setSpacing(15)  # 按钮间距
        
        self.ok_button = QPushButton("确定")
        self.ok_button.clicked.connect(self.accept)
        self.ok_button.setDefault(True)
        self.ok_button.setMinimumSize(100, 40)  # 设置最小尺寸
        
        self.cancel_button = QPushButton("取消")
        self.cancel_button.clicked.connect(self.reject)
        self.cancel_button.setMinimumSize(100, 40)  # 设置最小尺寸
        self.cancel_button.setStyleSheet("""
            QPushButton {
                background-color: #f44336;
                border: 2px solid #f44336;
            }
            QPushButton:hover {
                background-color: #da190b;
            }
            QPushButton:pressed {
                background-color: #b71c1c;
            }
        """)
        
        button_layout.addStretch()
        button_layout.addWidget(self.ok_button)
        button_layout.addWidget(self.cancel_button)
        
        layout.addLayout(button_layout)
        self.setLayout(layout)
    
    
    def _create_parameter_controls(self):
        """创建参数控件"""
        # 清除现有控件
        while self.params_layout.count():
            child = self.params_layout.takeAt(0)
            if child.widget():
                child.widget().deleteLater()
        
        if self.component.type == ComponentType.FORWARD_ROTATION:
            self._create_rotation_controls("正转")
        elif self.component.type == ComponentType.REVERSE_ROTATION:
            self._create_rotation_controls("反转")
        elif self.component.type == ComponentType.DELAY:
            self._create_delay_controls()
        elif self.component.type == ComponentType.HOME:
            self._create_home_controls()
        elif self.component.type == ComponentType.STOP:
            self._create_stop_controls()
    
    def _create_rotation_controls(self, direction: str):
        """创建旋转控件"""
        # 目标角度
        self.angle_spin = QDoubleSpinBox()
        self.angle_spin.setRange(-999999999, 999999999)  # 无限制
        self.angle_spin.setSuffix(" 度")
        self.angle_spin.setDecimals(1)
        self.angle_spin.setStyleSheet("""
            QDoubleSpinBox {
                color: #333;
                background-color: white;
                border: 2px solid #ddd;
            }
        """)
        self.angle_spin.setValue(self.component.parameters.get('target_angle', 90.0))
        
        # 反转部件: 当角度改变时,自动确保为负值
        if self.component.type == ComponentType.REVERSE_ROTATION:
            self.angle_spin.valueChanged.connect(self._ensure_negative_angle)
        
        self.params_layout.addRow(f"{direction}角度:", self.angle_spin)
        
        # 运动时间（毫秒）
        self.speed_spin = QSpinBox()
        self.speed_spin.setRange(100, 10000)  # 100ms - 10000ms
        self.speed_spin.setSuffix(" 毫秒")
        self.speed_spin.setStyleSheet("""
            QSpinBox {
                color: #333;
                background-color: white;
                border: 2px solid #ddd;
            }
        """)
        # 使用保存的运动时间
        self.speed_spin.setValue(self.component.parameters.get('speed_ms', 1000))
        self.params_layout.addRow("运动时间:", self.speed_spin)
    
    def _ensure_negative_angle(self, value: float):
        """确保反转角度为负值"""
        if value > 0:
            # 阻止信号循环
            self.angle_spin.blockSignals(True)
            self.angle_spin.setValue(-value)
            self.angle_spin.blockSignals(False)
    
    def _create_delay_controls(self):
        """创建延时控件"""
        # 延时时长
        self.delay_spin = QDoubleSpinBox()
        self.delay_spin.setRange(0.01, 999999)
        self.delay_spin.setSuffix(" 秒")
        self.delay_spin.setDecimals(3)
        self.delay_spin.setStyleSheet("""
            QDoubleSpinBox {
                color: #333;
                background-color: white;
                border: 2px solid #ddd;
            }
        """)
        self.delay_spin.setValue(self.component.parameters.get('delay_time', 1.0))
        self.params_layout.addRow("延时时长:", self.delay_spin)
    
    def _create_home_controls(self):
        """创建归零控件"""
        # 运动时间（毫秒）
        self.speed_spin = QSpinBox()
        self.speed_spin.setRange(100, 10000)  # 100ms - 10000ms
        self.speed_spin.setSuffix(" 毫秒")
        self.speed_spin.setStyleSheet("""
            QSpinBox {
                color: #333;
                background-color: white;
                border: 2px solid #ddd;
            }
        """)
        self.speed_spin.setValue(self.component.parameters.get('speed_ms', 1000))
        self.params_layout.addRow("运动时间:", self.speed_spin)
    
    def _create_stop_controls(self):
        """创建停止控件"""
        label = QLabel("停止部件无需额外参数")
        label.setStyleSheet("color: #666; font-style: italic;")
        self.params_layout.addRow("", label)
    
    def accept(self):
        """确定按钮点击"""
        try:
            # 更新基本参数
            self.component.start_time = self.start_time_spin.value()
            self.component.duration = self.duration_spin.value()
            
            # 更新类型特定参数
            if self.component.type in [ComponentType.FORWARD_ROTATION, ComponentType.REVERSE_ROTATION]:
                if hasattr(self, 'angle_spin'):
                    self.component.parameters['target_angle'] = self.angle_spin.value()
                if hasattr(self, 'speed_spin'):
                    self.component.parameters['speed_ms'] = self.speed_spin.value()
            elif self.component.type == ComponentType.DELAY:
                if hasattr(self, 'delay_spin'):
                    self.component.parameters['delay_time'] = self.delay_spin.value()
            elif self.component.type == ComponentType.HOME:
                if hasattr(self, 'speed_spin'):
                    self.component.parameters['speed_ms'] = self.speed_spin.value()
            
            # 发送更新信号
            self.component_updated.emit(self.component)
            
            logger.info(f"部件参数已更新: {self.component.type.value}")
            super().accept()
            
        except Exception as e:
            logger.error(f"更新部件参数失败: {e}")
            QMessageBox.warning(self, "错误", f"更新参数失败: {e}")
    
    def _on_duration_changed(self, new_duration: float):
        """处理持续时间变化"""
        old_duration = self.component.duration
        
        # 先发射信号通知父组件更新其他部件位置（在更新本地值之前）
        # 这样MotorTrack可以获取到正确的旧值
        self.duration_changed.emit(self.component.id, new_duration)
        
        # 然后更新部件的持续时间
        self.component.duration = new_duration
        
        # 注意：起始时间的更新将在父组件处理推挤后通过信号回调来更新
    
    def get_component(self):
        """获取更新后的部件对象"""
        return self.component
    
    def update_component_info(self, component_id: str, updated_component: Component):
        """更新部件信息显示"""
        if component_id == self.component.id:
            self.component = updated_component
            # 更新显示
            if hasattr(self, 'start_time_spin'):
                self.start_time_spin.setValue(self.component.start_time)
            if hasattr(self, 'duration_spin'):
                self.duration_spin.setValue(self.component.duration)

class SerialSettingsDialog(QDialog):
    """串口设置对话框"""
    
    # 信号定义
    settings_changed = pyqtSignal(dict)  # 设置改变信号
    
    def __init__(self, current_settings: dict = None, parent=None):
        super().__init__(parent)
        self.current_settings = current_settings or {}
        self.init_ui()
        self.load_settings()
    
    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle("串口设置")
        self.setModal(True)
        self.resize(400, 300)
        
        layout = QVBoxLayout()
        
        # 串口设置
        serial_group = QGroupBox("串口设置")
        serial_layout = QFormLayout()
        
        # 波特率
        self.baudrate_combo = QComboBox()
        self.baudrate_combo.addItems(["9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"])
        serial_layout.addRow("波特率:", self.baudrate_combo)
        
        # 数据位
        self.databits_combo = QComboBox()
        self.databits_combo.addItems(["7", "8"])
        serial_layout.addRow("数据位:", self.databits_combo)
        
        # 停止位
        self.stopbits_combo = QComboBox()
        self.stopbits_combo.addItems(["1", "2"])
        serial_layout.addRow("停止位:", self.stopbits_combo)
        
        # 校验位
        self.parity_combo = QComboBox()
        self.parity_combo.addItems(["无", "奇校验", "偶校验"])
        serial_layout.addRow("校验位:", self.parity_combo)
        
        # 流控制
        self.flowcontrol_combo = QComboBox()
        self.flowcontrol_combo.addItems(["无", "硬件", "软件"])
        serial_layout.addRow("流控制:", self.flowcontrol_combo)
        
        serial_group.setLayout(serial_layout)
        layout.addWidget(serial_group)
        
        # 超时设置
        timeout_group = QGroupBox("超时设置")
        timeout_layout = QFormLayout()
        
        self.timeout_spin = QDoubleSpinBox()
        self.timeout_spin.setRange(0.1, 60.0)
        self.timeout_spin.setSuffix(" 秒")
        self.timeout_spin.setDecimals(1)
        timeout_layout.addRow("读取超时:", self.timeout_spin)
        
        self.write_timeout_spin = QDoubleSpinBox()
        self.write_timeout_spin.setRange(0.1, 60.0)
        self.write_timeout_spin.setSuffix(" 秒")
        self.write_timeout_spin.setDecimals(1)
        timeout_layout.addRow("写入超时:", self.write_timeout_spin)
        
        timeout_group.setLayout(timeout_layout)
        layout.addWidget(timeout_group)
        
        # 按钮
        button_layout = QHBoxLayout()
        
        self.ok_button = QPushButton("确定")
        self.ok_button.clicked.connect(self.accept)
        self.ok_button.setDefault(True)
        
        self.cancel_button = QPushButton("取消")
        self.cancel_button.clicked.connect(self.reject)
        
        button_layout.addStretch()
        button_layout.addWidget(self.ok_button)
        button_layout.addWidget(self.cancel_button)
        
        layout.addLayout(button_layout)
        self.setLayout(layout)
    
    def refresh_ports(self):
        """刷新可用串口列表"""
        try:
            # 获取当前选中的端口
            current_port = self.port_combo.currentText()
            
            # 清空当前列表
            self.port_combo.clear()
            
            # 获取系统可用串口
            ports = serial.tools.list_ports.comports()
            
            if ports:
                # 添加检测到的串口
                for port in ports:
                    port_info = f"{port.device} - {port.description}"
                    self.port_combo.addItem(port_info, port.device)
                
                # 如果之前有选中的端口且仍然存在，则恢复选择
                if current_port:
                    for i in range(self.port_combo.count()):
                        if self.port_combo.itemData(i) == current_port:
                            self.port_combo.setCurrentIndex(i)
                            break
                    else:
                        # 如果之前的端口不存在了，添加为自定义端口
                        self.port_combo.addItem(f"{current_port} (未检测到)", current_port)
                        self.port_combo.setCurrentText(current_port)
            else:
                # 没有检测到串口
                self.port_combo.addItem("未检测到串口", "")
                
                # 如果之前有选中的端口，添加为自定义端口
                if current_port:
                    self.port_combo.addItem(f"{current_port} (未检测到)", current_port)
                    self.port_combo.setCurrentText(current_port)
                    
        except Exception as e:
            logger.error(f"刷新串口列表失败: {e}")
            self.port_combo.clear()
            self.port_combo.addItem("刷新失败", "")
    
    def get_selected_port(self):
        """获取选中的串口名称"""
        current_data = self.port_combo.currentData()
        if current_data:
            return current_data
        else:
            # 如果没有数据，返回当前文本（可能是手动输入的）
            return self.port_combo.currentText()
    
    def load_settings(self):
        """加载设置"""
        self.baudrate_combo.setCurrentText(str(self.current_settings.get('baudrate', 115200)))
        self.databits_combo.setCurrentText(str(self.current_settings.get('databits', 8)))
        self.stopbits_combo.setCurrentText(str(self.current_settings.get('stopbits', 1)))
        
        parity_map = {0: "无", 1: "奇校验", 2: "偶校验"}
        self.parity_combo.setCurrentText(parity_map.get(self.current_settings.get('parity', 0), "无"))
        
        flowcontrol_map = {0: "无", 1: "硬件", 2: "软件"}
        self.flowcontrol_combo.setCurrentText(flowcontrol_map.get(self.current_settings.get('flowcontrol', 0), "无"))
        
        self.timeout_spin.setValue(self.current_settings.get('timeout', 1.0))
        self.write_timeout_spin.setValue(self.current_settings.get('write_timeout', 1.0))
    
    def get_settings(self) -> dict:
        """获取设置"""
        parity_map = {"无": 0, "奇校验": 1, "偶校验": 2}
        flowcontrol_map = {"无": 0, "硬件": 1, "软件": 2}
        
        return {
            'baudrate': int(self.baudrate_combo.currentText()),
            'databits': int(self.databits_combo.currentText()),
            'stopbits': int(self.stopbits_combo.currentText()),
            'parity': parity_map[self.parity_combo.currentText()],
            'flowcontrol': flowcontrol_map[self.flowcontrol_combo.currentText()],
            'timeout': self.timeout_spin.value(),
            'write_timeout': self.write_timeout_spin.value()
        }
    
    
    def accept(self):
        """确定按钮点击"""
        settings = self.get_settings()
        self.settings_changed.emit(settings)
        super().accept()


class MotorSettingsDialog(QDialog):
    """电机参数设置对话框"""
    
    settings_changed = pyqtSignal(dict)
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.init_ui()
    
    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle("电机参数设置")
        self.resize(500, 400)
        
        # 应用白色背景样式
        self.setStyleSheet("""
            QDialog {
                background-color: white;
            }
            QGroupBox {
                background-color: white;
                font-size: 12px;
                font-weight: bold;
                border: 1px solid #CCCCCC;
                border-radius: 5px;
                margin-top: 10px;
                padding-top: 10px;
            }
            QGroupBox::title {
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px 0 5px;
                color: #333333;
            }
            QLabel {
                background-color: white;
                color: #333333;
                font-size: 11px;
            }
            QSpinBox, QDoubleSpinBox {
                background-color: white;
                color: #333333;
                font-size: 11px;
                padding: 3px;
                border: 1px solid #CCCCCC;
                border-radius: 3px;
            }
            QPushButton {
                background-color: #0066CC;
                color: white;
                font-size: 11px;
                padding: 5px 15px;
                border: none;
                border-radius: 3px;
            }
            QPushButton:hover {
                background-color: #0052A3;
            }
        """)
        
        layout = QVBoxLayout()
        
        # 标题说明
        title_label = QLabel("步进电机参数设置")
        title_label.setStyleSheet("font-size: 14px; font-weight: bold; margin-bottom: 10px;")
        layout.addWidget(title_label)
        
        desc_label = QLabel(
            "本软件使用角度(°)作为单位,grblHAL使用毫米(mm)作为单位。\n"
            "软件会根据电机参数自动计算并转换单位,grblHAL配置($100-$107)保持不变。\n\n"
            "核心公式: gcode_distance = (theta × microstep) / (alpha × steps_per_unit)"
        )
        desc_label.setStyleSheet("color: #666666; margin-bottom: 10px;")
        desc_label.setWordWrap(True)
        layout.addWidget(desc_label)
        
        # 电机参数组
        motor_group = QGroupBox("步进电机参数")
        motor_layout = QFormLayout()
        
        # 电机步距角 - 下拉选框
        self.step_angle_combo = QComboBox()
        self.step_angle_combo.addItems([
            "0.9° (400步/圈)",
            "1.8° (200步/圈)",
            "3.6° (100步/圈)",
            "7.5° (48步/圈)"
        ])
        self.step_angle_combo.setCurrentIndex(1)  # 默认1.8°
        self.step_angle_combo.setStyleSheet("""
            QComboBox {
                background-color: white;
                color: #333333;
                font-size: 11px;
                padding: 3px;
                border: 1px solid #CCCCCC;
                border-radius: 3px;
            }
        """)
        motor_layout.addRow("电机步距角:", self.step_angle_combo)
        
        # 自动计算步数/圈
        self.steps_per_rev_label = QLabel("200")
        self.steps_per_rev_label.setStyleSheet("color: #666666;")
        motor_layout.addRow("  (步数/圈):", self.steps_per_rev_label)
        
        # 驱动器细分 - 下拉选框
        self.microsteps_combo = QComboBox()
        self.microsteps_combo.addItems([
            "1",
            "2",
            "4",
            "8",
            "16",
            "32",
            "64"
        ])
        self.microsteps_combo.setCurrentIndex(3)  # 默认8
        self.microsteps_combo.setStyleSheet("""
            QComboBox {
                background-color: white;
                color: #333333;
                font-size: 11px;
                padding: 3px;
                border: 1px solid #CCCCCC;
                border-radius: 3px;
            }
        """)
        motor_layout.addRow("驱动器细分:", self.microsteps_combo)
        
        motor_group.setLayout(motor_layout)
        layout.addWidget(motor_group)
        
        # grblHAL参数组
        grbl_group = QGroupBox("grblHAL参数(从grblHAL读取)")
        grbl_layout = QFormLayout()
        
        # grblHAL的steps/mm设置
        self.grbl_steps_per_mm_spin = QDoubleSpinBox()
        self.grbl_steps_per_mm_spin.setRange(0.1, 10000.0)
        self.grbl_steps_per_mm_spin.setValue(250.0)
        self.grbl_steps_per_mm_spin.setDecimals(5)
        self.grbl_steps_per_mm_spin.setSuffix(" 步/mm")
        grbl_layout.addRow("$100-$107:", self.grbl_steps_per_mm_spin)
        
        grbl_group.setLayout(grbl_layout)
        layout.addWidget(grbl_group)
        
        # 计算结果组
        calc_group = QGroupBox("转换计算")
        calc_layout = QFormLayout()
        
        self.steps_per_degree_label = QLabel("8.889")
        self.steps_per_degree_label.setStyleSheet("font-weight: bold; color: #0066CC;")
        calc_layout.addRow("步数/度:", self.steps_per_degree_label)
        
        self.mm_per_degree_label = QLabel("0.0356")
        self.mm_per_degree_label.setStyleSheet("font-weight: bold; color: #FF8C00;")
        calc_layout.addRow("mm/度 (转换系数):", self.mm_per_degree_label)
        
        calc_group.setLayout(calc_layout)
        layout.addWidget(calc_group)
        
        # 连接信号更新计算
        self.step_angle_combo.currentIndexChanged.connect(self.update_calculation)
        self.microsteps_combo.currentIndexChanged.connect(self.update_calculation)
        self.grbl_steps_per_mm_spin.valueChanged.connect(self.update_calculation)
        
        # 说明文本
        info_label = QLabel(
            "<b>核心转换公式:</b><br>"
            "• gcode_distance = (theta × microstep) / (alpha × steps_per_unit)<br>"
            "• mm/度 = microstep / (alpha × steps_per_unit)<br><br>"
            "<b>示例 (alpha=1.8, microstep=8, steps_per_unit=250):</b><br>"
            "• mm/度 = 8 / (1.8 × 250) = 0.01778<br>"
            "• 转90° = 90 × 0.01778 = 1.6 mm<br>"
            "• G-code: G1 X1.6 F100"
        )
        info_label.setStyleSheet("color: #666666; background-color: #F5F5F5; padding: 10px; border-radius: 5px;")
        info_label.setWordWrap(True)
        layout.addWidget(info_label)
        
        layout.addStretch()
        
        # 按钮
        button_layout = QHBoxLayout()
        
        save_btn = QPushButton("保存设置")
        save_btn.clicked.connect(self.accept)
        
        cancel_btn = QPushButton("取消")
        cancel_btn.setStyleSheet("background-color: #999999;")
        cancel_btn.clicked.connect(self.reject)
        
        button_layout.addStretch()
        button_layout.addWidget(save_btn)
        button_layout.addWidget(cancel_btn)
        
        layout.addLayout(button_layout)
        self.setLayout(layout)
        
        # 初始计算
        self.update_calculation()
    
    def update_calculation(self):
        """更新计算结果 - 使用正确的公式"""
        # 从下拉框获取步距角 (alpha)
        step_angle_text = self.step_angle_combo.currentText()
        alpha = float(step_angle_text.split('°')[0])  # 例如: 1.8°
        
        # 从下拉框获取细分 (microstep)
        microstep = int(self.microsteps_combo.currentText())  # 例如: 8
        
        # grblHAL参数 (steps_per_unit)
        steps_per_unit = self.grbl_steps_per_mm_spin.value()  # 例如: 250 steps/mm
        
        # 1. 从步距角计算步数/圈: 360 / alpha
        steps_per_rev = 360.0 / alpha
        self.steps_per_rev_label.setText(f"{steps_per_rev:.0f}")
        
        # 2. 计算步数/度: microstep / alpha
        steps_per_degree = microstep / alpha
        
        # 3. 核心转换公式: mm/度 = microstep / (alpha × steps_per_unit)
        # 推导自: gcode_distance = (theta × microstep) / (alpha × steps_per_unit)
        mm_per_degree = microstep / (alpha * steps_per_unit)
        
        self.steps_per_degree_label.setText(f"{steps_per_degree:.5f}")
        self.mm_per_degree_label.setText(f"{mm_per_degree:.5f}")
    
    def get_settings(self):
        """获取设置"""
        # 从下拉框获取步距角 (alpha)
        step_angle_text = self.step_angle_combo.currentText()
        alpha = float(step_angle_text.split('°')[0])
        
        # 从下拉框获取细分 (microstep)
        microstep = int(self.microsteps_combo.currentText())
        
        # grblHAL参数 (steps_per_unit)
        steps_per_unit = self.grbl_steps_per_mm_spin.value()
        
        # 计算
        steps_per_rev = 360.0 / alpha
        steps_per_degree = microstep / alpha
        mm_per_degree = microstep / (alpha * steps_per_unit)
        
        return {
            'step_angle': alpha,
            'steps_per_rev': steps_per_rev,
            'microsteps': microstep,
            'grbl_steps_per_mm': steps_per_unit,
            'steps_per_degree': steps_per_degree,
            'mm_per_degree': mm_per_degree
        }
    
    def set_settings(self, settings: dict):
        """设置参数"""
        if 'step_angle' in settings:
            step_angle = settings['step_angle']
            # 在下拉框中查找匹配项
            for i in range(self.step_angle_combo.count()):
                text = self.step_angle_combo.itemText(i)
                if text.startswith(f"{step_angle}°"):
                    self.step_angle_combo.setCurrentIndex(i)
                    break
        
        if 'microsteps' in settings:
            microsteps = settings['microsteps']
            # 在下拉框中查找匹配项
            for i in range(self.microsteps_combo.count()):
                if self.microsteps_combo.itemText(i) == str(microsteps):
                    self.microsteps_combo.setCurrentIndex(i)
                    break
        
        if 'grbl_steps_per_mm' in settings:
            self.grbl_steps_per_mm_spin.setValue(settings['grbl_steps_per_mm'])
    
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
        self.resize(400, 250)
        
        # 应用白色背景样式
        self.setStyleSheet("""
            QDialog {
                background-color: white;
                font-size: 14px;
            }
            QGroupBox {
                background-color: white;
                border: 2px solid #ddd;
                border-radius: 8px;
                margin-top: 10px;
                padding-top: 10px;
                font-size: 16px;
                font-weight: bold;
            }
            QGroupBox::title {
                background-color: white;
                subcontrol-origin: margin;
                left: 10px;
                padding: 0 5px;
            }
            QLabel {
                background-color: white;
                color: #333;
                font-size: 14px;
            }
            QSpinBox, QDoubleSpinBox, QComboBox {
                background-color: white;
                color: #333;
                border: 1px solid #ccc;
                border-radius: 4px;
                padding: 5px;
                font-size: 14px;
            }
        """)
        
        layout = QVBoxLayout()
        
        # 当前部件信息
        info_group = QGroupBox("当前部件")
        info_layout = QFormLayout()
        info_layout.setSpacing(12)
        info_layout.setLabelAlignment(Qt.AlignRight)
        
        type_label = QLabel(self.component.type.value)
        type_label.setStyleSheet("font-weight: bold; color: #2196F3;")
        info_layout.addRow("部件类型:", type_label)
        
        current_start_label = QLabel(f"{self.component.start_time:.2f} 秒")
        info_layout.addRow("当前起始时间:", current_start_label)
        
        duration_label = QLabel(f"{self.component.duration:.2f} 秒")
        info_layout.addRow("持续时间:", duration_label)
        
        info_group.setLayout(info_layout)
        layout.addWidget(info_group)
        
        # 移动目标设置
        target_group = QGroupBox("移动目标")
        target_layout = QFormLayout()
        target_layout.setSpacing(12)
        target_layout.setLabelAlignment(Qt.AlignRight)
        
        # 目标电机选择
        self.motor_combo = QComboBox()
        for i in range(8):
            self.motor_combo.addItem(f"电机 {i + 1} (轴 {'XYZABCUV'[i]})")
        target_layout.addRow("目标电机:", self.motor_combo)
        
        # 目标起始时间
        self.start_time_spin = QDoubleSpinBox()
        self.start_time_spin.setRange(0, 10000)
        self.start_time_spin.setSuffix(" 秒")
        self.start_time_spin.setSingleStep(1.0)
        self.start_time_spin.setValue(self.component.start_time)
        target_layout.addRow("起始时间:", self.start_time_spin)
        
        target_group.setLayout(target_layout)
        layout.addWidget(target_group)
        
        # 按钮
        button_layout = QHBoxLayout()
        button_layout.setSpacing(15)
        
        self.ok_button = QPushButton("确定")
        self.ok_button.setMinimumSize(100, 40)
        self.ok_button.setStyleSheet("""
            QPushButton {
                background-color: #4CAF50;
                color: white;
                border: none;
                border-radius: 4px;
                font-size: 14px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #45a049;
            }
        """)
        self.ok_button.clicked.connect(self.accept)
        
        self.cancel_button = QPushButton("取消")
        self.cancel_button.setMinimumSize(100, 40)
        self.cancel_button.setStyleSheet("""
            QPushButton {
                background-color: #f44336;
                color: white;
                border: none;
                border-radius: 4px;
                font-size: 14px;
                font-weight: bold;
            }
            QPushButton:hover {
                background-color: #da190b;
            }
        """)
        self.cancel_button.clicked.connect(self.reject)
        
        button_layout.addStretch()
        button_layout.addWidget(self.ok_button)
        button_layout.addWidget(self.cancel_button)
        
        layout.addLayout(button_layout)
        
        self.setLayout(layout)
    
    def get_result(self):
        """获取移动目标"""
        motor_id = self.motor_combo.currentIndex()
        start_time = self.start_time_spin.value()
        return motor_id, start_time


class MotionTableDialog(QDialog):
    """舵机运动表对话框"""
    
    def __init__(self, table_text: str, parent=None):
        super().__init__(parent)
        self.table_text = table_text
        self.init_ui()
    
    def init_ui(self):
        """初始化UI"""
        self.setWindowTitle("舵机运动表")
        self.resize(800, 600)
        
        # 应用白色背景样式
        self.setStyleSheet("""
            QDialog {
                background-color: white;
                font-size: 14px;
            }
            QTextEdit {
                background-color: white;
                color: #333;
                border: 1px solid #ccc;
                border-radius: 4px;
                padding: 10px;
                font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
                font-size: 12px;
                line-height: 1.4;
            }
            QPushButton {
                background-color: #4CAF50;
                color: white;
                border: none;
                border-radius: 4px;
                font-size: 14px;
                font-weight: bold;
                padding: 8px 16px;
                min-width: 80px;
            }
            QPushButton:hover {
                background-color: #45a049;
            }
            QPushButton#save_button {
                background-color: #2196F3;
            }
            QPushButton#save_button:hover {
                background-color: #1976D2;
            }
            QPushButton#copy_button {
                background-color: #FF9800;
            }
            QPushButton#copy_button:hover {
                background-color: #F57C00;
            }
        """)
        
        layout = QVBoxLayout()
        
        # 标题
        title_label = QLabel("8轴电机运动逻辑表")
        title_label.setStyleSheet("""
            QLabel {
                font-size: 18px;
                font-weight: bold;
                color: #333;
                padding: 10px;
                background-color: #f5f5f5;
                border-radius: 4px;
                margin-bottom: 10px;
            }
        """)
        layout.addWidget(title_label)
        
        # 文本显示区域
        self.text_edit = QTextEdit()
        self.text_edit.setPlainText(self.table_text)
        self.text_edit.setReadOnly(True)
        layout.addWidget(self.text_edit)
        
        # 按钮区域
        button_layout = QHBoxLayout()
        button_layout.setSpacing(10)
        
        # 复制按钮
        self.copy_button = QPushButton("复制到剪贴板")
        self.copy_button.setObjectName("copy_button")
        self.copy_button.clicked.connect(self.copy_to_clipboard)
        button_layout.addWidget(self.copy_button)
        
        # 保存按钮
        self.save_button = QPushButton("保存到文件")
        self.save_button.setObjectName("save_button")
        self.save_button.clicked.connect(self.save_to_file)
        button_layout.addWidget(self.save_button)
        
        button_layout.addStretch()
        
        # 关闭按钮
        self.close_button = QPushButton("关闭")
        self.close_button.clicked.connect(self.accept)
        button_layout.addWidget(self.close_button)
        
        layout.addLayout(button_layout)
        
        self.setLayout(layout)
    
    def copy_to_clipboard(self):
        """复制到剪贴板"""
        from PyQt5.QtWidgets import QApplication
        clipboard = QApplication.clipboard()
        clipboard.setText(self.table_text)
        
        from PyQt5.QtWidgets import QMessageBox
        QMessageBox.information(self, "成功", "运动逻辑表已复制到剪贴板")
    
    def save_to_file(self):
        """保存到文件"""
        from PyQt5.QtWidgets import QFileDialog
        filename, _ = QFileDialog.getSaveFileName(
            self,
            "保存运动逻辑表",
            f"motion_table_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt",
            "文本文件 (*.txt);;所有文件 (*.*)"
        )
        
        if filename:
            try:
                with open(filename, 'w', encoding='utf-8') as f:
                    f.write(self.table_text)
                from PyQt5.QtWidgets import QMessageBox
                QMessageBox.information(self, "保存成功", f"运动逻辑表已保存到: {filename}")
            except Exception as e:
                from PyQt5.QtWidgets import QMessageBox
                QMessageBox.warning(self, "保存失败", f"保存运动逻辑表失败: {e}")


class DefaultParametersDialog(QDialog):
    """默认参数配置对话框"""
    
    def __init__(self, config_manager, parent=None):
        super().__init__(parent)
        self.config_manager = config_manager
        self.setWindowTitle("默认参数配置")
        self.setModal(True)
        self.resize(600, 700)
        self.init_ui()
        self.load_settings()
    
    def init_ui(self):
        """初始化UI"""
        layout = QVBoxLayout()
        layout.setSpacing(10)
        
        # 创建标签页
        self.tab_widget = QTabWidget()
        
        # 电机参数标签页
        self.motor_tab = self._create_motor_tab()
        self.tab_widget.addTab(self.motor_tab, "电机参数")
        
        # 默认速度标签页
        self.speed_tab = self._create_speed_tab()
        self.tab_widget.addTab(self.speed_tab, "默认速度")
        
        # 默认角度标签页
        self.angle_tab = self._create_angle_tab()
        self.tab_widget.addTab(self.angle_tab, "默认角度")
        
        # 默认时长标签页
        self.duration_tab = self._create_duration_tab()
        self.tab_widget.addTab(self.duration_tab, "默认时长")
        
        # grblHAL参数标签页
        self.grblhal_tab = self._create_grblhal_tab()
        self.tab_widget.addTab(self.grblhal_tab, "grblHAL参数")
        
        layout.addWidget(self.tab_widget)
        
        # 按钮
        button_layout = QHBoxLayout()
        
        reset_btn = QPushButton("恢复默认")
        reset_btn.clicked.connect(self.reset_to_default)
        button_layout.addWidget(reset_btn)
        
        button_layout.addStretch()
        
        cancel_btn = QPushButton("取消")
        cancel_btn.clicked.connect(self.reject)
        button_layout.addWidget(cancel_btn)
        
        save_btn = QPushButton("保存")
        save_btn.clicked.connect(self.save_settings)
        save_btn.setDefault(True)
        button_layout.addWidget(save_btn)
        
        layout.addLayout(button_layout)
        
        self.setLayout(layout)
    
    def _create_motor_tab(self) -> QWidget:
        """创建电机参数标签页"""
        widget = QWidget()
        layout = QFormLayout()
        layout.setSpacing(15)
        
        # 电机步距角
        self.step_angle_combo = QComboBox()
        self.step_angle_combo.addItems(['0.9°', '1.8°', '3.6°', '7.5°'])
        self.step_angle_combo.setEditable(True)
        layout.addRow("电机步距角:", self.step_angle_combo)
        
        # 驱动器细分
        self.microsteps_combo = QComboBox()
        self.microsteps_combo.addItems(['1', '2', '4', '8', '16', '32', '64', '128', '256'])
        layout.addRow("驱动器细分:", self.microsteps_combo)
        
        # grblHAL steps/mm
        self.grbl_steps_spin = QDoubleSpinBox()
        self.grbl_steps_spin.setRange(1, 10000)
        self.grbl_steps_spin.setDecimals(5)
        self.grbl_steps_spin.setSuffix(" steps/mm")
        layout.addRow("grblHAL steps/mm:", self.grbl_steps_spin)
        
        # 说明
        info_label = QLabel(
            "这些参数用于角度与mm之间的转换\n"
            "请确保与grblHAL的$100-$107配置一致"
        )
        info_label.setStyleSheet("color: #666; font-size: 11px; margin-top: 10px;")
        info_label.setWordWrap(True)
        layout.addRow("", info_label)
        
        widget.setLayout(layout)
        return widget
    
    def _create_speed_tab(self) -> QWidget:
        """创建默认速度标签页"""
        widget = QWidget()
        layout = QFormLayout()
        layout.setSpacing(15)
        
        # 旋转速度
        self.rotation_speed_spin = QDoubleSpinBox()
        self.rotation_speed_spin.setRange(1, 999999999)
        self.rotation_speed_spin.setDecimals(0)
        self.rotation_speed_spin.setSuffix(" 度/分钟")
        layout.addRow("旋转默认速度:", self.rotation_speed_spin)
        
        # 归零速度
        self.home_speed_spin = QDoubleSpinBox()
        self.home_speed_spin.setRange(1, 999999999)
        self.home_speed_spin.setDecimals(0)
        self.home_speed_spin.setSuffix(" 度/分钟")
        layout.addRow("归零默认速度:", self.home_speed_spin)
        
        # 说明
        info_label = QLabel(
            "新建部件时,将使用这些默认速度值\n"
            "实际速度会根据角度和持续时间自动调整"
        )
        info_label.setStyleSheet("color: #666; font-size: 11px; margin-top: 10px;")
        info_label.setWordWrap(True)
        layout.addRow("", info_label)
        
        widget.setLayout(layout)
        return widget
    
    def _create_angle_tab(self) -> QWidget:
        """创建默认角度标签页"""
        widget = QWidget()
        layout = QFormLayout()
        layout.setSpacing(15)
        
        # 正转角度
        self.forward_angle_spin = QDoubleSpinBox()
        self.forward_angle_spin.setRange(-360000, 360000)
        self.forward_angle_spin.setDecimals(1)
        self.forward_angle_spin.setSuffix(" 度")
        layout.addRow("正转默认角度:", self.forward_angle_spin)
        
        # 反转角度
        self.reverse_angle_spin = QDoubleSpinBox()
        self.reverse_angle_spin.setRange(-360000, 360000)
        self.reverse_angle_spin.setDecimals(1)
        self.reverse_angle_spin.setSuffix(" 度")
        layout.addRow("反转默认角度:", self.reverse_angle_spin)
        
        # 说明
        info_label = QLabel(
            "新建旋转部件时,将使用这些默认角度值\n"
            "正值为正转,负值为反转"
        )
        info_label.setStyleSheet("color: #666; font-size: 11px; margin-top: 10px;")
        info_label.setWordWrap(True)
        layout.addRow("", info_label)
        
        widget.setLayout(layout)
        return widget
    
    def _create_duration_tab(self) -> QWidget:
        """创建默认时长标签页"""
        widget = QWidget()
        layout = QFormLayout()
        layout.setSpacing(15)
        
        # 旋转持续时间
        self.rotation_duration_spin = QDoubleSpinBox()
        self.rotation_duration_spin.setRange(0.1, 3600)
        self.rotation_duration_spin.setDecimals(1)
        self.rotation_duration_spin.setSuffix(" 秒")
        layout.addRow("旋转默认持续时间:", self.rotation_duration_spin)
        
        # 延时时长
        self.delay_duration_spin = QDoubleSpinBox()
        self.delay_duration_spin.setRange(0.1, 3600)
        self.delay_duration_spin.setDecimals(1)
        self.delay_duration_spin.setSuffix(" 秒")
        layout.addRow("延时默认时长:", self.delay_duration_spin)
        
        # 说明
        info_label = QLabel(
            "新建部件时,将使用这些默认持续时间值\n"
            "速度会根据角度和持续时间自动计算"
        )
        info_label.setStyleSheet("color: #666; font-size: 11px; margin-top: 10px;")
        info_label.setWordWrap(True)
        layout.addRow("", info_label)
        
        widget.setLayout(layout)
        return widget
    
    def _create_grblhal_tab(self) -> QWidget:
        """创建grblHAL参数标签页"""
        widget = QWidget()
        layout = QFormLayout()
        layout.setSpacing(15)
        
        # 最大行程
        self.max_travel_spin = QDoubleSpinBox()
        self.max_travel_spin.setRange(100, 999999)
        self.max_travel_spin.setDecimals(0)
        self.max_travel_spin.setSuffix(" mm")
        layout.addRow("最大行程($130-$137):", self.max_travel_spin)
        
        # 自动设置
        self.auto_set_travel_check = QCheckBox("连接时自动设置最大行程")
        layout.addRow("", self.auto_set_travel_check)
        
        # 说明
        info_label = QLabel(
            "最大行程限制:\n"
            "• 默认200mm约等于31圈\n"
            "• 建议设置36000mm,约等于5625圈\n"
            "• 如果启用自动设置,连接时会自动发送$130-$137配置"
        )
        info_label.setStyleSheet("color: #666; font-size: 11px; margin-top: 10px;")
        info_label.setWordWrap(True)
        layout.addRow("", info_label)
        
        widget.setLayout(layout)
        return widget
    
    def load_settings(self):
        """从配置管理器加载设置"""
        # 电机参数
        motor = self.config_manager.config['motor']
        step_angle_text = f"{motor['step_angle']}°"
        index = self.step_angle_combo.findText(step_angle_text)
        if index >= 0:
            self.step_angle_combo.setCurrentIndex(index)
        else:
            self.step_angle_combo.setCurrentText(step_angle_text)
        
        microsteps_text = str(motor['microsteps'])
        index = self.microsteps_combo.findText(microsteps_text)
        if index >= 0:
            self.microsteps_combo.setCurrentIndex(index)
        else:
            self.microsteps_combo.setCurrentText(microsteps_text)
        
        self.grbl_steps_spin.setValue(motor['grbl_steps_per_mm'])
        
        # 默认速度
        speed = self.config_manager.config['default_speed']
        self.rotation_speed_spin.setValue(speed['rotation'])
        self.home_speed_spin.setValue(speed['home'])
        
        # 默认角度
        angle = self.config_manager.config['default_angle']
        self.forward_angle_spin.setValue(angle['forward'])
        self.reverse_angle_spin.setValue(angle['reverse'])
        
        # 默认时长
        duration = self.config_manager.config['default_duration']
        self.rotation_duration_spin.setValue(duration['rotation'])
        self.delay_duration_spin.setValue(duration['delay'])
        
        # grblHAL参数
        grblhal = self.config_manager.config['grblhal']
        self.max_travel_spin.setValue(grblhal['max_travel'])
        self.auto_set_travel_check.setChecked(grblhal['auto_set_travel'])
    
    def save_settings(self):
        """保存设置到配置管理器"""
        try:
            # 电机参数
            step_angle_text = self.step_angle_combo.currentText()
            step_angle = float(step_angle_text.replace('°', ''))
            microsteps = int(self.microsteps_combo.currentText())
            grbl_steps = self.grbl_steps_spin.value()
            
            self.config_manager.config['motor'] = {
                'step_angle': step_angle,
                'microsteps': microsteps,
                'grbl_steps_per_mm': grbl_steps
            }
            
            # 默认速度
            self.config_manager.config['default_speed'] = {
                'rotation': self.rotation_speed_spin.value(),
                'home': self.home_speed_spin.value()
            }
            
            # 默认角度
            self.config_manager.config['default_angle'] = {
                'forward': self.forward_angle_spin.value(),
                'reverse': self.reverse_angle_spin.value()
            }
            
            # 默认时长
            self.config_manager.config['default_duration'] = {
                'rotation': self.rotation_duration_spin.value(),
                'delay': self.delay_duration_spin.value()
            }
            
            # grblHAL参数
            self.config_manager.config['grblhal'] = {
                'max_travel': self.max_travel_spin.value(),
                'auto_set_travel': self.auto_set_travel_check.isChecked()
            }
            
            # 保存到文件
            if self.config_manager.save():
                QMessageBox.information(self, "保存成功", "默认参数配置已保存")
                self.accept()
            else:
                QMessageBox.warning(self, "保存失败", "保存配置文件失败")
        
        except Exception as e:
            logger.error(f"保存配置失败: {e}")
            QMessageBox.warning(self, "错误", f"保存配置失败: {e}")
    
    def reset_to_default(self):
        """重置为默认值"""
        reply = QMessageBox.question(
            self, 
            "确认重置", 
            "确定要恢复所有默认参数吗?",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No
        )
        
        if reply == QMessageBox.Yes:
            self.config_manager.reset_to_default()
            self.load_settings()
            QMessageBox.information(self, "重置成功", "所有参数已恢复为默认值")


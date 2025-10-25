#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
主窗口
应用程序的主界面
"""

from PyQt5.QtWidgets import (QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
                             QMenuBar, QMenu, QAction, QToolBar, QStatusBar,
                             QDockWidget, QSplitter, QMessageBox, QFileDialog,
                             QProgressBar, QLabel, QTextEdit, QDialog, QTabWidget,
                             QGroupBox, QFormLayout, QPushButton, QLineEdit, QSizePolicy,
                             QCheckBox)
from PyQt5.QtCore import Qt, pyqtSignal, QTimer
from PyQt5.QtGui import QIcon, QKeySequence, QFont
import os
from datetime import datetime
from ui.component_palette import ComponentPalette
from ui.timeline_widget import TimelineWidget
from ui.dialogs import ComponentEditDialog, SerialSettingsDialog, ServoSettingsDialog
from core.serial_comm import SerialComm
from core.servo_commander import ServoCommander
from core.project_manager import ProjectManager
from core.config_manager import ConfigManager
from models.component import Component
from core.logger import get_logger
from models.timeline_data import TimelineData, LoopMode
from models.component import ComponentType
import logging

logger = get_logger()

class MainWindow(QMainWindow):
    """主窗口类"""
    
    def __init__(self):
        super().__init__()
        
        # 初始化配置管理器(最先初始化)
        self.config_manager = ConfigManager()
        logger.info("配置管理器初始化完成")
        
        # 初始化组件
        self.timeline_data = TimelineData()
        self.serial_comm = SerialComm()
        self.servo_commander = ServoCommander()
        self.project_manager = ProjectManager()
        
        # 舵机设置（简化配置）
        self.servo_settings = {
            'servo_count': 18,
            'angle_min': 0.0,
            'angle_max': 180.0,
            'default_speed_ms': 1000
        }
        
        # 舵机使能状态管理
        self.servo_enable_states = [False] * 18  # 18个舵机的使能状态
        self.enable_all_mode = False  # 全部使能模式
        
        logger.info(f"舵机参数: {self.servo_settings['servo_count']}个舵机, 角度范围{self.servo_settings['angle_min']}-{self.servo_settings['angle_max']}°")
        
        # 状态变量
        self.is_connected = False
        self.is_running = False
        self.current_component_id = ""
        
        # 初始化UI
        self.init_ui()
        self.setup_connections()
        self.setup_logging()
        
        # 设置窗口属性
        self.setWindowTitle("18通道舵机时间轴控制上位机 v1.0.0")
        self.setMinimumSize(1200, 800)
        self.resize(1400, 900)
        
        # 居中显示
        self.center_window()
    
    def init_ui(self):
        """初始化UI"""
        # 创建中央部件 - 只包含左侧面板和时间轴
        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        
        central_layout = QHBoxLayout()
        central_layout.setContentsMargins(5, 5, 5, 5)
        central_layout.setSpacing(5)
        
        # 左侧：部件面板
        self.component_palette = ComponentPalette()
        self.component_palette.setMinimumWidth(200)
        self.component_palette.setMaximumWidth(250)
        central_layout.addWidget(self.component_palette)
        
        # 中间：时间轴 - 传递配置管理器
        self.timeline_widget = TimelineWidget(config_manager=self.config_manager)
        central_layout.addWidget(self.timeline_widget, 1)
        
        central_widget.setLayout(central_layout)
        
        # 创建菜单栏
        self.create_menu_bar()
        
        # 创建工具栏
        self.create_toolbar()
        
        # 创建状态栏
        self.create_status_bar()
        
        # 创建停靠窗口（可浮动用于测试，但不可在主窗口内拖动）
        self.create_dock_widgets()
    
    def create_menu_bar(self):
        """创建菜单栏"""
        menubar = self.menuBar()
        
        # 文件菜单
        file_menu = menubar.addMenu("文件(&F)")
        
        new_action = QAction("新建(&N)", self)
        new_action.setShortcut(QKeySequence.New)
        new_action.triggered.connect(self.new_project)
        file_menu.addAction(new_action)
        
        open_action = QAction("打开(&O)", self)
        open_action.setShortcut(QKeySequence.Open)
        open_action.triggered.connect(self.open_project)
        file_menu.addAction(open_action)
        
        save_action = QAction("保存(&S)", self)
        save_action.setShortcut(QKeySequence.Save)
        save_action.triggered.connect(self.save_project)
        file_menu.addAction(save_action)
        
        save_as_action = QAction("另存为(&A)", self)
        save_as_action.setShortcut(QKeySequence.SaveAs)
        save_as_action.triggered.connect(self.save_as_project)
        file_menu.addAction(save_as_action)
        
        file_menu.addSeparator()
        
        export_commands_action = QAction("导出命令序列(&E)", self)
        export_commands_action.triggered.connect(self.export_commands)
        file_menu.addAction(export_commands_action)
        
        file_menu.addSeparator()
        
        exit_action = QAction("退出(&X)", self)
        exit_action.setShortcut(QKeySequence.Quit)
        exit_action.triggered.connect(self.close)
        file_menu.addAction(exit_action)
        
        # 编辑菜单
        edit_menu = menubar.addMenu("编辑(&E)")
        
        undo_action = QAction("撤销(&U)", self)
        undo_action.setShortcut(QKeySequence.Undo)
        edit_menu.addAction(undo_action)
        
        redo_action = QAction("重做(&R)", self)
        redo_action.setShortcut(QKeySequence.Redo)
        edit_menu.addAction(redo_action)
        
        edit_menu.addSeparator()
        
        clear_action = QAction("清空时间轴(&C)", self)
        clear_action.triggered.connect(self.clear_timeline)
        edit_menu.addAction(clear_action)
        
        # 工具菜单
        tools_menu = menubar.addMenu("工具(&T)")
        
        serial_settings_action = QAction("串口设置(&S)", self)
        serial_settings_action.triggered.connect(self.open_serial_settings)
        tools_menu.addAction(serial_settings_action)
        
        servo_settings_action = QAction("舵机设置(&M)", self)
        servo_settings_action.triggered.connect(self.open_motor_settings)
        tools_menu.addAction(servo_settings_action)
        
        tools_menu.addSeparator()
        
        default_params_action = QAction("默认参数配置(&P)", self)
        default_params_action.triggered.connect(self.open_default_params)
        tools_menu.addAction(default_params_action)
        
        tools_menu.addSeparator()
        
        set_start_pos_action = QAction("设置起始位置(&S)", self)
        set_start_pos_action.triggered.connect(self.set_start_positions)
        tools_menu.addAction(set_start_pos_action)
        
        # 帮助菜单
        help_menu = menubar.addMenu("帮助(&H)")
        
        about_action = QAction("关于(&A)", self)
        about_action.triggered.connect(self.show_about)
        help_menu.addAction(about_action)
    
    def create_toolbar(self):
        """创建工具栏"""
        toolbar = QToolBar("主工具栏")
        self.addToolBar(toolbar)
        
        # 当前串口显示
        toolbar.addWidget(QLabel("当前串口:"))
        self.current_port_label = QLabel("未选择")
        self.current_port_label.setStyleSheet("color: #666; padding: 2px 8px; min-width: 60px;")
        toolbar.addWidget(self.current_port_label)
        
        toolbar.addSeparator()
        
        # 连接/断开按钮
        self.connect_action = QAction("连接", self)
        self.connect_action.setCheckable(True)
        self.connect_action.triggered.connect(self.toggle_connection)
        toolbar.addAction(self.connect_action)
        
        toolbar.addSeparator()
        
        # 全部使能按钮
        self.enable_all_action = QAction("全部使能", self)
        self.enable_all_action.setCheckable(True)
        self.enable_all_action.triggered.connect(self.toggle_enable_all)
        toolbar.addAction(self.enable_all_action)
        
        toolbar.addSeparator()
        
        # 运行按钮
        self.run_action = QAction("运行", self)
        self.run_action.setEnabled(False)
        self.run_action.triggered.connect(self.run_program)
        toolbar.addAction(self.run_action)
        
        # 停止按钮
        self.stop_action = QAction("停止", self)
        self.stop_action.setEnabled(False)
        self.stop_action.triggered.connect(self.stop_program)
        toolbar.addAction(self.stop_action)
        
        # 暂停按钮
        self.pause_action = QAction("暂停", self)
        self.pause_action.setEnabled(False)
        self.pause_action.setCheckable(True)
        self.pause_action.triggered.connect(self.pause_program)
        toolbar.addAction(self.pause_action)
        
        toolbar.addSeparator()
        
        # 输出运动逻辑表按钮
        self.motion_table_action = QAction("输出舵机运动表", self)
        self.motion_table_action.triggered.connect(self.generate_motion_table)
        toolbar.addAction(self.motion_table_action)
        
        toolbar.addSeparator()
        
        # 生成命令序列按钮
        self.generate_action = QAction("生成命令序列", self)
        self.generate_action.triggered.connect(self.generate_command_preview)
        toolbar.addAction(self.generate_action)
    
    def create_status_bar(self):
        """创建状态栏"""
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        
        # 连接状态
        self.connection_label = QLabel("未连接")
        self.connection_label.setStyleSheet("color: red; font-weight: bold;")
        self.status_bar.addWidget(self.connection_label)
        
        # 分隔符
        self.status_bar.addWidget(QLabel("|"))
        
        # 运行状态
        self.running_label = QLabel("就绪")
        self.status_bar.addWidget(self.running_label)
        
        # 分隔符
        self.status_bar.addWidget(QLabel("|"))
        
        # 进度条
        self.progress_bar = QProgressBar()
        self.progress_bar.setVisible(False)
        self.status_bar.addPermanentWidget(self.progress_bar)
        
        # 时间显示
        self.time_label = QLabel("00:00:00")
        self.status_bar.addPermanentWidget(self.time_label)
    
    def create_dock_widgets(self):
        """创建停靠窗口 - 可浮动用于测试，但不可在主窗口内拖动"""
        
        # 右侧信息停靠窗口
        self.info_dock = QDockWidget("信息", self)
        info_widget = QWidget()
        info_layout = QVBoxLayout()
        info_layout.setContentsMargins(5, 5, 5, 5)
        
        # 创建选项卡
        self.info_tab_widget = QTabWidget()
        
        # 选项卡1: 命令序列
        self.gcode_text = QTextEdit()
        self.gcode_text.setReadOnly(True)
        self.gcode_text.setFont(QFont("Consolas", 9))
        self.info_tab_widget.addTab(self.gcode_text, "命令序列")
        
        # 选项卡2: 串口与舵机状态
        status_widget = QWidget()
        status_layout = QVBoxLayout()
        status_layout.setContentsMargins(5, 5, 5, 5)
        status_layout.setSpacing(10)
        
        # 串口信息组 - 设置固定高度，紧凑显示
        serial_group = QGroupBox("串口信息")
        serial_group.setMaximumHeight(100)  # 进一步限制最大高度
        serial_group.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Fixed)  # 固定高度
        serial_layout = QFormLayout()
        serial_layout.setSpacing(6)  # 减少间距
        serial_layout.setContentsMargins(8, 8, 8, 8)  # 减少边距
        
        self.port_label = QLabel("未连接")
        self.baud_label = QLabel("-")
        self.connection_status_label = QLabel("未连接")
        self.connection_status_label.setStyleSheet("color: red; font-weight: bold;")
        
        serial_layout.addRow("串口:", self.port_label)
        serial_layout.addRow("波特率:", self.baud_label)
        serial_layout.addRow("连接状态:", self.connection_status_label)
        
        serial_group.setLayout(serial_layout)
        status_layout.addWidget(serial_group)
        
        # 舵机状态组 - 允许拉伸，充分利用空间
        servo_group = QGroupBox("舵机状态")
        servo_group.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Expanding)  # 垂直方向允许拉伸
        servo_layout = QFormLayout()
        servo_layout.setSpacing(6)  # 减少间距
        servo_layout.setContentsMargins(8, 8, 8, 8)  # 减少边距
        servo_layout.setFieldGrowthPolicy(QFormLayout.ExpandingFieldsGrow)  # 字段可以扩展
        
        # 运行状态
        self.servo_state_label = QLabel("未知")
        self.servo_state_label.setStyleSheet("color: #666; font-weight: bold;")
        servo_layout.addRow("运行状态:", self.servo_state_label)
        
        # 舵机位置标签
        self.servo_labels = {}
        servo_names = [f'舵机{i}' for i in range(18)]  # 舵机0-17
        for servo in servo_names:
            label = QLabel("0.00°")
            label.setStyleSheet("color: #333; font-family: monospace;")
            self.servo_labels[servo] = label
            servo_layout.addRow(f"{servo}位置:", label)
        
        # 运动速度
        self.servo_speed_label = QLabel("0%")
        self.servo_speed_label.setStyleSheet("color: #333; font-family: monospace;")
        servo_layout.addRow("运动速度:", self.servo_speed_label)
        
        # 连接状态
        self.servo_connection_label = QLabel("未连接")
        self.servo_connection_label.setStyleSheet("color: #dc3545; font-weight: bold;")
        servo_layout.addRow("舵机连接:", self.servo_connection_label)
        
        servo_group.setLayout(servo_layout)
        status_layout.addWidget(servo_group, 1)  # 设置拉伸比例为1，让舵机状态区域获得更多空间
        
        status_widget.setLayout(status_layout)
        self.info_tab_widget.addTab(status_widget, "串口与舵机状态")
        
        info_layout.addWidget(self.info_tab_widget)
        info_widget.setLayout(info_layout)
        self.info_dock.setWidget(info_widget)
        
        # 设置默认显示的选项卡为"串口与舵机状态"（索引1）
        self.info_tab_widget.setCurrentIndex(1)
        
        # 设置停靠窗口特性：只允许浮动，不允许关闭和移动到其他位置
        self.info_dock.setFeatures(QDockWidget.DockWidgetFloatable)
        self.info_dock.setAllowedAreas(Qt.RightDockWidgetArea)  # 只允许停靠在右侧
        self.addDockWidget(Qt.RightDockWidgetArea, self.info_dock)
        
        # 底部日志停靠窗口
        self.log_dock = QDockWidget("日志", self)
        log_widget = QWidget()
        log_layout = QVBoxLayout()
        log_layout.setContentsMargins(5, 5, 5, 5)
        
        # 创建选项卡
        self.log_tab_widget = QTabWidget()
        
        # 选项卡1: 应用程序日志
        self.app_log_text = QTextEdit()
        self.app_log_text.setReadOnly(True)
        self.app_log_text.setFont(QFont("Consolas", 8))
        self.log_tab_widget.addTab(self.app_log_text, "应用程序日志")
        
        # 选项卡2: 串口通信日志
        serial_log_widget = QWidget()
        serial_log_layout = QVBoxLayout()
        serial_log_layout.setContentsMargins(0, 0, 0, 0)
        
        # 串口日志工具栏
        serial_toolbar = QHBoxLayout()
        serial_toolbar.setSpacing(5)
        
        # 清空日志按钮
        clear_log_btn = QPushButton("清空日志")
        clear_log_btn.clicked.connect(self.clear_serial_log)
        clear_log_btn.setStyleSheet("QPushButton { padding: 2px 8px; font-size: 10px; }")
        serial_toolbar.addWidget(clear_log_btn)
        
        # 保存日志按钮
        save_log_btn = QPushButton("保存日志")
        save_log_btn.clicked.connect(self.save_serial_log)
        save_log_btn.setStyleSheet("QPushButton { padding: 2px 8px; font-size: 10px; }")
        serial_toolbar.addWidget(save_log_btn)
        
        # 自动滚动复选框
        self.auto_scroll_checkbox = QCheckBox("自动滚动")
        self.auto_scroll_checkbox.setChecked(True)
        self.auto_scroll_checkbox.setStyleSheet("QCheckBox { font-size: 10px; }")
        serial_toolbar.addWidget(self.auto_scroll_checkbox)
        
        # 只显示协议帧复选框
        self.protocol_only_checkbox = QCheckBox("仅协议帧")
        self.protocol_only_checkbox.setChecked(False)
        self.protocol_only_checkbox.setStyleSheet("QCheckBox { font-size: 10px; }")
        self.protocol_only_checkbox.setToolTip("勾选后只显示协议帧(FF FE开头)，不显示调试信息")
        serial_toolbar.addWidget(self.protocol_only_checkbox)
        
        serial_toolbar.addStretch()
        
        # 连接状态指示
        self.serial_status_label = QLabel("未连接")
        self.serial_status_label.setStyleSheet("color: red; font-weight: bold; font-size: 10px;")
        serial_toolbar.addWidget(self.serial_status_label)
        
        serial_log_layout.addLayout(serial_toolbar)
        
        # 串口日志文本区域
        self.serial_log_text = QTextEdit()
        self.serial_log_text.setReadOnly(True)
        self.serial_log_text.setFont(QFont("Consolas", 9))
        self.serial_log_text.setAcceptRichText(True)  # 支持HTML格式
        serial_log_layout.addWidget(self.serial_log_text)
        
        serial_log_widget.setLayout(serial_log_layout)
        self.log_tab_widget.addTab(serial_log_widget, "串口通信日志")
        
        # 默认显示串口通信日志标签页
        self.log_tab_widget.setCurrentIndex(1)
        
        log_layout.addWidget(self.log_tab_widget)
        log_widget.setLayout(log_layout)
        self.log_dock.setWidget(log_widget)
        
        # 设置停靠窗口特性：只允许浮动，不允许关闭和移动到其他位置
        self.log_dock.setFeatures(QDockWidget.DockWidgetFloatable)
        self.log_dock.setAllowedAreas(Qt.BottomDockWidgetArea)  # 只允许停靠在底部
        
        # 设置日志窗口的高度限制
        self.log_dock.setMinimumHeight(200)  # 设置最小高度
        self.log_dock.setMaximumHeight(300)  # 设置最大高度
        
        self.addDockWidget(Qt.BottomDockWidgetArea, self.log_dock)
        
        # 舵机控制命令工具停靠窗口
        self.command_dock = QDockWidget("舵机控制命令", self)
        command_widget = self._create_command_panel()
        self.command_dock.setWidget(command_widget)
        
        # 设置停靠窗口特性：只允许浮动，不允许关闭和移动到其他位置
        self.command_dock.setFeatures(QDockWidget.DockWidgetFloatable)
        self.command_dock.setAllowedAreas(Qt.BottomDockWidgetArea)  # 只允许停靠在底部
        
        # 设置命令工具窗口的尺寸限制
        self.command_dock.setMinimumWidth(250)
        self.command_dock.setMaximumWidth(350)
        self.command_dock.setMinimumHeight(200)  # 设置最小高度
        self.command_dock.setMaximumHeight(300)  # 设置最大高度
        
        self.addDockWidget(Qt.BottomDockWidgetArea, self.command_dock)
        
        # 设置底部停靠窗口为水平分割
        self.splitDockWidget(self.log_dock, self.command_dock, Qt.Horizontal)
    
    def _create_command_panel(self):
        """创建舵机控制命令工具面板"""
        command_widget = QWidget()
        command_layout = QVBoxLayout()
        command_layout.setContentsMargins(6, 6, 6, 6)
        command_layout.setSpacing(4)
        
        # 统一按钮样式 - 普通样式,字体增大
        button_style = """
        QPushButton {
            background-color: #f0f0f0;
            border: 1px solid #ccc;
            border-radius: 4px;
            padding: 6px 10px;
            font-size: 13px;
            color: #333;
            min-height: 28px;
        }
        QPushButton:hover {
            background-color: #e0e0e0;
            border-color: #999;
        }
        QPushButton:pressed {
            background-color: #d0d0d0;
        }
        """
        
        
        # 标题
        title_label = QLabel("舵机控制命令")
        title_label.setStyleSheet("font-weight: bold; font-size: 12px; color: #1a1a1a; margin-bottom: 4px;")
        command_layout.addWidget(title_label)
        
        # 紧急命令
        emergency_row1 = QHBoxLayout()
        emergency_row1.setSpacing(4)
        
        self.emergency_stop_btn = QPushButton("紧急停止")
        self.emergency_stop_btn.clicked.connect(self.emergency_stop)
        self.emergency_stop_btn.setStyleSheet(button_style)
        emergency_row1.addWidget(self.emergency_stop_btn)
        
        self.status_query_btn = QPushButton("查询状态")
        self.status_query_btn.clicked.connect(self.query_status)
        self.status_query_btn.setStyleSheet(button_style)
        emergency_row1.addWidget(self.status_query_btn)
        
        command_layout.addLayout(emergency_row1)
        
        # 归零命令
        homing_row = QHBoxLayout()
        homing_row.setSpacing(4)
        
        self.home_all_btn = QPushButton("全部归零(90°)")
        self.home_all_btn.clicked.connect(self.home_all_servos)
        self.home_all_btn.setStyleSheet(button_style)
        homing_row.addWidget(self.home_all_btn)
        
        command_layout.addLayout(homing_row)
        
        # 自定义命令
        # 输入框样式
        input_style = """
        QLineEdit {
            background-color: #ffffff;
            border: 1px solid #cccccc;
            border-radius: 3px;
            padding: 4px 6px;
            font-size: 10px;
            color: #2c2c2c;
            min-height: 18px;
        }
        QLineEdit:focus {
            border-color: #4a90e2;
        }
        """
        
        # 自定义命令布局
        custom_layout = QHBoxLayout()
        custom_layout.setSpacing(4)
        
        self.custom_command_input = QLineEdit()
        self.custom_command_input.setPlaceholderText("输入自定义舵机命令...")
        self.custom_command_input.setStyleSheet(input_style)
        custom_layout.addWidget(self.custom_command_input)
        
        self.send_custom_btn = QPushButton("发送命令")
        self.send_custom_btn.clicked.connect(self.send_custom_command)
        self.send_custom_btn.setStyleSheet(button_style)
        custom_layout.addWidget(self.send_custom_btn)
        
        command_layout.addLayout(custom_layout)
        
        # 回车键发送命令
        self.custom_command_input.returnPressed.connect(self.send_custom_command)
        
        command_widget.setLayout(command_layout)
        return command_widget
    
    
    def setup_connections(self):
        """设置信号连接"""
        # 时间轴信号
        self.timeline_widget.component_dropped.connect(self.on_component_dropped)
        self.timeline_widget.component_selected.connect(self.on_component_selected)
        self.timeline_widget.component_moved.connect(self.on_component_moved)
        self.timeline_widget.component_deleted.connect(self.on_component_deleted)
        self.timeline_widget.servo_enable_clicked.connect(self.on_servo_enable_clicked)
        
        # 串口通信信号
        self.serial_comm.connected.connect(self.on_serial_connected)
        self.serial_comm.disconnected.connect(self.on_serial_disconnected)
        self.serial_comm.data_received.connect(self.on_serial_data_received)
        self.serial_comm.data_sent.connect(self.on_serial_data_sent)
        self.serial_comm.error_occurred.connect(self.on_serial_error)
        # 舵机控制不需要状态更新和程序结束信号
        
        logger.debug("[main_window] 串口通信信号已连接")
        logger.debug(f"[main_window] data_received连接数: {self.serial_comm.receivers(self.serial_comm.data_received)}")
        logger.debug(f"[main_window] data_sent连接数: {self.serial_comm.receivers(self.serial_comm.data_sent)}")
    
    def setup_logging(self):
        """设置日志系统"""
        # 创建日志处理器
        from PyQt5.QtCore import QObject, pyqtSignal
        
        class LogHandler(logging.Handler, QObject):
            log_message = pyqtSignal(str, int)
            
            def __init__(self):
                logging.Handler.__init__(self)
                QObject.__init__(self)
                # 只显示INFO及以上级别的日志，过滤掉DEBUG级别的状态报告
                self.setLevel(logging.INFO)
                self.setFormatter(logging.Formatter('%(asctime)s - %(levelname)s - %(message)s'))
            
            def emit(self, record):
                try:
                    msg = self.format(record)
                    self.log_message.emit(msg, record.levelno)
                except Exception:
                    self.handleError(record)
        
        # 添加UI日志处理器
        self.ui_handler = LogHandler()
        self.ui_handler.log_message.connect(self.add_log_message)
        
        # 获取motor_controller日志器并添加处理器
        motor_logger = logging.getLogger('motor_controller')
        motor_logger.addHandler(self.ui_handler)
    
    def center_window(self):
        """窗口居中显示"""
        from PyQt5.QtWidgets import QApplication
        screen = QApplication.desktop().screenGeometry()
        size = self.geometry()
        self.move(
            (screen.width() - size.width()) // 2,
            (screen.height() - size.height()) // 2
        )
    
    # 菜单动作处理
    def new_project(self):
        """新建项目"""
        if self.check_unsaved_changes():
            new_timeline_data = self.project_manager.create_new_project()
            self.timeline_widget.set_timeline_data(new_timeline_data)
            self.gcode_text.clear()
            self.update_window_title()
            logger.info("新建项目")
    
    def open_project(self):
        """打开项目"""
        if self.check_unsaved_changes():
            # 获取项目根目录（MotorControllerSystem的上级目录）
            import os
            project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
            
            filename, _ = QFileDialog.getOpenFileName(
                self, "打开项目", project_root, "项目文件 (*.json);;所有文件 (*)"
            )
            if filename:
                timeline_data = self.project_manager.load_project(filename)
                if timeline_data:
                    self.timeline_widget.set_timeline_data(timeline_data)
                    self.update_window_title()
                    logger.info(f"打开项目: {filename}")
    
    def save_project(self):
        """保存项目"""
        if self.project_manager.get_current_project_path():
            # 调试：检查TimelineData中的部件数量
            total_components = 0
            for i, track in enumerate(self.timeline_widget.timeline_data.tracks):
                track_components = len(track.components)
                total_components += track_components
                if track_components > 0:
                    logger.info(f"保存时电机{i}轨道有{track_components}个部件")
                    for j, comp in enumerate(track.components):
                        logger.info(f"  部件{j}: {comp.type.value}, 起始时间: {comp.start_time}, 持续时间: {comp.duration}")
            logger.info(f"保存时总共有{total_components}个部件")
            
            success = self.project_manager.save_project(
                self.timeline_widget.timeline_data, 
                self.project_manager.get_current_project_path()
            )
            if success:
                self.update_window_title()
                logger.info("项目已保存")
        else:
            self.save_as_project()
    
    def save_as_project(self):
        """另存为项目"""
        # 获取项目根目录（MotorControllerSystem的上级目录）
        import os
        project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        default_filename = os.path.join(project_root, f"project_{datetime.now().strftime('%Y%m%d_%H%M%S')}.json")
        
        filename, _ = QFileDialog.getSaveFileName(
            self, "另存为项目", default_filename, "项目文件 (*.json);;所有文件 (*)"
        )
        if filename:
            success = self.project_manager.save_as_project(self.timeline_widget.timeline_data, filename)
            if success:
                self.update_window_title()
                logger.info(f"项目已保存: {filename}")
    
    def export_commands(self):
        """导出命令序列"""
        # 获取项目根目录（ControlManagementSystem的上级目录）
        import os
        project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        default_filename = os.path.join(project_root, f"servo_commands_{datetime.now().strftime('%Y%m%d_%H%M%S')}.txt")
        
        filename, _ = QFileDialog.getSaveFileName(
            self, "导出命令序列", default_filename, "文本文件 (*.txt);;所有文件 (*)"
        )
        if filename:
            success = self.servo_commander.generate_command_file(filename)
            if success:
                QMessageBox.information(self, "导出成功", f"命令序列已导出到: {filename}")
    
    def clear_timeline(self):
        """清空时间轴"""
        reply = QMessageBox.question(
            self, "确认", "确定要清空时间轴吗？此操作不可撤销。",
            QMessageBox.Yes | QMessageBox.No, QMessageBox.No
        )
        if reply == QMessageBox.Yes:
            self.timeline_data.clear_all()
            self.timeline_widget.set_timeline_data(self.timeline_data)
            self.gcode_text.clear()
            logger.info("清空时间轴")
    
    def show_serial_settings(self):
        """显示串口设置"""
        dialog = SerialSettingsDialog(parent=self)
        if dialog.exec_() == QDialog.Accepted:
            # 应用串口设置
            pass
    
    # 舵机命令工具方法
    def emergency_stop(self):
        """紧急停止"""
        if self.is_connected:
            self.serial_comm.emergency_stop()
            logger.info("发送紧急停止命令")
        else:
            QMessageBox.warning(self, "警告", "请先连接串口")
    
    def home_all_servos(self):
        """所有舵机归零（回到90度中位）"""
        if self.is_connected:
            angles = [90.0] * 18  # 18个舵机
            self.serial_comm.move_all_servos(angles, 1000)
            logger.info("所有舵机归零到90度")
        else:
            QMessageBox.warning(self, "警告", "请先连接串口")
    
    def query_status(self):
        """查询舵机状态"""
        if self.is_connected:
            self.serial_comm.get_all_status()
            logger.info("查询舵机状态")
        else:
            QMessageBox.warning(self, "警告", "请先连接串口")
    
    def send_custom_command(self):
        """发送自定义命令"""
        command = self.custom_command_input.text().strip()
        if command:
            if self.is_connected:
                # 注意：这里需要根据实际协议处理自定义命令
                # 暂时保留，可能需要后续实现
                logger.info(f"自定义命令功能暂未实现: {command}")
                self.custom_command_input.clear()
            else:
                QMessageBox.warning(self, "警告", "请先连接串口")
        else:
            QMessageBox.warning(self, "警告", "请输入命令")
    
    def show_about(self):
        """显示关于对话框"""
        QMessageBox.about(self, "关于", 
            "18通道舵机时间轴控制上位机 v1.0.0\n\n"
            "基于PyQt5开发的可视化舵机控制软件\n"
            "支持Raspberry Pi Pico 2 + 18个PWM舵机（编号0-17）\n\n"
            "开发时间: 2025年"
        )
    
    # 工具栏动作处理
    def toggle_connection(self):
        """切换连接状态"""
        if not self.is_connected:
            # 尝试连接
            ports = self.serial_comm.get_available_ports()
            if not ports:
                QMessageBox.warning(self, "错误", "没有找到可用的串口")
                return
            
            # 如果有多个串口,让用户选择
            if len(ports) > 1:
                from PyQt5.QtWidgets import QInputDialog
                port_names = [f"{p['device']} - {p['description']}" for p in ports]
                port_name, ok = QInputDialog.getItem(
                    self, "选择串口", "请选择要连接的串口:", 
                    port_names, 0, False
                )
                if not ok:
                    return
                # 提取端口名 (COM3, COM4等)
                port_name = port_name.split(' - ')[0]
            else:
                # 只有一个串口,直接使用
                port_name = ports[0]['device']
            
            if self.serial_comm.connect(port_name):
                self.is_connected = True
                self.connect_action.setText("断开")
                self.connect_action.setChecked(True)
                self.run_action.setEnabled(True)
                self.connection_label.setText("已连接")
                self.connection_label.setStyleSheet("color: green; font-weight: bold;")
                # 更新工具栏的当前串口显示
                self.current_port_label.setText(port_name)
                self.current_port_label.setStyleSheet("color: #007bff; padding: 2px 8px; min-width: 60px; font-weight: bold;")
                logger.info(f"串口连接成功: {port_name}")
            else:
                QMessageBox.warning(self, "错误", "串口连接失败")
        else:
            # 断开连接
            self.serial_comm.disconnect()
            self.is_connected = False
            self.connect_action.setText("连接")
            self.connect_action.setChecked(False)
            self.run_action.setEnabled(False)
            self.connection_label.setText("未连接")
            self.connection_label.setStyleSheet("color: red; font-weight: bold;")
            # 更新工具栏的当前串口显示
            self.current_port_label.setText("未选择")
            self.current_port_label.setStyleSheet("color: #666; padding: 2px 8px; min-width: 60px;")
            logger.info("串口已断开")
    
    def toggle_enable_all(self):
        """切换全部使能状态"""
        if self.enable_all_action.isChecked():
            # 切换到全部使能模式
            self.enable_all_mode = True
            self.enable_all_action.setText("全部使能 ✓")
            
            # 使能所有舵机
            if self.is_connected:
                self.serial_comm.enable_servo(0xFF)  # 0xFF表示全部使能
                logger.info("全部使能模式：使能所有18个舵机")
            
            # 更新所有舵机轨道显示为使能状态
            for servo_id in range(18):
                self.servo_enable_states[servo_id] = True
                if servo_id in self.timeline_widget.motor_tracks:
                    self.timeline_widget.motor_tracks[servo_id].set_enable_state(True)
        else:
            # 切换到单个使能模式
            self.enable_all_mode = False
            self.enable_all_action.setText("全部使能")
            
            # 禁用所有舵机
            if self.is_connected:
                self.serial_comm.disable_servo(0xFF)  # 0xFF表示全部禁用
                logger.info("单个使能模式：禁用所有舵机，可单独使能")
            
            # 更新所有舵机轨道显示为禁用状态
            for servo_id in range(18):
                self.servo_enable_states[servo_id] = False
                if servo_id in self.timeline_widget.motor_tracks:
                    self.timeline_widget.motor_tracks[servo_id].set_enable_state(False)
    
    def run_program(self):
        """运行程序"""
        if not self.is_connected:
            QMessageBox.warning(self, "警告", "请先连接串口")
            return
        
        logger.info("开始执行舵机控制程序")
        
        # 生成命令序列
        sequence = self.servo_commander.generate_command_sequence(self.timeline_widget.timeline_data)
        
        if not sequence:
            QMessageBox.warning(self, "警告", "没有可执行的命令")
            return
        
        # 在后台线程执行命令序列
        import threading
        def execute_thread():
            self.is_running = True
            self.run_action.setEnabled(False)
            self.stop_action.setEnabled(True)
            self.running_label.setText("运行中")
            
            # 传入timeline_data以支持循环模式
            success = self.servo_commander.execute_sequence(
                self.serial_comm, 
                sequence, 
                self.timeline_widget.timeline_data
            )
            
            self.is_running = False
            self.run_action.setEnabled(True)
            self.stop_action.setEnabled(False)
            self.running_label.setText("就绪")
            
            if success:
                logger.info("程序执行完成")
            else:
                logger.error("程序执行失败")
        
        thread = threading.Thread(target=execute_thread, daemon=True)
        thread.start()
    
    def stop_program(self):
        """停止程序"""
        if self.is_connected:
            # 发送停止信号给servo_commander（退出循环）
            self.servo_commander.stop_execution()
            
            # 发送紧急停止命令到单片机
            self.serial_comm.emergency_stop()
            
            self.is_running = False
            self.run_action.setEnabled(True)
            self.stop_action.setEnabled(False)
            self.pause_action.setEnabled(False)
            self.pause_action.setChecked(False)
            self.running_label.setText("已停止")
            
            logger.info("程序已停止")
    
    # 舵机控制系统不需要程序结束回调
    # （舵机控制是同步的，不需要循环发送命令）
    
    def pause_program(self):
        """暂停/继续程序"""
        if self.pause_action.isChecked():
            # 暂停
            self.running_label.setText("已暂停")
            logger.info("程序已暂停")
        else:
            # 继续
            self.running_label.setText("运行中")
            logger.info("程序继续运行")
    
    def generate_command_preview(self):
        """生成命令序列预览"""
        preview_text = self.servo_commander.generate_preview_text(self.timeline_widget.timeline_data)
        self.gcode_text.setPlainText(preview_text)
        logger.info("命令序列已生成")
    
    # 信号处理
    def on_component_dropped(self, component_type: ComponentType, motor_id: int, start_time: float):
        """部件被拖拽到时间轴"""
        self.timeline_widget.add_component(component_type, motor_id, start_time)
        self.project_manager.set_modified(True)
        self.update_window_title()
    
    def on_component_selected(self, component_id: str):
        """部件被选中"""
        self.current_component_id = component_id
        component = self.timeline_data.get_component(component_id)
        if component:
            logger.info(f"部件被选中: {component.type.value}")
        else:
            logger.info("部件选择已清除")
    
    def on_component_moved(self, component_id: str, new_start_time: float, new_duration: float):
        """部件被移动"""
        component = self.timeline_data.get_component(component_id)
        if component:
            component.start_time = new_start_time
            component.duration = new_duration
            self.project_manager.set_modified(True)
            self.update_window_title()
    
    def on_component_deleted(self, component_id: str):
        """部件被删除"""
        self.timeline_widget.remove_component(component_id)
        self.project_manager.set_modified(True)
        self.update_window_title()
    
    def on_servo_enable_clicked(self, servo_id: int):
        """舵机使能点击处理"""
        if not self.is_connected:
            QMessageBox.warning(self, "警告", "请先连接串口")
            return
        
        if self.enable_all_mode:
            # 全部使能模式下，不允许单独使能
            QMessageBox.information(self, "提示", "当前为全部使能模式，请先关闭全部使能")
            return
        
        # 切换单个舵机的使能状态
        if servo_id < len(self.servo_enable_states):
            self.servo_enable_states[servo_id] = not self.servo_enable_states[servo_id]
            
            # 发送使能/禁用命令
            if self.servo_enable_states[servo_id]:
                self.serial_comm.enable_servo(servo_id)
                logger.info(f"使能舵机{servo_id}")
            else:
                self.serial_comm.disable_servo(servo_id)
                logger.info(f"禁用舵机{servo_id}")
            
            # 更新UI显示
            if servo_id in self.timeline_widget.motor_tracks:
                self.timeline_widget.motor_tracks[servo_id].set_enable_state(self.servo_enable_states[servo_id])
    
    def on_component_updated(self, component: Component):
        """部件参数更新"""
        self.timeline_widget.update_component(component)
        self.project_manager.set_modified(True)
        self.update_window_title()
    
    def on_serial_connected(self):
        """串口连接成功"""
        self.is_connected = True
        self.connect_action.setText("断开")
        self.run_action.setEnabled(True)
        self.connection_label.setText("已连接")
        self.connection_label.setStyleSheet("color: green; font-weight: bold;")
        
        # 更新主窗口状态显示
        self.port_label.setText(self.serial_comm.port_name)
        self.baud_label.setText(str(self.serial_comm.baud_rate))
        self.connection_status_label.setText("已连接")
        self.connection_status_label.setStyleSheet("color: green; font-weight: bold;")
        
        # 更新串口日志状态
        self.serial_status_label.setText("已连接")
        self.serial_status_label.setStyleSheet("color: green; font-weight: bold; font-size: 10px;")
        
        # 舵机控制系统不需要状态轮询
    
    def on_serial_disconnected(self):
        """串口断开"""
        self.is_connected = False
        self.connect_action.setText("连接")
        self.run_action.setEnabled(False)
        self.connection_label.setText("未连接")
        self.connection_label.setStyleSheet("color: red; font-weight: bold;")
        
        # 更新主窗口状态显示
        self.port_label.setText("未连接")
        self.baud_label.setText("-")
        self.connection_status_label.setText("未连接")
        self.connection_status_label.setStyleSheet("color: red; font-weight: bold;")
        
        # 更新串口日志状态
        self.serial_status_label.setText("未连接")
        self.serial_status_label.setStyleSheet("color: red; font-weight: bold; font-size: 10px;")
        
        # 舵机控制系统不需要状态轮询
    
    def on_serial_data_received(self, data: str):
        """串口数据接收"""
        logger.debug(f"[main_window] UI收到接收信号: {data[:50]}...")
        # 添加到串口通信日志 - 显示十六进制数据
        self.add_serial_log(f"← RX: {data}", "receive")
        logger.debug(f"[main_window] 串口日志添加完成")
    
    def on_serial_data_sent(self, data: str):
        """串口数据发送"""
        logger.debug(f"[main_window] UI收到发送信号: {data}")
        # 添加到串口通信日志 - 显示十六进制数据
        self.add_serial_log(f"→ TX: {data}", "send")
        logger.debug(f"[main_window] 串口日志添加完成")
    
    def on_serial_error(self, error: str):
        """串口错误"""
        # 添加到串口通信日志
        self.add_serial_log(f"错误: {error}", "error")
        QMessageBox.warning(self, "串口错误", error)
    
    # 舵机控制系统不需要状态更新回调
    # （如果将来需要显示舵机状态，可以在这里实现）
    
    def add_log_message(self, message: str, level: int):
        """添加日志消息"""
        # 添加到应用程序日志选项卡
        self.app_log_text.append(message)
        
        # 滚动到底部
        scrollbar = self.app_log_text.verticalScrollBar()
        scrollbar.setValue(scrollbar.maximum())
    
    def add_serial_log(self, message: str, msg_type: str = "info"):
        """添加串口通信日志 - 专业串口调试工具格式"""
        from datetime import datetime
        timestamp = datetime.now().strftime("%H:%M:%S.%f")[:-3]  # 毫秒精度
        
        logger.debug(f"[main_window] add_serial_log被调用: type={msg_type}, msg={message[:50]}...")
        
        # 如果勾选了"仅协议帧"，只显示包含协议帧的消息
        if self.protocol_only_checkbox.isChecked():
            # 检查是否是协议帧（包含 FF FE 或者是发送的TX消息）
            is_protocol = False
            if msg_type == "send":  # 发送的都是协议帧
                is_protocol = True
            elif "RX:" in message and "FF FE" in message:  # 接收的协议帧
                is_protocol = True
            
            if not is_protocol:
                return  # 不显示非协议帧消息
        
        # 根据消息类型设置不同的显示格式
        if msg_type == "send":
            # 发送数据 - 绿色
            formatted_message = f'<span style="color: #00AA00;">[{timestamp}] {message}</span>'
        elif msg_type == "receive":
            # 接收数据 - 蓝色
            formatted_message = f'<span style="color: #0066CC;">[{timestamp}] {message}</span>'
        elif msg_type == "error":
            # 错误信息 - 红色
            formatted_message = f'<span style="color: #CC0000;">[{timestamp}] {message}</span>'
        elif msg_type == "status":
            # 状态信息 - 橙色
            formatted_message = f'<span style="color: #FF6600;">[{timestamp}] {message}</span>'
        else:
            # 普通信息 - 黑色
            formatted_message = f'<span style="color: #000000;">[{timestamp}] {message}</span>'
        
        logger.debug(f"[main_window] 准备append到serial_log_text")
        
        try:
            self.serial_log_text.append(formatted_message)
            logger.debug(f"[main_window] append完成")
        except Exception as e:
            logger.error(f"[main_window] append失败: {e}")
            logger.error(f"[main_window] formatted_message长度: {len(formatted_message)}")
            logger.error(f"[main_window] formatted_message[:100]: {formatted_message[:100]}")
        
        # 滚动到底部（如果启用了自动滚动）
        if self.auto_scroll_checkbox.isChecked():
            try:
                scrollbar = self.serial_log_text.verticalScrollBar()
                scrollbar.setValue(scrollbar.maximum())
            except Exception as e:
                logger.error(f"[main_window] 滚动失败: {e}")
    
    def clear_serial_log(self):
        """清空串口日志"""
        self.serial_log_text.clear()
        logger.info("串口日志已清空")
    
    def save_serial_log(self):
        """保存串口日志到文件"""
        from PyQt5.QtWidgets import QFileDialog
        from datetime import datetime
        
        # 生成默认文件名
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        default_filename = f"serial_log_{timestamp}.txt"
        
        filename, _ = QFileDialog.getSaveFileName(
            self, "保存串口日志", default_filename, 
            "文本文件 (*.txt);;所有文件 (*)"
        )
        
        if filename:
            try:
                # 获取纯文本内容（去掉HTML标签）
                plain_text = self.serial_log_text.toPlainText()
                with open(filename, 'w', encoding='utf-8') as f:
                    f.write(plain_text)
                logger.info(f"串口日志已保存到: {filename}")
                QMessageBox.information(self, "保存成功", f"串口日志已保存到:\n{filename}")
            except Exception as e:
                logger.error(f"保存串口日志失败: {e}")
                QMessageBox.warning(self, "保存失败", f"保存串口日志失败:\n{e}")
    
    def check_unsaved_changes(self) -> bool:
        """检查未保存的更改"""
        if self.project_manager.has_unsaved_changes():
            reply = QMessageBox.question(
                self, "未保存的更改", 
                "当前项目有未保存的更改，是否保存？",
                QMessageBox.Yes | QMessageBox.No | QMessageBox.Cancel,
                QMessageBox.Yes
            )
            if reply == QMessageBox.Yes:
                self.save_project()
                return True
            elif reply == QMessageBox.No:
                return True
            else:
                return False
        return True
    
    def update_window_title(self):
        """更新窗口标题"""
        title = "18通道舵机时间轴控制上位机 v1.0.0"
        if self.project_manager.get_current_project_path():
            filename = os.path.basename(self.project_manager.get_current_project_path())
            title += f" - {filename}"
            if self.project_manager.has_unsaved_changes():
                title += " *"
        self.setWindowTitle(title)
    
    def open_serial_settings(self):
        """打开串口设置对话框"""
        from ui.dialogs import SerialSettingsDialog
        
        # 获取当前串口设置
        current_settings = {
            'port': getattr(self.serial_comm, 'current_port', ''),
            'baudrate': getattr(self.serial_comm, 'baudrate', 115200),
            'timeout': getattr(self.serial_comm, 'timeout', 1.0)
        }
        
        dialog = SerialSettingsDialog(current_settings, self)
        if dialog.exec_() == QDialog.Accepted:
            settings = dialog.get_settings()
            # 更新串口设置
            self.serial_comm.baudrate = settings.get('baudrate', 115200)
            self.serial_comm.timeout = settings.get('timeout', 1.0)
            
            # 如果当前已连接，重新连接以应用新设置
            if self.is_connected:
                current_port = getattr(self.serial_comm, 'current_port', '')
                if current_port:
                    self.serial_comm.disconnect()
                    if self.serial_comm.connect(current_port):
                        # 更新工具栏串口显示
                        self.current_port_label.setText(current_port)
                        self.current_port_label.setStyleSheet("color: #007bff; padding: 2px 8px; min-width: 60px; font-weight: bold;")
                        logger.info("串口设置已更新并重新连接")
                    else:
                        self.is_connected = False
                        self.connect_action.setText("连接")
                        self.connect_action.setChecked(False)
                        self.run_action.setEnabled(False)
                        self.connection_label.setText("未连接")
                        self.connection_label.setStyleSheet("color: red; font-weight: bold;")
                        self.current_port_label.setText("未选择")
                        self.current_port_label.setStyleSheet("color: #666; padding: 2px 8px; min-width: 60px;")
                        QMessageBox.warning(self, "错误", "重新连接失败")
            else:
                logger.info("串口设置已更新")
    
    def open_motor_settings(self):
        """打开舵机参数设置对话框"""
        dialog = ServoSettingsDialog(self)
        dialog.set_settings(self.servo_settings)
        
        if dialog.exec_() == QDialog.Accepted:
            self.servo_settings = dialog.get_settings()
            logger.info(f"舵机参数已更新: 角度范围={self.servo_settings['angle_min']}-{self.servo_settings['angle_max']}°")
            QMessageBox.information(
                self, "设置成功",
                f"舵机参数已保存:\n\n"
                f"舵机数量: {self.servo_settings['servo_count']}个\n"
                f"角度范围: {self.servo_settings['angle_min']}° - {self.servo_settings['angle_max']}°\n"
                f"默认运动时间: {self.servo_settings['default_speed_ms']}ms\n\n"
                f"所有舵机将使用这些参数进行控制"
            )
            
            # 同时保存到配置文件
            self.config_manager.set_servo_settings({
                'servo_count': self.servo_settings['servo_count'],
                'angle_min': self.servo_settings['angle_min'],
                'angle_max': self.servo_settings['angle_max'],
                'default_speed_ms': self.servo_settings['default_speed_ms']
            })
            self.config_manager.save()
    
    def set_start_positions(self):
        """设置起始位置"""
        if not self.is_connected:
            QMessageBox.warning(self, "警告", "请先连接设备！")
            return
        
        # 导入对话框
        from ui.dialogs import StartPositionsDialog
        
        # 打开设置对话框
        dialog = StartPositionsDialog(parent=self)
        if dialog.exec_() == QDialog.Accepted:
            angles = dialog.get_angles()
            
            # 确认对话框
            reply = QMessageBox.question(
                self, 
                "确认设置", 
                "确定要将这些角度保存为起始位置吗？\n"
                "重启后所有舵机将恢复到设置的位置。",
                QMessageBox.Yes | QMessageBox.No,
                QMessageBox.No
            )
            
            if reply == QMessageBox.Yes:
                if self.serial_comm.set_start_positions(angles):
                    QMessageBox.information(
                        self, 
                        "成功", 
                        "起始位置已保存到Flash！\n重启后生效。"
                    )
                    logger.info("起始位置已保存到Flash")
                else:
                    QMessageBox.critical(self, "失败", "保存失败！")
                    logger.error("保存起始位置失败")
    
    def open_default_params(self):
        """打开默认参数配置对话框"""
        from ui.dialogs import DefaultParametersDialog
        
        dialog = DefaultParametersDialog(self.config_manager, self)
        if dialog.exec_() == QDialog.Accepted:
            # 重新加载舵机参数
            servo_config = self.config_manager.get_servo_settings()
            servo_count = servo_config.get('servo_count', 18)
            angle_min = servo_config.get('angle_min', 0.0)
            angle_max = servo_config.get('angle_max', 180.0)
            default_speed_ms = servo_config.get('default_speed_ms', 1000)
            
            self.servo_settings = {
                'servo_count': servo_count,
                'angle_min': angle_min,
                'angle_max': angle_max,
                'default_speed_ms': default_speed_ms
            }
            
            logger.info("默认参数已更新")
            QMessageBox.information(self, "配置已更新", "默认参数配置已应用,新建部件时将使用新的默认值")
    
    def generate_motion_table(self):
        """生成舵机运动表"""
        try:
            # 获取所有轨道的运动数据
            motion_data = self.timeline_widget.get_motion_table_data()
            
            # 显示在对话框中
            from ui.dialogs import MotionTableDialog
            dialog = MotionTableDialog(motion_data, self)
            dialog.exec_()
            
            logger.info("舵机运动表已生成")
            
        except Exception as e:
            logger.error(f"生成舵机运动表失败: {e}")
            from PyQt5.QtWidgets import QMessageBox
            QMessageBox.warning(self, "错误", f"生成舵机运动表失败: {e}")
    
    def _format_motion_table(self, motion_data: dict) -> str:
        """格式化舵机运动表"""
        text = "舵机运动逻辑表\n"
        text += "=" * 50 + "\n\n"
        
        # 获取舵机数量（默认18个）
        servo_count = self.servo_settings.get('servo_count', 18)
        
        for motor_id in range(servo_count):
            track_data = motion_data.get(motor_id, {})
            motor_name = track_data.get('name', f'舵机{motor_id}')
            loop_mode = track_data.get('loop_mode', '单次')
            components = track_data.get('components', [])
            
            text += f"【{motor_name}】\n"
            text += f"执行模式: {loop_mode}\n"
            
            if not components:
                text += "  无运动指令\n"
            else:
                text += "  运动序列:\n"
                for i, comp in enumerate(components, 1):
                    comp_type = comp['type']
                    start_time = comp['start_time']
                    duration = comp['duration']
                    end_time = start_time + duration
                    parameters = comp.get('parameters', {})
                    
                    text += f"    {i}. {comp_type}\n"
                    text += f"       时间: {start_time:.2f}s - {end_time:.2f}s (持续{duration:.2f}s)\n"
                    
                    # 添加参数信息
                    if comp_type == "正转" or comp_type == "反转":
                        angle = parameters.get('target_angle', 0)
                        speed_ms = parameters.get('speed_ms', 1000)
                        text += f"       角度: {angle:.2f}°\n"
                        text += f"       运动时间: {speed_ms}ms\n"
                    elif comp_type == "延时":
                        delay_time = parameters.get('delay_time', 0)
                        text += f"       延时时长: {delay_time:.2f}s\n"
                    elif comp_type == "归零":
                        text += f"       归零到原点位置\n"
                    elif comp_type == "停止":
                        text += f"       停止运动\n"
                    
                    text += "\n"
            
            text += "-" * 30 + "\n\n"
        
        # 添加时间轴总览
        text += "【时间轴总览】\n"
        all_components = []
        for motor_id in range(18):
            track_data = motion_data.get(motor_id, {})
            components = track_data.get('components', [])
            for comp in components:
                comp['motor_id'] = motor_id
                comp['motor_name'] = track_data.get('name', f'舵机{motor_id}')
                all_components.append(comp)
        
        # 按时间排序
        all_components.sort(key=lambda x: x['start_time'])
        
        if all_components:
            text += "按时间顺序执行:\n"
            for i, comp in enumerate(all_components, 1):
                start_time = comp['start_time']
                text += f"  {start_time:.2f}s: {comp['motor_name']} - {comp['type']}\n"
        else:
            text += "无运动指令\n"
        
        return text
    
    def closeEvent(self, event):
        """窗口关闭事件"""
        if self.check_unsaved_changes():
            # 断开串口连接
            if self.is_connected:
                self.serial_comm.disconnect()
            
            # 移除日志处理器，避免退出时的异常
            try:
                motor_logger = logging.getLogger('motor_controller')
                if hasattr(self, 'ui_handler'):
                    motor_logger.removeHandler(self.ui_handler)
            except:
                pass
            
            event.accept()
        else:
            event.ignore()

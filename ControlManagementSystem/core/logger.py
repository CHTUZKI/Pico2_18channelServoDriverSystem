#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
日志系统模块
"""

import logging
import os
from datetime import datetime
from PyQt5.QtCore import QObject, pyqtSignal

class LogHandler(QObject):
    """日志信号处理器，用于在UI中显示日志"""
    log_message = pyqtSignal(str, int)  # 消息内容, 日志级别
    
    def __init__(self):
        super().__init__()
        self.logger = logging.getLogger('servo_controller')
    
    def emit(self, record):
        """发送日志信号到UI"""
        msg = self.format(record)
        self.log_message.emit(msg, record.levelno)

def setup_logger():
    """设置双日志系统 - 分离应用日志和串口通信日志"""
    # 创建logs目录
    log_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'logs')
    os.makedirs(log_dir, exist_ok=True)
    
    # ========== 1. 应用日志（上位机操作） ==========
    app_log_file = os.path.join(log_dir, 'app_latest.log')
    if os.path.exists(app_log_file):
        try:
            os.remove(app_log_file)
        except:
            pass
    
    app_logger = logging.getLogger('app_control')
    app_logger.setLevel(logging.DEBUG)
    for handler in app_logger.handlers[:]:
        app_logger.removeHandler(handler)
    
    app_handler = logging.FileHandler(app_log_file, encoding='utf-8', mode='w')
    app_handler.setLevel(logging.DEBUG)
    app_formatter = logging.Formatter(
        '%(asctime)s.%(msecs)03d - %(levelname)-7s - %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S'
    )
    app_handler.setFormatter(app_formatter)
    app_logger.addHandler(app_handler)
    
    # 应用日志启动信息
    app_logger.info("=" * 80)
    app_logger.info("舵机控制系统 - 应用操作日志")
    app_logger.info(f"时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    app_logger.info("日志文件: app_latest.log (覆盖模式)")
    app_logger.info("=" * 80)
    
    # ========== 2. 串口通信日志（收发数据） ==========
    serial_log_file = os.path.join(log_dir, 'serial_latest.log')
    if os.path.exists(serial_log_file):
        try:
            os.remove(serial_log_file)
        except:
            pass
    
    serial_logger = logging.getLogger('serial_comm')
    serial_logger.setLevel(logging.DEBUG)
    for handler in serial_logger.handlers[:]:
        serial_logger.removeHandler(handler)
    
    serial_handler = logging.FileHandler(serial_log_file, encoding='utf-8', mode='w')
    serial_handler.setLevel(logging.DEBUG)
    serial_formatter = logging.Formatter(
        '%(asctime)s.%(msecs)03d - %(message)s',
        datefmt='%H:%M:%S'
    )
    serial_handler.setFormatter(serial_formatter)
    serial_logger.addHandler(serial_handler)
    
    # 串口日志启动信息
    serial_logger.info("=" * 80)
    serial_logger.info(f"串口通信日志 - {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    serial_logger.info("=" * 80)
    
    # 兼容旧代码，返回应用logger
    return app_logger

def get_logger():
    """获取应用日志器实例"""
    return logging.getLogger('app_control')

def get_serial_logger():
    """获取串口通信日志器实例"""
    return logging.getLogger('serial_comm')


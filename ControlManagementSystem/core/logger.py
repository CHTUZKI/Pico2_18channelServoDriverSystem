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
    """设置日志系统 - 固定文件名，每次覆盖旧日志"""
    # 创建logs目录
    log_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'logs')
    os.makedirs(log_dir, exist_ok=True)
    
    # 固定日志文件名（每次运行覆盖）
    log_file = os.path.join(log_dir, 'servo_control_latest.log')
    
    # 删除旧日志文件
    if os.path.exists(log_file):
        try:
            os.remove(log_file)
        except:
            pass
    
    # 配置根日志器
    logger = logging.getLogger('servo_controller')
    logger.setLevel(logging.DEBUG)
    
    # 清除已有的处理器
    for handler in logger.handlers[:]:
        logger.removeHandler(handler)
    
    # 文件处理器（记录所有DEBUG级别的详细信息）
    file_handler = logging.FileHandler(log_file, encoding='utf-8', mode='w')
    file_handler.setLevel(logging.DEBUG)
    file_formatter = logging.Formatter(
        '%(asctime)s.%(msecs)03d - %(levelname)-7s - %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S'
    )
    file_handler.setFormatter(file_formatter)
    logger.addHandler(file_handler)
    
    # 记录日志启动信息
    logger.info("=" * 80)
    logger.info("舵机控制系统日志 - 启动")
    logger.info(f"时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    logger.info("日志文件: servo_control_latest.log (覆盖模式)")
    logger.info("=" * 80)
    
    return logger

def get_logger():
    """获取日志器实例"""
    return logging.getLogger('servo_controller')


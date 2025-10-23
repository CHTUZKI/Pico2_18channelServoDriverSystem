#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
18轴舵机时间轴控制上位机
主程序入口
"""

import sys
import os
from PyQt5.QtWidgets import QApplication
from PyQt5.QtCore import Qt
from PyQt5.QtGui import QIcon

# 添加项目根目录到Python路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from ui.main_window import MainWindow
from core.logger import setup_logger

def main():
    """主函数"""
    # 创建应用程序
    app = QApplication(sys.argv)
    app.setApplicationName("18轴舵机时间轴控制上位机")
    app.setApplicationVersion("1.0.0")
    app.setOrganizationName("ServoController")
     
    # 设置应用程序属性（必须在创建QApplication之前设置）
    # app.setAttribute(Qt.AA_EnableHighDpiScaling, True)
    # app.setAttribute(Qt.AA_UseHighDpiPixmaps, True)
     
    # 设置日志
    logger = setup_logger()
    logger.info("应用程序启动")
    
    try:
        # 创建主窗口
        main_window = MainWindow()
        main_window.show()
        
        # 运行应用程序
        sys.exit(app.exec_())
        
    except Exception as e:
        logger.error(f"应用程序启动失败: {e}")
        sys.exit(1)

if __name__ == "__main__":
    main()

  
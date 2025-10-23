#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
诊断脚本 - 检查所有导入和依赖
"""

import sys
import os

print("=" * 60)
print("8轴电机时间轴控制上位机 - 诊断工具")
print("=" * 60)
print()

# 1. 检查Python版本
print("1. Python版本:")
print(f"   {sys.version}")
print()

# 2. 检查PyQt5
print("2. 检查PyQt5...")
try:
    from PyQt5 import QtCore
    print(f"   [OK] PyQt5 已安装 (版本: {QtCore.PYQT_VERSION_STR})")
except ImportError as e:
    print(f"   [ERROR] PyQt5 导入失败: {e}")
    sys.exit(1)

# 3. 检查pyserial
print("\n3. 检查pyserial...")
try:
    import serial
    print(f"   [OK] pyserial 已安装")
except ImportError as e:
    print(f"   [ERROR] pyserial 导入失败: {e}")

# 4. 检查各个模块导入
print("\n4. 检查项目模块...")
modules_to_check = [
    ("models.component", "部件数据模型"),
    ("models.timeline_data", "时间轴数据模型"),
    ("core.logger", "日志系统"),
    ("core.gcode_generator", "G代码生成器"),
    ("core.serial_comm", "串口通信"),
    ("core.project_manager", "项目管理器"),
]

for module_name, desc in modules_to_check:
    try:
        __import__(module_name)
        print(f"   [OK] {desc} ({module_name})")
    except Exception as e:
        print(f"   [ERROR] {desc} ({module_name}): {e}")

# 5. 检查UI模块
print("\n5. 检查UI模块...")
ui_modules = [
    ("ui.component_palette", "部件面板"),
    ("ui.motor_track", "电机轨道"),
    ("ui.timeline_widget", "时间轴组件"),
    ("ui.dialogs", "对话框"),
    ("ui.main_window", "主窗口"),
]

for module_name, desc in ui_modules:
    try:
        __import__(module_name)
        print(f"   [OK] {desc} ({module_name})")
    except Exception as e:
        print(f"   [ERROR] {desc} ({module_name}):")
        print(f"      错误: {e}")
        import traceback
        traceback.print_exc()

# 6. 尝试创建Qt应用程序
print("\n6. 尝试创建Qt应用程序...")
try:
    from PyQt5.QtWidgets import QApplication
    app = QApplication(sys.argv)
    print("   [OK] Qt应用程序创建成功")
    
    # 7. 尝试创建主窗口
    print("\n7. 尝试创建主窗口...")
    try:
        from ui.main_window import MainWindow
        window = MainWindow()
        print("   [OK] 主窗口创建成功")
        print("\n" + "=" * 60)
        print("诊断完成！所有模块正常，现在显示主窗口...")
        print("=" * 60)
        window.show()
        sys.exit(app.exec_())
    except Exception as e:
        print(f"   [ERROR] 主窗口创建失败: {e}")
        import traceback
        traceback.print_exc()
        
except Exception as e:
    print(f"   [ERROR] Qt应用程序创建失败: {e}")
    import traceback
    traceback.print_exc()

print("\n" + "=" * 60)
print("诊断结束")
print("=" * 60)

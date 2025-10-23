#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
AI调试助手 - 快速查看和分析日志
"""

import os
import sys

# 添加父目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(__file__)))

from tools.log_analyzer import LogAnalyzer


def quick_check():
    """快速检查 - 最常用的调试命令"""
    analyzer = LogAnalyzer()
    
    print("\n" + "=" * 100)
    print("[AI DEBUG] 快速检查")
    print("=" * 100)
    
    # 1. 统计信息
    stats = analyzer.analyze_timeline()
    print("\n[STATS] 统计信息:")
    for key, value in stats.items():
        print(f"  {key}: {value}")
    
    # 2. 错误检查
    errors = analyzer.search_error()
    if errors:
        print(f"\n[ERROR] 发现 {len(errors)} 个错误:")
        for line_num, content in errors[-10:]:  # 最后10个错误
            print(f"  [{line_num:5d}] {content}")
    else:
        print("\n[OK] 没有错误")
    
    # 3. 最后的操作
    print("\n[LAST] 最后20行:")
    for line in analyzer.get_last_n_lines(20):
        print(f"  {line}")
    
    print("=" * 100)


def search_keyword(keyword: str, max_lines: int = 30):
    """搜索关键词"""
    analyzer = LogAnalyzer()
    results = analyzer.search(keyword)
    
    print(f"\n[SEARCH] 搜索 '{keyword}' - 找到 {len(results)} 条记录")
    print("=" * 100)
    
    for line_num, content in results[:max_lines]:
        print(f"[{line_num:5d}] {content}")
    
    if len(results) > max_lines:
        print(f"\n... 还有 {len(results) - max_lines} 条记录")
    print("=" * 100)


def check_serial():
    """检查串口通信"""
    analyzer = LogAnalyzer()
    
    print("\n[SERIAL] 串口通信检查")
    print("=" * 100)
    
    # 检查连接
    connect_logs = analyzer.search("串口连接|串口已断开")
    print(f"\n连接/断开记录 ({len(connect_logs)} 条):")
    for line_num, content in connect_logs:
        print(f"  [{line_num:5d}] {content}")
    
    # 检查状态报告
    status_reports = analyzer.search_status_report()
    print(f"\n状态报告 ({len(status_reports)} 条):")
    if status_reports:
        # 只显示前3条和后3条
        for line_num, content in status_reports[:3]:
            print(f"  [{line_num:5d}] {content}")
        if len(status_reports) > 6:
            print(f"  ... 省略 {len(status_reports) - 6} 条 ...")
        for line_num, content in status_reports[-3:]:
            print(f"  [{line_num:5d}] {content}")
    
    print("=" * 100)


def check_gcode():
    """检查G代码生成和发送"""
    analyzer = LogAnalyzer()
    
    print("\n[GCODE] G代码检查")
    print("=" * 100)
    
    gcode_logs = analyzer.search_gcode()
    print(f"\nG代码相关记录 ({len(gcode_logs)} 条):")
    for line_num, content in gcode_logs:
        print(f"  [{line_num:5d}] {content}")
    
    print("=" * 100)


def tail(n: int = 50):
    """查看最后N行"""
    analyzer = LogAnalyzer()
    
    print(f"\n[TAIL] 最后 {n} 行")
    print("=" * 100)
    
    for line in analyzer.get_last_n_lines(n):
        print(line)
    
    print("=" * 100)


if __name__ == '__main__':
    if len(sys.argv) == 1:
        quick_check()
    elif sys.argv[1] == 'serial':
        check_serial()
    elif sys.argv[1] == 'gcode':
        check_gcode()
    elif sys.argv[1] == 'tail':
        n = int(sys.argv[2]) if len(sys.argv) > 2 else 50
        tail(n)
    elif sys.argv[1] == 'search':
        if len(sys.argv) > 2:
            search_keyword(sys.argv[2])
        else:
            print("[ERROR] 请提供搜索关键词")
    else:
        print("[USAGE] python ai_debug.py [serial|gcode|tail [N]|search <关键词>]")


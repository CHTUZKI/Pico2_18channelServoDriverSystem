#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
日志分析工具 - 供AI快速查找和分析调试信息
"""

import os
import re
from typing import List, Dict, Tuple

class LogAnalyzer:
    """日志分析器"""
    
    def __init__(self, log_file: str = None):
        if log_file is None:
            # 默认使用最新的日志文件
            log_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), 'logs')
            log_file = os.path.join(log_dir, 'motor_control_latest.log')
        
        self.log_file = log_file
        self.lines = []
        self.load_log()
    
    def load_log(self):
        """加载日志文件"""
        if os.path.exists(self.log_file):
            with open(self.log_file, 'r', encoding='utf-8') as f:
                self.lines = f.readlines()
            print(f"[OK] 已加载日志文件: {self.log_file} ({len(self.lines)} 行)")
        else:
            print(f"[ERROR] 日志文件不存在: {self.log_file}")
    
    def search(self, keyword: str, case_sensitive: bool = False) -> List[Tuple[int, str]]:
        """搜索关键词"""
        results = []
        flags = 0 if case_sensitive else re.IGNORECASE
        
        for i, line in enumerate(self.lines, 1):
            if re.search(keyword, line, flags):
                results.append((i, line.strip()))
        
        return results
    
    def search_error(self) -> List[Tuple[int, str]]:
        """搜索所有错误"""
        return self.search(r'ERROR|Error|错误|失败|异常|Exception')
    
    def search_warning(self) -> List[Tuple[int, str]]:
        """搜索所有警告"""
        return self.search(r'WARNING|Warning|警告')
    
    def search_status_report(self) -> List[Tuple[int, str]]:
        """搜索状态报告"""
        return self.search(r'<Idle|<Run|<Alarm|<Hold')
    
    def search_gcode(self) -> List[Tuple[int, str]]:
        """搜索G代码相关"""
        return self.search(r'G0|G1|G4|M30|发送G代码')
    
    def search_component(self, component_type: str = None) -> List[Tuple[int, str]]:
        """搜索组件操作"""
        if component_type:
            return self.search(f'{component_type}')
        return self.search(r'添加部件|删除部件|更新部件|正转|反转|延时|停止')
    
    def get_last_n_lines(self, n: int = 100) -> List[str]:
        """获取最后N行"""
        return [line.strip() for line in self.lines[-n:]]
    
    def get_lines_range(self, start: int, end: int) -> List[str]:
        """获取指定范围的行"""
        return [line.strip() for line in self.lines[start-1:end]]
    
    def filter_by_level(self, level: str) -> List[Tuple[int, str]]:
        """按日志级别过滤"""
        return self.search(f' - {level} - ')
    
    def print_results(self, results: List[Tuple[int, str]], max_lines: int = 50):
        """打印搜索结果"""
        print(f"\n[RESULTS] 找到 {len(results)} 条匹配记录:")
        print("=" * 100)
        
        for i, (line_num, content) in enumerate(results[:max_lines], 1):
            print(f"[{line_num:5d}] {content}")
        
        if len(results) > max_lines:
            print(f"\n... 还有 {len(results) - max_lines} 条记录未显示")
        print("=" * 100)
    
    def analyze_timeline(self) -> Dict[str, int]:
        """分析操作时间线"""
        stats = {
            '总行数': len(self.lines),
            '错误数': len(self.search_error()),
            '警告数': len(self.search_warning()),
            '状态报告数': len(self.search_status_report()),
            'G代码操作数': len(self.search_gcode()),
            '组件操作数': len(self.search_component())
        }
        return stats
    
    def quick_summary(self):
        """快速摘要"""
        stats = self.analyze_timeline()
        print("\n" + "=" * 100)
        print("[SUMMARY] 日志快速摘要")
        print("=" * 100)
        for key, value in stats.items():
            print(f"  {key}: {value}")
        print("=" * 100)
        
        # 显示最后的错误
        errors = self.search_error()
        if errors:
            print("\n[WARNING] 最近的错误:")
            self.print_results(errors[-5:], max_lines=5)
        else:
            print("\n[OK] 没有错误记录")
        
        # 显示最后几行
        print("\n[LAST] 最后10行:")
        print("-" * 100)
        for line in self.get_last_n_lines(10):
            print(line)
        print("-" * 100)

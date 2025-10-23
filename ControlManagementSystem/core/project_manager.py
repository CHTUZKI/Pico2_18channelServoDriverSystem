#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
项目管理器
负责项目的保存和加载
"""

import json
import os
from datetime import datetime
from typing import Dict, Any, Optional
from models.timeline_data import TimelineData
import logging

logger = logging.getLogger('motor_controller')

class ProjectManager:
    """项目管理器"""
    
    def __init__(self):
        self.current_project_path: Optional[str] = None
        self.project_modified = False
        
    def create_new_project(self) -> TimelineData:
        """创建新项目"""
        logger.info("创建新项目")
        self.current_project_path = None
        self.project_modified = False
        return TimelineData()
    
    def save_project(self, timeline_data: TimelineData, file_path: str) -> bool:
        """保存项目到文件"""
        try:
            # 准备项目数据
            project_data = {
                'version': '1.0.0',
                'created_time': datetime.now().isoformat(),
                'modified_time': datetime.now().isoformat(),
                'timeline_data': timeline_data.to_dict()
            }
            
            # 确保目录存在
            dir_path = os.path.dirname(file_path)
            if dir_path:
                os.makedirs(dir_path, exist_ok=True)
            
            # 保存到文件
            with open(file_path, 'w', encoding='utf-8') as f:
                json.dump(project_data, f, ensure_ascii=False, indent=2)
            
            self.current_project_path = file_path
            self.project_modified = False
            
            logger.info(f"项目已保存: {file_path}")
            return True
            
        except Exception as e:
            logger.error(f"保存项目失败: {e}")
            return False
    
    def load_project(self, file_path: str) -> Optional[TimelineData]:
        """从文件加载项目"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                project_data = json.load(f)
            
            # 检查版本兼容性
            version = project_data.get('version', '1.0.0')
            if not self._is_version_compatible(version):
                logger.warning(f"项目版本 {version} 可能不兼容")
            
            # 加载时间轴数据
            timeline_data = TimelineData.from_dict(project_data['timeline_data'])
            
            self.current_project_path = file_path
            self.project_modified = False
            
            logger.info(f"项目已加载: {file_path}")
            return timeline_data
            
        except Exception as e:
            logger.error(f"加载项目失败: {e}")
            return None
    
    def save_as_project(self, timeline_data: TimelineData, file_path: str) -> bool:
        """另存为项目"""
        return self.save_project(timeline_data, file_path)
    
    def export_servo_commands(self, timeline_data: TimelineData, file_path: str) -> bool:
        """导出舵机命令到文件"""
        try:
            from .servo_commander import ServoCommander
            
            commander = ServoCommander()
            preview_text = commander.generate_preview_text(timeline_data)
            
            # 确保目录存在
            dir_path = os.path.dirname(file_path)
            if dir_path:
                os.makedirs(dir_path, exist_ok=True)
            
            # 保存舵机命令
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(preview_text)
            
            logger.info(f"舵机命令已导出: {file_path}")
            return True
            
        except Exception as e:
            logger.error(f"导出舵机命令失败: {e}")
            return False
    
    def get_project_info(self, file_path: str) -> Optional[Dict[str, Any]]:
        """获取项目信息"""
        try:
            with open(file_path, 'r', encoding='utf-8') as f:
                project_data = json.load(f)
            
            return {
                'version': project_data.get('version', '1.0.0'),
                'created_time': project_data.get('created_time', ''),
                'modified_time': project_data.get('modified_time', ''),
                'file_path': file_path,
                'file_size': os.path.getsize(file_path)
            }
            
        except Exception as e:
            logger.error(f"获取项目信息失败: {e}")
            return None
    
    def _is_version_compatible(self, version: str) -> bool:
        """检查版本兼容性"""
        try:
            major, minor, patch = map(int, version.split('.'))
            current_major, current_minor, current_patch = map(int, '1.0.0'.split('.'))
            
            # 主版本号必须相同
            return major == current_major
            
        except:
            return False
    
    def set_modified(self, modified: bool = True):
        """设置项目修改状态"""
        self.project_modified = modified
    
    def is_modified(self) -> bool:
        """检查项目是否已修改"""
        return self.project_modified
    
    def get_current_project_path(self) -> Optional[str]:
        """获取当前项目路径"""
        return self.current_project_path
    
    def has_unsaved_changes(self) -> bool:
        """检查是否有未保存的更改"""
        return self.project_modified
    
    def get_recent_projects(self, max_count: int = 10) -> List[Dict[str, Any]]:
        """获取最近的项目列表"""
        try:
            # 从配置文件读取最近项目列表
            config_file = os.path.join(os.path.expanduser('~'), '.motor_controller', 'recent_projects.json')
            
            if os.path.exists(config_file):
                with open(config_file, 'r', encoding='utf-8') as f:
                    recent_projects = json.load(f)
                return recent_projects[:max_count]
            
        except Exception as e:
            logger.error(f"读取最近项目列表失败: {e}")
        
        return []
    
    def add_to_recent_projects(self, file_path: str):
        """添加项目到最近列表"""
        try:
            config_dir = os.path.join(os.path.expanduser('~'), '.motor_controller')
            os.makedirs(config_dir, exist_ok=True)
            
            config_file = os.path.join(config_dir, 'recent_projects.json')
            
            # 读取现有列表
            recent_projects = []
            if os.path.exists(config_file):
                with open(config_file, 'r', encoding='utf-8') as f:
                    recent_projects = json.load(f)
            
            # 添加新项目到列表开头
            project_info = {
                'file_path': file_path,
                'name': os.path.basename(file_path),
                'last_accessed': datetime.now().isoformat()
            }
            
            # 移除重复项
            recent_projects = [p for p in recent_projects if p['file_path'] != file_path]
            recent_projects.insert(0, project_info)
            
            # 限制列表长度
            recent_projects = recent_projects[:10]
            
            # 保存列表
            with open(config_file, 'w', encoding='utf-8') as f:
                json.dump(recent_projects, f, ensure_ascii=False, indent=2)
            
        except Exception as e:
            logger.error(f"添加最近项目失败: {e}")

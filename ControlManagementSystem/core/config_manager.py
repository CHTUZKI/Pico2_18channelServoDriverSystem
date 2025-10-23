"""
参数配置管理器
用于保存和加载用户的默认参数设置
"""

import json
import os
import sys
from typing import Dict, Any
from core.logger import get_logger

logger = get_logger()


class ConfigManager:
    """配置管理器 - 管理用户的默认参数设置"""
    
    def __init__(self, config_file: str = "config.json"):
        """
        初始化配置管理器
        
        Args:
            config_file: 配置文件名(不含路径)
        """
        # 获取程序运行目录(支持打包成exe)
        if getattr(sys, 'frozen', False):
            # 打包成exe后,使用exe所在目录
            app_dir = os.path.dirname(sys.executable)
        else:
            # 开发模式,使用脚本所在目录的父目录(MotorControllerSystem目录)
            app_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        
        # 配置文件放在程序目录下
        self.config_file = os.path.join(app_dir, config_file)
        logger.info(f"配置文件路径: {self.config_file}")
        
        self.config = self._load_default_config()
        self.load()
    
    def _load_default_config(self) -> Dict[str, Any]:
        """加载默认配置"""
        return {
            # 电机参数
            "motor": {
                "step_angle": 1.8,              # 电机步距角(度)
                "microsteps": 8,                # 驱动器细分
                "grbl_steps_per_mm": 250.0,     # grblHAL的steps/mm设置
            },
            
            # 默认速度
            "default_speed": {
                "rotation": 1000.0,             # 旋转默认速度(度/分钟)
                "home": 1000.0,                 # 归零默认速度(度/分钟)
            },
            
            # 默认角度
            "default_angle": {
                "forward": 90.0,                # 正转默认角度(度)
                "reverse": -90.0,               # 反转默认角度(度)
            },
            
            # 默认持续时间
            "default_duration": {
                "rotation": 5.0,                # 旋转默认持续时间(秒)
                "delay": 1.0,                   # 延时默认时长(秒)
            },
            
            # grblHAL参数
            "grblhal": {
                "max_travel": 36000.0,          # 最大行程(mm) - 自动设置$130-$137
                "auto_set_travel": False,       # 是否自动设置最大行程
            },
            
            # 串口参数
            "serial": {
                "baudrate": 115200,             # 波特率
                "timeout": 1.0,                 # 超时时间(秒)
                "write_timeout": 1.0,           # 写入超时(秒)
            },
            
            # UI参数
            "ui": {
                "window_width": 1400,           # 窗口宽度
                "window_height": 900,           # 窗口高度
                "timeline_zoom": 50,            # 时间轴缩放
            }
        }
    
    def load(self) -> bool:
        """从文件加载配置"""
        try:
            if os.path.exists(self.config_file):
                with open(self.config_file, 'r', encoding='utf-8') as f:
                    loaded_config = json.load(f)
                    # 合并加载的配置和默认配置
                    self._merge_config(self.config, loaded_config)
                    logger.info(f"配置文件加载成功: {self.config_file}")
                    return True
            else:
                logger.info("配置文件不存在,使用默认配置")
                return False
        except Exception as e:
            logger.error(f"加载配置文件失败: {e}")
            return False
    
    def save(self) -> bool:
        """保存配置到文件"""
        try:
            with open(self.config_file, 'w', encoding='utf-8') as f:
                json.dump(self.config, f, indent=4, ensure_ascii=False)
            logger.info(f"配置文件保存成功: {self.config_file}")
            return True
        except Exception as e:
            logger.error(f"保存配置文件失败: {e}")
            return False
    
    def _merge_config(self, default: Dict, loaded: Dict):
        """递归合并配置,保留默认值中有但加载值中没有的项"""
        for key, value in loaded.items():
            if key in default:
                if isinstance(value, dict) and isinstance(default[key], dict):
                    self._merge_config(default[key], value)
                else:
                    default[key] = value
    
    def get(self, key_path: str, default: Any = None) -> Any:
        """
        获取配置值
        
        Args:
            key_path: 配置路径,用.分隔,如 "motor.step_angle"
            default: 默认值
            
        Returns:
            配置值
        """
        keys = key_path.split('.')
        value = self.config
        
        try:
            for key in keys:
                value = value[key]
            return value
        except (KeyError, TypeError):
            return default
    
    def set(self, key_path: str, value: Any):
        """
        设置配置值
        
        Args:
            key_path: 配置路径,用.分隔,如 "motor.step_angle"
            value: 配置值
        """
        keys = key_path.split('.')
        config = self.config
        
        # 导航到目标位置
        for key in keys[:-1]:
            if key not in config:
                config[key] = {}
            config = config[key]
        
        # 设置值
        config[keys[-1]] = value
    
    def get_motor_settings(self) -> Dict[str, Any]:
        """获取电机设置"""
        return self.config.get('motor', {})
    
    def set_motor_settings(self, settings: Dict[str, Any]):
        """设置电机参数"""
        self.config['motor'] = settings
    
    def get_default_speed(self, component_type: str) -> float:
        """获取默认速度"""
        return self.config['default_speed'].get(component_type, 1000.0)
    
    def get_default_angle(self, direction: str) -> float:
        """获取默认角度"""
        return self.config['default_angle'].get(direction, 90.0)
    
    def get_default_duration(self, component_type: str) -> float:
        """获取默认持续时间"""
        return self.config['default_duration'].get(component_type, 5.0)
    
    def get_serial_settings(self) -> Dict[str, Any]:
        """获取串口设置"""
        return self.config.get('serial', {})
    
    def reset_to_default(self):
        """重置为默认配置"""
        self.config = self._load_default_config()
        logger.info("配置已重置为默认值")


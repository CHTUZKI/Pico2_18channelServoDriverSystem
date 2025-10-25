#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
舵机命令器 - 流式缓冲区架构
基于运动缓冲区，Pico自主调度执行
"""

from typing import List, Dict, Any
from models.timeline_data import TimelineData
from models.component import ComponentType
from core.logger import get_logger
from core.serial_comm import SerialComm
import time

logger = get_logger()

class ServoCommander:
    """
    舵机命令器 - 流式缓冲区架构
    
    架构特点：
    1. 上位机将时间线转换为运动指令序列
    2. 流式发送到Pico的运动缓冲区（不等待）
    3. Pico根据时间戳自主调度执行
    4. 上位机只监听状态，无需参与调度
    """
    
    def __init__(self, serial_comm: SerialComm):
        self.serial_comm = serial_comm
        self.servo_count = 18
        self.should_stop = False
        self.current_positions = [90.0] * 18
    
    def execute_timeline(self, timeline_data: TimelineData, should_loop: bool = False) -> bool:
        """
        执行时间线（流式缓冲区模式）
        
        Args:
            timeline_data: 时间线数据
            should_loop: 是否循环（暂不支持）
            
        Returns:
            bool: 是否成功
        """
        try:
            # 1. 清空Pico的运动缓冲区
            logger.info("=" * 80)
            logger.info("【流式缓冲区模式】Pico自主调度执行")
            logger.info("步骤1/5: 清空运动缓冲区...")
            
            if not self.serial_comm.clear_buffer():
                logger.error("清空缓冲区失败")
                return False
            
            time.sleep(0.1)
            
            # 2. 生成所有运动指令
            logger.info("步骤2/5: 生成运动指令...")
            motion_blocks = []
            
            for track in timeline_data.tracks:
                if not track.visible or not track.components:
                    continue
                
                servo_id = track.motor_id
                current_pos = self.current_positions[servo_id]
                
                for component in track.components:
                    # 只处理运动组件
                    if component.type not in [ComponentType.FORWARD_ROTATION, ComponentType.REVERSE_ROTATION]:
                        continue
                    
                    # 计算时间戳（秒→毫秒）
                    timestamp_ms = int(component.start_time * 1000)
                    
                    # 获取目标角度
                    target_angle = component.target_angle
                    
                    # 获取运动参数（默认梯形模式）
                    motion_mode = component.parameters.get('motion_mode', 'trapezoid')
                    
                    if motion_mode == 'trapezoid':
                        # 梯形速度模式
                        velocity = component.parameters.get('velocity', 30.0)
                        acceleration = component.parameters.get('acceleration', 60.0)
                        deceleration = component.parameters.get('deceleration', 0.0)
                        
                        motion_blocks.append({
                            'timestamp_ms': timestamp_ms,
                            'servo_id': servo_id,
                            'angle': target_angle,
                            'velocity': velocity,
                            'acceleration': acceleration,
                            'deceleration': deceleration
                        })
                        
                        distance = abs(target_angle - current_pos)
                        logger.info(f"  生成: t={timestamp_ms}ms 舵机{servo_id}: {current_pos:.1f}°→{target_angle:.1f}° (Δ{distance:.1f}°) v={velocity}°/s a={acceleration}°/s²")
                        
                        current_pos = target_angle
                    else:
                        logger.warning(f"  跳过非梯形模式: {motion_mode}")
            
            if not motion_blocks:
                logger.warning("没有生成任何运动指令")
                return False
            
            # 3. 按时间排序
            motion_blocks.sort(key=lambda x: x['timestamp_ms'])
            logger.info(f"共生成{len(motion_blocks)}条运动指令")
            
            # 4. 收集需要使能的舵机
            logger.info("步骤3/5: 使能舵机...")
            active_servos = set(block['servo_id'] for block in motion_blocks)
            logger.info(f"使能舵机: {sorted(active_servos)}")
            
            for servo_id in active_servos:
                self.serial_comm.enable_servo(servo_id)
                time.sleep(0.05)
            
            # 5. 流式发送到Pico
            logger.info("步骤4/5: 流式上传指令到Pico...")
            
            success_count = 0
            failed_count = 0
            
            for i, block in enumerate(motion_blocks):
                result = self.serial_comm.add_motion_block(
                    block['timestamp_ms'],
                    block['servo_id'],
                    block['angle'],
                    block['velocity'],
                    block['acceleration'],
                    block['deceleration']
                )
                
                if result:
                    success_count += 1
                    # 每10条或最后一条时输出进度
                    if (i + 1) % 10 == 0 or (i + 1) == len(motion_blocks):
                        logger.info(f"  上传进度: {i+1}/{len(motion_blocks)} 条")
                else:
                    failed_count += 1
                    logger.error(f"  指令{i+1}上传失败（缓冲区可能已满）")
                    if failed_count >= 3:
                        logger.error("连续失败，终止上传")
                        break
                
                # 短暂延时避免串口拥堵
                time.sleep(0.01)
            
            logger.info(f"上传完成: 成功{success_count}条，失败{failed_count}条")
            
            if success_count == 0:
                logger.error("没有成功上传任何指令")
                return False
            
            # 6. 查询缓冲区状态
            time.sleep(0.1)
            status = self.serial_comm.get_buffer_status()
            logger.info(f"缓冲区状态: 已用={status['count']}/{32-status['available']}, 可用={status['available']}")
            
            # 7. 启动Pico自主执行
            logger.info("步骤5/5: 启动Pico自主执行...")
            
            if not self.serial_comm.start_motion():
                logger.error("启动执行失败")
                return False
            
            logger.info("=" * 80)
            logger.info("✓ Pico已开始自主执行！")
            logger.info("  Pico将根据时间戳自动调度每条指令")
            logger.info("  上位机无需参与调度，只监听执行状态")
            logger.info("=" * 80)
            
            # 8. 等待并监听状态
            if motion_blocks:
                last_block = motion_blocks[-1]
                estimated_duration = last_block['timestamp_ms'] / 1000.0 + 5.0
                
                logger.info(f"预计执行时长: {estimated_duration:.1f}秒")
                logger.info("监听执行状态...")
                
                start_time = time.time()
                last_count = status['count']
                
                while time.time() - start_time < estimated_duration:
                    time.sleep(1.0)
                    
                    if self.should_stop:
                        logger.info("检测到停止信号，停止Pico执行")
                        self.serial_comm.stop_motion()
                        return False
                    
                    status = self.serial_comm.get_buffer_status()
                    
                    # 只在指令数量变化时输出
                    if status['count'] != last_count:
                        progress = (len(motion_blocks) - status['count']) / len(motion_blocks) * 100
                        logger.info(f"  执行进度: {progress:.0f}% (剩余{status['count']}条)")
                        last_count = status['count']
                    
                    if status['count'] == 0 and not status['running']:
                        logger.info("=" * 80)
                        logger.info("✓ 所有运动指令执行完成！")
                        logger.info("=" * 80)
                        break
                else:
                    # 超时
                    logger.warning("执行超时，可能还有指令未完成")
            
            return True
            
        except Exception as e:
            logger.error(f"执行时间线失败: {e}", exc_info=True)
            return False
    
    def stop(self):
        """停止执行"""
        self.should_stop = True
        logger.info("设置停止标志")

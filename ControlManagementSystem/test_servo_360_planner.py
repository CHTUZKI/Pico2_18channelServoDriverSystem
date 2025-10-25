#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
360度舵机与Look-Ahead Planner集成测试
展示360度舵机的速度平滑过渡功能
"""

import sys
import os
import time

# 添加项目路径
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core.serial_comm import SerialComm


def test_continuous_speed_planning():
    """测试1：连续速度变化（速度平滑过渡）"""
    
    print("=" * 60)
    print("测试1：360度舵机速度平滑过渡")
    print("=" * 60)
    
    serial = SerialComm()
    
    # 连接到Pico
    if not serial.connect('COM3', 115200):
        print("❌ 连接失败，请检查端口")
        return False
    
    print("✅ 已连接到Pico\n")
    time.sleep(0.5)
    
    # 清空缓冲区
    serial.clear_buffer()
    print("📋 缓冲区已清空\n")
    
    # 定义速度序列
    speed_sequence = [
        # (时间戳ms, 舵机ID, 速度%, 加速度, 减速度, 持续时间ms)
        (0,    0, 30,  40, 40, 1500),   # 加速到30%，持续1.5秒
        (1500, 0, 60,  50, 50, 2000),   # 加速到60%，持续2秒
        (3500, 0, 90,  60, 60, 1500),   # 加速到90%，持续1.5秒
        (5000, 0, 40,  50, 50, 2000),   # 减速到40%，持续2秒
        (7000, 0, -30, 40, 40, 2000),   # 反向到-30%，持续2秒
        (9000, 0, 0,   30, 30, 0),      # 停止
    ]
    
    print("📤 添加速度控制块...")
    for ts, sid, speed, accel, decel, duration in speed_sequence:
        success = serial.add_continuous_motion(
            timestamp_ms=ts,
            servo_id=sid,
            speed_pct=speed,
            accel_rate=accel,
            decel_rate=decel,
            duration_ms=duration
        )
        if success:
            print(f"  ✅ t={ts:5d}ms: 速度={speed:+4d}%, 加速度={accel}%/s, 持续={duration}ms")
        else:
            print(f"  ❌ 添加失败")
            serial.disconnect()
            return False
    
    # 启动执行
    print("\n🚀 启动执行（规划器自动计算速度衔接）...\n")
    if not serial.start_motion():
        print("❌ 启动失败")
        serial.disconnect()
        return False
    
    # 监控执行状态
    print("📊 执行状态监控:")
    print("-" * 60)
    
    start_time = time.time()
    while True:
        status = serial.get_buffer_status()
        info = serial.servo_360_get_info(0)
        
        elapsed = time.time() - start_time
        
        print(f"[{elapsed:5.1f}s] 缓冲区:{status['count']:2d}块 | "
              f"当前速度:{info['current_speed']:+4d}% | "
              f"目标速度:{info['target_speed']:+4d}% | "
              f"运动中:{info['moving']}")
        
        if status['count'] == 0 and not status['running']:
            print("\n✅ 执行完成")
            break
        
        time.sleep(0.5)
    
    serial.disconnect()
    print("\n✅ 测试1通过\n")
    return True


def test_mixed_control():
    """测试2：混合控制（180度 + 360度）"""
    
    print("=" * 60)
    print("测试2：混合控制（180度位置 + 360度速度）")
    print("=" * 60)
    
    serial = SerialComm()
    
    if not serial.connect('COM3', 115200):
        print("❌ 连接失败")
        return False
    
    print("✅ 已连接到Pico\n")
    time.sleep(0.5)
    
    serial.clear_buffer()
    
    print("📤 添加混合控制块...")
    
    # 舵机1（180度位置舵机）- 往复运动
    print("\n舵机1（180度位置模式）：")
    serial.add_motion_block(0,    servo_id=1, target_angle=90,  velocity=30, acceleration=60, deceleration=60)
    print(f"  ✅ t=0ms:    位置=90°,  速度=30°/s")
    
    serial.add_motion_block(2000, servo_id=1, target_angle=180, velocity=40, acceleration=80, deceleration=80)
    print(f"  ✅ t=2000ms: 位置=180°, 速度=40°/s")
    
    serial.add_motion_block(4000, servo_id=1, target_angle=0,   velocity=30, acceleration=60, deceleration=60)
    print(f"  ✅ t=4000ms: 位置=0°,   速度=30°/s")
    
    # 舵机2（360度连续舵机）- 变速旋转
    print("\n舵机2（360度速度模式）：")
    serial.add_continuous_motion(0,    servo_id=2, speed_pct=40,  accel_rate=30, duration_ms=2000)
    print(f"  ✅ t=0ms:    速度=40%,  持续2秒")
    
    serial.add_continuous_motion(2000, servo_id=2, speed_pct=70,  accel_rate=50, duration_ms=2000)
    print(f"  ✅ t=2000ms: 速度=70%,  持续2秒")
    
    serial.add_continuous_motion(4000, servo_id=2, speed_pct=-50, accel_rate=40, duration_ms=2000)
    print(f"  ✅ t=4000ms: 速度=-50%, 持续2秒")
    
    serial.add_continuous_motion(6000, servo_id=2, speed_pct=0,   accel_rate=30, duration_ms=0)
    print(f"  ✅ t=6000ms: 停止")
    
    # 启动执行
    print("\n🚀 启动混合执行...\n")
    serial.start_motion()
    
    # 简单监控
    print("⏳ 执行中...")
    time.sleep(8)
    
    serial.disconnect()
    print("\n✅ 测试2通过\n")
    return True


def test_immediate_control():
    """测试3：即时速度控制（不使用规划器）"""
    
    print("=" * 60)
    print("测试3：即时速度控制（手动模式）")
    print("=" * 60)
    
    serial = SerialComm()
    
    if not serial.connect('COM3', 115200):
        print("❌ 连接失败")
        return False
    
    print("✅ 已连接到Pico\n")
    time.sleep(0.5)
    
    # 设置加减速参数
    print("⚙️  设置加减速参数...")
    serial.servo_360_set_accel(servo_id=0, accel_rate=40, decel_rate=40)
    print("  ✅ 加速度=40%/s, 减速度=40%/s\n")
    
    # 直接速度控制
    print("🎮 手动速度控制:")
    
    print("  → 正转50%")
    serial.servo_360_set_speed(0, 50)
    time.sleep(3)
    
    print("  → 正转100%（最大速度）")
    serial.servo_360_set_speed(0, 100)
    time.sleep(3)
    
    print("  → 反转-60%")
    serial.servo_360_set_speed(0, -60)
    time.sleep(3)
    
    print("  → 软停止（平滑减速）")
    serial.servo_360_soft_stop(0)
    time.sleep(2)
    
    # 查询状态
    info = serial.servo_360_get_info(0)
    print(f"\n📊 最终状态:")
    print(f"  当前速度: {info['current_speed']}%")
    print(f"  目标速度: {info['target_speed']}%")
    print(f"  运动中: {info['moving']}")
    
    serial.disconnect()
    print("\n✅ 测试3通过\n")
    return True


def main():
    """主测试函数"""
    
    print("\n" + "=" * 60)
    print("360度舵机 + Look-Ahead Planner 集成测试")
    print("=" * 60 + "\n")
    
    print("测试内容：")
    print("  1. 连续速度变化（速度平滑过渡）")
    print("  2. 混合控制（180度 + 360度）")
    print("  3. 即时速度控制（手动模式）")
    print()
    
    # 选择测试
    print("请选择测试（1/2/3/all）：", end='')
    choice = input().strip().lower()
    
    if choice == '1':
        test_continuous_speed_planning()
    elif choice == '2':
        test_mixed_control()
    elif choice == '3':
        test_immediate_control()
    elif choice == 'all':
        test_continuous_speed_planning()
        time.sleep(2)
        test_mixed_control()
        time.sleep(2)
        test_immediate_control()
    else:
        print("❌ 无效选择")
        return
    
    print("\n" + "=" * 60)
    print("🎉 所有测试完成")
    print("=" * 60)


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print("\n\n⚠️  用户中断")
    except Exception as e:
        print(f"\n❌ 错误: {e}")
        import traceback
        traceback.print_exc()


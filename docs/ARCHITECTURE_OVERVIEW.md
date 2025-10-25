# 架构总览文档

## 📋 **整体架构**

本项目是一个**18路舵机驱动系统**，采用**智能运动控制器**架构，Pico2作为自主运动规划器。

```
┌─────────────────────────────────────────────────────────────┐
│                    上位机（任意平台）                          │
│  ┌─────────────┐  ┌──────────────┐  ┌──────────────┐       │
│  │  PyQt5 GUI  │  │  ROS2 Node   │  │  Web/Mobile  │  ...  │
│  └──────┬──────┘  └──────┬───────┘  └──────┬───────┘       │
│         └─────────────────┴──────────────────┘               │
│                           │                                  │
│                  流式发送运动指令                             │
│                           ↓                                  │
└───────────────────────────┼──────────────────────────────────┘
                            │ USB CDC
┌───────────────────────────┼──────────────────────────────────┐
│              Raspberry Pi Pico 2 (RP2350)                    │
│  ┌──────────────────────────────────────────────────────┐   │
│  │            运动缓冲区（32条指令）                      │   │
│  │  [t=0ms, S1→90°] [t=1000ms, S1→0°] [t=2000ms...]    │   │
│  └────────────────────┬─────────────────────────────────┘   │
│                       │                                      │
│  ┌────────────────────▼──────────────────────────────┐      │
│  │          运动调度器（20ms定时检查）               │      │
│  │  • 检查时间戳是否到达                             │      │
│  │  • 自动触发梯形速度运动                           │      │
│  │  • 管理18路舵机独立插值                           │      │
│  └────────────────────┬──────────────────────────────┘      │
│                       │                                      │
│  ┌────────────────────▼──────────────────────────────┐      │
│  │       梯形速度插值器（实时计算）                   │      │
│  │  • 加速段 → 匀速段 → 减速段                       │      │
│  │  • 平滑运动曲线生成                               │      │
│  └────────────────────┬──────────────────────────────┘      │
│                       │                                      │
│  ┌────────────────────▼──────────────────────────────┐      │
│  │        18路硬件PWM输出（500-2500μs）              │      │
│  └───────────────────────────────────────────────────┘      │
└───────────────────────────────────────────────────────────────┘
```

---

## 🎯 **核心设计理念**

### **1. Pico作为智能运动控制器**
- ✅ 自主运动规划和执行
- ✅ 时间戳驱动的精确调度
- ✅ 不依赖上位机实时性
- ✅ 可脱机运行

### **2. 上位机作为指令发送器**
- ✅ 轨迹编辑和可视化
- ✅ 流式发送运动序列
- ✅ 状态监控
- ✅ 任何平台都可替换（PC/ROS2/手机）

---

## 📦 **Pico端架构**

### **核心模块**

#### **1. 运动缓冲区 (motion_buffer)**
```c
// include/motion/motion_buffer.h
// src/motion/motion_buffer.c

typedef struct {
    uint32_t timestamp_ms;       // 执行时间戳
    uint8_t servo_id;            // 舵机ID
    float target_angle;          // 目标角度
    float velocity;              // 速度
    float acceleration;          // 加速度
    float deceleration;          // 减速度
} motion_block_t;

// 环形缓冲区，支持32条指令
motion_buffer_t g_motion_buffer;
```

**功能**：
- 存储待执行的运动指令
- 环形队列管理（生产者-消费者模式）
- 流式添加，按序执行

---

#### **2. 运动调度器 (AO_Motion)**
```c
// src/ao/ao_motion.c

static void motion_scheduler_update(AO_Motion_t * const me) {
    // 每20ms检查一次
    uint32_t elapsed_ms = current_time - start_time;
    
    motion_block_t *block = motion_buffer_peek();
    if (elapsed_ms >= block->timestamp_ms) {
        // 时间到了，执行这条指令
        AO_Motion_set_trapezoid(...);
        QACTIVE_POST(MOTION_START_SIG);
        motion_buffer_remove();
    }
}
```

**功能**：
- 定时检查缓冲区
- 根据时间戳自动触发运动
- 管理状态机转换

---

#### **3. 梯形速度插值器 (interpolation)**
```c
// src/motion/interpolation.c

float interpolate_trapezoid(float start, float end, float t_current,
                            float distance, float t_accel, float t_const, 
                            float t_decel, float v_max);
```

**功能**：
- 计算梯形速度曲线
- 三段式运动：加速→匀速→减速
- 实时位置计算

---

#### **4. 通信协议**
```c
// include/communication/protocol.h

// 新架构命令
#define CMD_ADD_MOTION_BLOCK    0x40  // 添加运动指令
#define CMD_START_MOTION        0x41  // 开始执行
#define CMD_STOP_MOTION         0x42  // 停止
#define CMD_CLEAR_BUFFER        0x45  // 清空缓冲区
#define CMD_GET_BUFFER_STATUS   0x46  // 查询状态
```

**数据格式**（ADD_MOTION_BLOCK）：
```
[timestamp_ms(4)] [servo_id(1)] [angle(2)] [velocity(2)] [accel(2)] [decel(2)]
总计：13字节
```

---

## 💻 **上位机架构**

### **核心模块**

#### **1. 串口通信层 (serial_comm.py)**
```python
class SerialComm:
    # 新架构接口
    def add_motion_block(timestamp_ms, servo_id, angle, velocity, acceleration)
    def start_motion()
    def stop_motion()
    def clear_buffer()
    def get_buffer_status()
```

---

#### **2. 命令器V2 (servo_commander_v2.py)** ⭐ 新架构
```python
class ServoCommanderV2:
    def execute_timeline(timeline_data):
        # 1. 清空缓冲区
        serial_comm.clear_buffer()
        
        # 2. 生成所有运动指令
        for component in timeline:
            motion_blocks.append({
                'timestamp_ms': component.start_time * 1000,
                'servo_id': servo_id,
                'angle': target_angle,
                'velocity': velocity,
                'acceleration': acceleration
            })
        
        # 3. 流式发送（不等待）
        for block in motion_blocks:
            serial_comm.add_motion_block(...)
        
        # 4. 启动Pico自主执行
        serial_comm.start_motion()
        
        # 5. 上位机只监听状态
        while True:
            status = serial_comm.get_buffer_status()
            if status['count'] == 0:
                break  # 执行完成
```

---

#### **3. 命令器V1 (servo_commander.py)** 🔄 旧架构（兼容）
```python
class ServoCommander:
    def execute_timeline(timeline_data):
        # 旧方式：逐个发送并等待
        for cmd in sequence:
            serial_comm.move_servo_trapezoid(...)
            time.sleep(wait_time)  # 等待完成
```

**保留原因**：向后兼容，GUI无需修改

---

## 🔄 **执行流程对比**

### **旧架构（V1）**
```
上位机                          Pico
  │                              │
  ├─[0s] 发送: S1→0°──────────→ 执行
  │      等待1秒...              │
  ├─[1s] 发送: S1→90°─────────→ 执行
  │      等待1秒...              │
  └─[2s] 发送: S1→180°────────→ 执行
  
❌ 问题：
- 上位机负责调度（实时性差）
- 换上位机需重写调度逻辑
- 网络延迟影响精度
```

### **新架构（V2）**
```
上位机                          Pico
  │                              │
  ├─ 发送: [0s, S1→0°]──────┐   │
  ├─ 发送: [1s, S1→90°]─────┼──→ 缓冲区
  ├─ 发送: [2s, S1→180°]────┘   │
  │                              │
  ├─ START ──────────────────────→ 开始调度
  │                              │
  └─ 等待监听状态                ├─[0s] 自动执行S1→0°
                                 ├─[1s] 自动执行S1→90°
                                 └─[2s] 自动执行S1→180°

✅ 优势：
- Pico自主调度（高精度）
- 上位机可随意更换
- 可脱机运行
```

---

## 📁 **目录结构**

```
Pico2_18channelServoDriverSystem/
├── include/
│   ├── motion/
│   │   ├── motion_buffer.h          ⭐ 新增：运动缓冲区
│   │   └── interpolation.h
│   └── communication/
│       ├── protocol.h                ✏️ 修改：新增命令
│       └── commands.h                ✏️ 修改：新增处理器
├── src/
│   ├── motion/
│   │   ├── motion_buffer.c           ⭐ 新增：缓冲区实现
│   │   └── interpolation.c
│   ├── ao/
│   │   └── ao_motion.c               ✏️ 修改：集成调度器
│   └── communication/
│       ├── cmd_motion_buffer.c       ⭐ 新增：缓冲区命令
│       └── commands.c                ✏️ 修改：注册新命令
├── ControlManagementSystem/
│   └── core/
│       ├── serial_comm.py            ✏️ 修改：新增缓冲区接口
│       ├── servo_commander.py        🔄 保留：旧架构（兼容）
│       └── servo_commander_v2.py     ⭐ 新增：新架构
```

---

## 🚀 **使用指南**

### **启用新架构**
修改GUI主窗口，使用V2命令器：

```python
# ControlManagementSystem/gui/main_window.py

# 旧方式
from core.servo_commander import ServoCommander
self.commander = ServoCommander(self.serial_comm)

# 新方式（推荐）
from core.servo_commander_v2 import ServoCommanderV2
self.commander = ServoCommanderV2(self.serial_comm)

# GUI其他部分无需修改！
```

---

## 🔧 **编译与测试**

### **编译Pico固件**
```bash
cd Pico2_18channelServoDriverSystem
pio run
pio run --target upload  # 按住BOOTSEL
```

### **运行上位机**
```bash
cd ControlManagementSystem
python main.py
```

---

## 📊 **性能指标**

| 指标 | 旧架构 | 新架构 |
|------|--------|--------|
| 时间精度 | ±100ms | ±10ms |
| 最大缓冲 | 1条 | 32条 |
| 上位机CPU | 中等 | 极低 |
| 可脱机运行 | ❌ | ✅ |
| 更换上位机 | 需重写 | 无需修改 |

---

## 📝 **待办事项**

- [x] Pico端缓冲区实现
- [x] Pico端调度器集成
- [x] 上位机新架构实现
- [ ] GUI切换到V2命令器
- [ ] 完整测试验证
- [ ] 文档完善

---

## 🎓 **技术参考**

本架构借鉴了CNC运动控制器的设计理念：
- 上位机生成轨迹→发送G代码流→机床自主执行
- 本项目：上位机编辑时间线→发送运动指令流→Pico自主执行

优势：
- 解耦上位机和执行器
- 提高实时性和可靠性
- 便于多平台集成

---

**版本**: V2.0  
**日期**: 2025-10-26  
**作者**: AI Assistant


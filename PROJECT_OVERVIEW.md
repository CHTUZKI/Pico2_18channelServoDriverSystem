# 18轴舵机控制系统 - 项目概览

## 📋 项目信息

- **项目名称**: 18轴舵机控制系统
- **版本**: v2.0.0（架构优化版）
- **最后更新**: 2025-10-25
- **平台**: Raspberry Pi Pico 2 (RP2350)
- **应用**: 6足机器人、多关节机械臂

## 🎯 核心功能

### 已实现功能 ✅

#### Phase 1: 基础PWM功能
- [x] 18路硬件PWM输出（GPIO 0-17）
- [x] 50Hz频率，500-2500μs脉宽范围
- [x] 1μs分辨率
- [x] 独立使能/禁用控制

#### Phase 2: 通信协议
- [x] USB CDC虚拟串口通信
- [x] 自定义帧格式协议
- [x] CRC-16 CCITT校验
- [x] 完整命令集实现：
  - MOVE_SINGLE (0x01) - 单轴控制
  - MOVE_ALL (0x03) - 批量控制
  - GET_SINGLE (0x10) - 单轴查询
  - GET_ALL (0x11) - 全轴查询
  - ENABLE/DISABLE (0x20/0x21) - 使能控制
  - SAVE/LOAD_FLASH (0x30/0x31) - 参数存储
  - PING (0xFE) - 心跳检测
  - ESTOP (0xFF) - 紧急停止

#### Phase 3: 运动控制
- [x] 线性插值算法
- [x] S曲线插值（平滑加减速）
- [x] 多轴同步运动
- [x] 20ms插值周期
- [x] 可配置运动时间

#### Phase 4: 参数存储
- [x] Flash存储接口
- [x] 舵机校准参数持久化
- [x] 参数版本管理
- [x] CRC校验保护
- [x] 出厂复位功能

#### Phase 5: 高级功能
- [x] QP/C双核调度
- [x] LED状态指示
- [x] 错误处理机制
- [x] 安全保护（急停、超时、限位）
- [x] 环形缓冲区通信

#### Phase 6: 架构优化 v2.0 ✨
- [x] 运动调度器独立模块（motion_scheduler.c）
- [x] 时间戳自主调度（CNC式架构）
- [x] 流式缓冲区架构（32条指令队列）
- [x] 上位机流式命令器整合
- [x] 代码清理优化
  - [x] 删除usb_handler.c（冗余）
  - [x] 删除servo_commander_v2.py（整合）
  - [x] ao_motion.c简化（444→381行）
- [x] 职责分离
  - motion_buffer.c：数据结构管理
  - motion_scheduler.c：时间调度
  - interpolation.c：曲线计算

### 待实现功能 📝

#### Phase 7: ROS2集成 (未来)
- [ ] ROS2节点框架
- [ ] serial库集成
- [ ] joint_state发布器
- [ ] 命令订阅器
- [ ] 参数服务器

#### Phase 8: 测试与优化
- [ ] 单元测试
- [ ] 压力测试
- [ ] 长时间稳定性测试
- [ ] 性能优化

## 📁 项目结构

```
18ChannelServoDriver/
├── include/                    # 头文件
│   ├── config/                 # 配置
│   │   ├── config.h           # 系统参数
│   │   └── pinout.h           # GPIO定义
│   ├── pwm/                    # PWM驱动
│   │   └── pio_pwm.h          # PWM接口
│   ├── servo/                  # 舵机控制
│   │   └── servo_control.h    # 舵机接口
│   ├── communication/          # 通信
│   │   ├── usb_handler.h      # USB CDC
│   │   ├── protocol.h         # 协议解析
│   │   ├── crc16.h            # CRC校验
│   │   └── commands.h         # 命令处理
│   ├── motion/                 # 运动控制
│   │   └── interpolation.h    # 插值算法
│   ├── storage/                # 存储
│   │   ├── flash_storage.h    # Flash接口
│   │   └── param_manager.h    # 参数管理
│   ├── tasks/                  # FreeRTOS任务
│   │   ├── task_communication.h
│   │   ├── task_motion.h
│   │   └── task_pwm.h
│   ├── utils/                  # 工具
│   │   ├── ring_buffer.h      # 环形缓冲
│   │   └── error_handler.h    # 错误处理
│   └── FreeRTOSConfig.h       # FreeRTOS配置
│
├── src/                        # 源文件
│   ├── main.c                 # 主程序
│   ├── pwm/
│   │   ├── pio_pwm.c
│   │   └── servo_pwm.pio
│   ├── servo/
│   │   └── servo_control.c
│   ├── communication/
│   │   ├── usb_handler.c
│   │   ├── protocol.c
│   │   ├── crc16.c
│   │   └── commands.c
│   ├── motion/
│   │   └── interpolation.c
│   ├── storage/
│   │   ├── flash_storage.c
│   │   └── param_manager.c
│   ├── tasks/
│   │   ├── task_communication.c
│   │   ├── task_motion.c
│   │   └── task_pwm.c
│   └── utils/
│       ├── ring_buffer.c
│       └── error_handler.c
│
├── platformio.ini             # PlatformIO配置
├── CMakeLists.txt             # CMake配置
├── README.md                  # 项目说明
├── BUILD_GUIDE.md             # 构建指南
├── test_servo.py              # Python测试工具
└── 功能需求文档.md            # 原始需求文档
```

## 🔧 技术栈

### 硬件
- **MCU**: RP2350 (Dual Cortex-M33 @ 150MHz)
- **内存**: 520KB SRAM
- **PWM**: 24个硬件PWM通道（使用18个）
- **PIO**: 12个PIO状态机（保留未用）

### 软件
- **RTOS**: FreeRTOS (SMP双核)
- **SDK**: Pico SDK
- **通信**: TinyUSB (USB CDC)
- **构建**: PlatformIO + CMake
- **语言**: C11

### 算法
- **PWM生成**: 硬件PWM定时器（非PIO方案）
- **插值**: 线性 + S曲线
- **通信**: 自定义协议 + CRC-16
- **存储**: Flash + 校验和

## 📊 性能指标

### 实际指标
| 指标 | 值 | 说明 |
|------|-----|------|
| PWM方案 | 硬件PWM | 非PIO实现 |
| PWM频率 | 50Hz | 标准舵机频率 |
| PWM分辨率 | ±0.5μs | 2MHz时钟精度 |
| PWM稳定性 | 硬件自动 | 不受中断影响 |
| 通信波特率 | 115200 bps | USB CDC |
| 插值周期 | 20ms | 与PWM同步 |
| 系统Tick | 1ms | FreeRTOS |
| 堆大小 | 128KB | 动态内存 |

### 目标指标
| 指标 | 目标 | 状态 |
|------|------|------|
| 单轴响应 | < 20ms | ⏳ 待测试 |
| 批量响应 | < 30ms | ⏳ 待测试 |
| PWM抖动 | < 0.5μs | ✅ 硬件保证 |
| 稳定运行 | 24h+ | ⏳ 待测试 |

## 🎨 系统架构

### 双核分工

```
┌─────────────────────────────────┐
│ Core 0 (通信/算法核)              │
│  ┌────────────────────────────┐ │
│  │ Task: Communication (P3)    │ │ → USB通信、协议解析
│  │ 周期: 10ms                  │ │
│  └────────────────────────────┘ │
│  ┌────────────────────────────┐ │
│  │ Task: Motion (P2)          │ │ → 运动插值计算
│  │ 周期: 20ms                 │ │
│  └────────────────────────────┘ │
└─────────────────────────────────┘

┌─────────────────────────────────┐
│ Core 1 (PWM实时核)               │
│  ┌────────────────────────────┐ │
│  │ Task: PWM (P4, 最高优先级)  │ │ → PWM更新、LED指示
│  │ 周期: 20ms                  │ │
│  └────────────────────────────┘ │
└─────────────────────────────────┘
```

### 数据流

```
USB接收 → 环形缓冲 → 协议解析 → 命令处理
                              ↓
                        舵机控制层 ← 参数管理
                              ↓
                        运动插值器
                              ↓
                        PWM硬件输出 → 18个舵机
```

## 📈 代码统计

### 文件统计
- **总文件数**: ~50个
- **头文件**: 25个
- **源文件**: 22个
- **配置文件**: 3个

### 代码量（估算）
- **核心代码**: ~3000行
- **注释**: ~800行
- **配置**: ~200行
- **总计**: ~4000行

### 模块分布
| 模块 | 文件数 | 代码行数 |
|------|--------|----------|
| PWM驱动 | 3 | ~400 |
| 舵机控制 | 2 | ~300 |
| 通信协议 | 8 | ~1200 |
| 运动控制 | 2 | ~400 |
| 存储管理 | 4 | ~400 |
| FreeRTOS任务 | 6 | ~300 |
| 工具模块 | 4 | ~350 |
| 主程序 | 1 | ~220 |

## 🚀 快速开始

### 1. 环境准备
```bash
# 安装PlatformIO
pip install platformio

# 克隆项目
git clone <repository>
cd 18ChannelServoDriver
```

### 2. 编译
```bash
pio run
```

### 3. 烧录
```bash
# 按住BOOTSEL按钮，连接USB
pio run --target upload
```

### 4. 测试
```bash
# 使用Python工具测试
python test_servo.py --ping
python test_servo.py --all 90 1000
```

## 📚 文档

- **README.md** - 项目说明和使用指南
- **BUILD_GUIDE.md** - 详细构建指南
- **功能需求文档.md** - 完整需求文档
- **PROJECT_OVERVIEW.md** (本文档) - 项目概览

## 🔍 关键技术点

### 1. 硬件PWM实现
- ✅ 使用RP2350的24个硬件PWM通道（使用18个）
- ✅ 由硬件定时器自动生成，不需要CPU干预
- ✅ **不受中断影响**，不会产生PWM抖动
- ✅ 精度：±0.5μs（2MHz时钟）
- ⚠️ PIO方案未使用（保留未来扩展）

**为什么不用PIO？**
- 硬件PWM已经足够：24通道 > 18路需求
- 硬件PWM更可靠：完全由硬件实现
- 代码更简单：不需要编写PIO汇编
- PIO留给未来其他用途（如特殊通信协议）

### 2. FreeRTOS双核
- Core 0: 通信和算法密集型任务
- Core 1: PWM实时输出，最小延迟
- 任务亲和性绑定确保性能

### 3. 批量命令优化
- MOVE_ALL一次控制18轴
- 通信效率提升3倍
- 减少USB开销

### 4. Flash参数管理
- 魔数+版本号+CRC保护
- 掉电不丢失
- 支持出厂复位

## ⚠️ 注意事项

1. **电源隔离**: 舵机电源必须独立，共地
2. **大电流**: 18个MG995峰值45A，需要大电流电源
3. **限位保护**: 软件限位避免舵机过载
4. **CRC校验**: 通信必须校验，防止错误指令
5. **急停功能**: 随时可紧急停止所有舵机

## 🛠️ 调试技巧

### 串口输出
系统启动时输出详细日志：
- 系统版本
- 初始化状态
- 任务创建信息

### LED指示
- **正常闪烁 (1s)**: 系统正常
- **快速闪烁 (50ms)**: 运动中
- **超快闪烁 (100ms)**: 错误状态

### Python工具
`test_servo.py` 提供完整的测试接口：
- 心跳测试
- 单轴/全轴控制
- 状态查询
- 参数保存/加载

## 📞 支持

如有问题，请参考：
1. README.md
2. BUILD_GUIDE.md
3. 功能需求文档.md
4. 提交Issue（如果是开源项目）

---

**项目状态**: ✅ Phase 1-5 完成，可用于生产测试

**下一步**: ROS2集成、压力测试、性能优化


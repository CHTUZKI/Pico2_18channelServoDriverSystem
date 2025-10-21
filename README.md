# **⚠️ 项目开发中 - 尚未完成 ⚠️**

## 18轴舵机控制系统

基于 Raspberry Pi Pico 2 (RP2350) 的18通道舵机驱动系统，使用FreeRTOS双核调度实现高精度PWM控制。

## 系统特性

- ✅ **18路硬件PWM输出** - 使用RP2350的24个硬件PWM通道，50Hz频率，0.5μs分辨率
- ✅ **USB CDC通信** - 虚拟串口，自定义协议，CRC-16校验
- ✅ **FreeRTOS双核** - Core 0处理通信/算法，Core 1负责状态监控
- ✅ **运动插值** - 线性和S曲线插值，支持多轴同步运动
- ✅ **Flash存储** - 舵机校准参数持久化
- ✅ **安全保护** - 急停、超时、限位保护机制
- ✅ **硬件可靠** - PWM由硬件定时器自动生成，不受中断影响

## 硬件要求

- **主控**: Raspberry Pi Pico 2 (RP2350)
- **舵机**: 18个 MG995 PWM舵机
- **电源**: 6V/20-30A 开关电源（舵机独立供电）
- **通信**: USB Type-C

## 引脚分配

| 功能 | GPIO | 说明 |
|------|------|------|
| Servo 0-17 | GPIO 0-17 | 18路PWM输出 |
| LED | GPIO 25 | 板载LED状态指示 |
| USB | USB接口 | 虚拟串口通信 |

## 编译与烧录

### 使用 PlatformIO

```bash
# 首次使用：安装依赖
pio pkg install

# 编译项目
pio run -e pico2

# 编译并上传（按住BOOTSEL按钮连接USB）
pio run -e pico2 --target upload

# 串口监视器
pio device monitor -e pico2
```

### 配置说明

项目配置文件：`platformio.ini`

关键配置：
- **平台**: 本地平台文件 (`platform-raspberrypi-develop/`)
- **板卡**: `rpipico2` (RP2350)
- **框架**: `picosdk` (Raspberry Pi Pico SDK)
- **库依赖**: FreeRTOS-Kernel (从GitHub自动下载)
- **构建脚本**: `scripts/add_freertos.py` (自定义FreeRTOS集成)
- **USB**: TinyUSB CDC (Pico SDK内置)

### 依赖说明

项目采用**自定义构建脚本**来集成FreeRTOS SMP：

**自动处理**：
- ✅ Pico SDK (本地平台包含)
- ✅ FreeRTOS-Kernel (PlatformIO从GitHub下载)
- ✅ 硬件PWM库 (hardware/pwm.h)
- ✅ Flash存储库 (hardware/flash.h)
- ✅ TinyUSB (USB CDC)

**构建脚本** (`scripts/add_freertos.py`) **自动完成**：
- 检测RP2350芯片并选择正确的FreeRTOS移植层
- 添加FreeRTOS头文件路径和编译定义
- 编译FreeRTOS核心组件（tasks, queue, timers等）
- 配置双核SMP支持
- 链接必要的Pico SDK组件（hardware_exception等）

**不需要手动配置任何依赖！PlatformIO和构建脚本会自动处理一切。**

## 通信协议

### 帧格式

```
[0xFF] [0xFE] [ID] [CMD] [LEN] [DATA...] [CRC_H] [CRC_L]
```

字段说明：
- **帧头**: 0xFF 0xFE (固定)
- **ID**: 设备ID (0x00=广播, 0x01-0xFE=单播)
- **CMD**: 命令字节
- **LEN**: 数据长度
- **DATA**: 数据内容
- **CRC**: CRC-16校验 (CCITT标准)

### 命令集

#### 位置控制
- **0x01** - `MOVE_SINGLE`: 单轴位置控制
  - 数据: `[ID] [角度高] [角度低] [速度高] [速度低]`
  
- **0x03** - `MOVE_ALL`: 全轴批量控制 (推荐)
  - 数据: `[角度1高] [角度1低] ... [角度18高] [角度18低] [速度高] [速度低]`

#### 查询命令
- **0x10** - `GET_SINGLE`: 查询单轴状态
- **0x11** - `GET_ALL`: 查询全轴状态

#### 配置命令
- **0x20** - `ENABLE`: 使能舵机
- **0x21** - `DISABLE`: 失能舵机

#### 存储命令
- **0x30** - `SAVE_FLASH`: 保存参数到Flash
- **0x31** - `LOAD_FLASH`: 从Flash加载参数
- **0x32** - `RESET_FACTORY`: 恢复出厂设置

#### 系统命令
- **0xFE** - `PING`: 心跳/连接检测
- **0xFF** - `ESTOP`: 紧急停止

## Python测试工具

项目包含Python测试脚本 `test_servo.py`，用于测试舵机控制。

### 安装依赖

```bash
pip install pyserial
```

### 使用示例

```python
# 连接设备
python test_servo.py

# 控制单个舵机
# 舵机0转到90度，速度1000ms
python test_servo.py --single 0 90 1000

# 控制所有舵机
# 所有舵机转到90度
python test_servo.py --all 90

# 查询状态
python test_servo.py --query
```

## 代码结构

```
Pico2_18channelServoDriverSystem/
├── platformio.ini              # PlatformIO配置文件
├── scripts/
│   └── add_freertos.py        # FreeRTOS自定义构建脚本
├── platform-raspberrypi-develop/  # 本地RP2350平台支持
├── pico-sdk-master/            # Pico SDK (本地)
├── src/                        # 源代码
│   ├── main.c                 # 主程序，FreeRTOS初始化
│   ├── FreeRTOSConfig.h       # FreeRTOS配置文件
│   ├── communication/         # 通信模块
│   │   ├── usb_handler.c/h   # USB CDC处理
│   │   ├── protocol.c/h      # 协议解析
│   │   ├── crc16.c/h         # CRC校验
│   │   └── commands.c/h      # 命令处理
│   ├── pwm/                   # PWM驱动
│   │   └── pwm_driver.c/h    # 硬件PWM控制
│   ├── servo/                 # 舵机控制
│   │   └── servo_control.c/h # 角度转换、限位
│   ├── motion/                # 运动控制
│   │   └── interpolation.c/h # 插值算法
│   ├── storage/               # 存储管理
│   │   ├── flash_storage.c/h # Flash读写
│   │   └── param_manager.c/h # 参数管理
│   ├── tasks/                 # FreeRTOS任务
│   │   ├── task_communication.c/h # 通信任务 (Core 0)
│   │   ├── task_motion.c/h        # 运动任务 (Core 0)
│   │   └── task_pwm.c/h           # PWM任务 (Core 1)
│   └── utils/                 # 工具模块
│       ├── ring_buffer.c/h   # 环形缓冲区
│       └── error_handler.c/h # 错误处理
├── include/                    # 头文件
│   ├── config/
│   │   ├── config.h          # 系统参数
│   │   └── pinout.h          # GPIO定义
│   └── [各模块头文件目录]
└── test_servo.py              # Python测试工具
```

## 性能指标

- **开发框架**: Pico SDK (非Arduino)
- **PWM方案**: RP2350硬件PWM（hardware/pwm.h，非PIO）
- **PWM频率**: 50Hz (20ms周期)
- **PWM分辨率**: ±0.5μs (125MHz时钟分频)
- **PWM稳定性**: 由硬件定时器自动生成，不受中断影响
- **RTOS**: FreeRTOS SMP (对称多处理，双核调度)
- **通信接口**: USB CDC (TinyUSB)
- **通信波特率**: 115200 bps
- **命令响应**: 单轴 < 20ms，批量 < 30ms
- **插值更新周期**: 20ms
- **系统时钟**: 150MHz (双核 Cortex-M33)

## 应用场景

本系统适用于：
- 6足机器人控制（每条腿3个舵机）
- 多关节机械臂
- 仿生机器人
- 机器人教育平台

## 常见问题

### Q: 舵机不动？
A: 检查：
1. 舵机电源是否连接（6V/大电流）
2. GPIO引脚连接是否正确
3. 舵机是否使能（发送ENABLE命令）
4. 检查串口通信是否正常

### Q: 舵机抖动？
A: 可能原因：
1. 电源供电不足
2. PWM信号干扰
3. 校准参数不正确

### Q: 通信无响应？
A: 检查：
1. USB连接是否正常
2. 波特率是否匹配（115200）
3. CRC校验是否正确
4. 帧格式是否正确

## 开发路线图

- [x] Phase 1: 基础PWM功能
- [x] Phase 2: 通信协议
- [x] Phase 3: 运动控制
- [x] Phase 4: 参数存储
- [x] Phase 5: 高级功能
- [ ] Phase 6: ROS2集成
- [ ] Phase 7: 测试与优化

## 许可证

MIT License

## 作者

18轴舵机控制系统 v1.0.0

---

**注意**: 舵机电源必须独立供电，峰值电流可达45A，请使用合适的电源！


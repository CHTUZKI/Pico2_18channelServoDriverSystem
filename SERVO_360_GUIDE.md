# 360度舵机使用指南

## 快速开始

### 1. 配置舵机类型
```c
// 在初始化时设置舵机类型
servo_manager_init();
servo_manager_set_type(0, SERVO_TYPE_CONTINUOUS_360);  // 设置ID 0为360度舵机
```

### 2. 基础速度控制
```c
// 速度范围：-100(满速反转) 到 +100(满速正转), 0=停止
servo_360_set_speed(0, 50);      // 50%速度正转
servo_360_set_speed(0, -30);     // 30%速度反转
servo_360_stop(0);               // 立即停止
servo_360_soft_stop(0);          // 平滑停止
```

### 3. 使能控制
```c
servo_360_enable(0, true);       // 使能舵机
servo_360_enable(0xFF, false);   // 禁用所有360度舵机
```

## 核心功能

### ✅ 已实现的保护功能
- **PWM限幅**：自动限制在500-2500μs
- **死区补偿**：跳过中点不转区域(±50μs)
- **加减速控制**：平滑启停，默认50%/s加速，80%/s减速
- **最小速度阈值**：速度<5%自动停止，防抖动
- **软停止**：指数衰减平滑停止
- **方向切换保护**：切换前延迟200ms
- **超时保护**：3秒无指令自动停止

### 调整加减速
```c
servo_360_set_acceleration(0, 80);   // 加速度80%/s
servo_360_set_deceleration(0, 100);  // 减速度100%/s
```

### 中点校准
```c
// 自动校准停止点
servo_360_calibrate_neutral(0, 5000);  // 超时5秒

// 或手动设置
servo_360_calibration_t cal = {
    .neutral_pulse_us = 1503,  // 实测停止点
    .min_pulse_us = 500,
    .max_pulse_us = 2500,
    .deadzone_us = 50,
    .reverse = false,
    .calibrated = true
};
servo_360_set_calibration(0, &cal);
```

## 位置估算（可选）

如果需要粗略定位功能：

```c
// 启用位置估算（需实测speed_to_dps系数）
servo_360_enable_position_estimate(0, 3.6f);  // 速度100% ≈ 360度/秒

// 简单位置控制
servo_360_goto_position(0, 90.0f, 5.0f);  // 转到90度，容差5度
```

## 统一管理器

混合使用180度和360度舵机：

```c
// 初始化
servo_manager_init();
servo_manager_set_type(0, SERVO_TYPE_POSITION_180);    // ID 0: 180度
servo_manager_set_type(1, SERVO_TYPE_CONTINUOUS_360);  // ID 1: 360度

// 统一控制
servo_manager_set_angle(0, 90.0f);   // 180度舵机设置角度
servo_manager_set_speed(1, 50);      // 360度舵机设置速度
servo_manager_stop(0xFF);            // 停止所有舵机
```

## 定期更新

在FreeRTOS任务中每20ms调用：

```c
void task_pwm(void *params) {
    while (1) {
        servo_360_update_all();  // 更新所有360度舵机
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

## 配置参数

在 `include/config/config.h` 中可调整：

```c
#define SERVO_360_DEADZONE_US           50      // 死区大小
#define SERVO_360_MIN_SPEED_THRESHOLD   5       // 最小速度阈值
#define SERVO_360_DEFAULT_ACCEL         50      // 默认加速度
#define SERVO_360_DEFAULT_DECEL         80      // 默认减速度
#define SERVO_360_DIRECTION_CHANGE_DELAY_MS  200  // 方向切换延迟
#define SERVO_360_TIMEOUT_MS            3000    // 超时时间
```

## 注意事项

⚠️ **360度舵机是开环控制**
- 只能控制速度，不能精确定位
- 位置估算会有累积误差
- 适合轮式移动、传送带等应用
- 不适合需要精确角度的关节控制

✅ **适用场景**
- 移动底盘驱动轮
- 传送带/输送机构
- 持续旋转的机构
- 不需要精确位置的应用


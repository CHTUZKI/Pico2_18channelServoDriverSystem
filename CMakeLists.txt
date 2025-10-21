cmake_minimum_required(VERSION 3.13)

# ============================================
# Pico SDK 初始化
# ============================================
# 设置SDK路径（PlatformIO会自动设置环境变量）
if(NOT DEFINED PICO_SDK_PATH)
    set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
endif()

# 包含Pico SDK
include(${PICO_SDK_PATH}/pico_sdk_init.cmake)

# ============================================
# 项目配置
# ============================================
project(ServoDriver C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# 初始化SDK
pico_sdk_init()

# ============================================
# FreeRTOS-Kernel 配置
# ============================================
# FreeRTOS路径
set(FREERTOS_KERNEL_PATH ${CMAKE_CURRENT_LIST_DIR}/.pio/libdeps/pico2/FreeRTOS-Kernel)

# FreeRTOS配置接口库
add_library(freertos_config INTERFACE)
target_include_directories(freertos_config SYSTEM INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

# FreeRTOS SMP配置
target_compile_definitions(freertos_config INTERFACE
    FREERTOS_KERNEL_SMP=1
)

# 包含FreeRTOS的RP2040 SMP移植层
include(${FREERTOS_KERNEL_PATH}/portable/ThirdParty/Community-Supported-Ports/GCC/RP2040/FreeRTOS_Kernel_import.cmake)

# ============================================
# 项目源文件
# ============================================
# 手动列出所有源文件（比GLOB更可靠）
set(PROJECT_SOURCES
    # 主程序
    src/main.c
    
    # PWM驱动
    src/pwm/pwm_driver.c
    
    # 舵机控制
    src/servo/servo_control.c
    
    # 通信模块
    src/communication/usb_handler.c
    src/communication/protocol.c
    src/communication/crc16.c
    src/communication/commands.c
    
    # 运动控制
    src/motion/interpolation.c
    
    # 存储管理
    src/storage/flash_storage.c
    src/storage/param_manager.c
    
    # FreeRTOS任务
    src/tasks/task_communication.c
    src/tasks/task_motion.c
    src/tasks/task_pwm.c
    
    # 工具模块
    src/utils/error_handler.c
    src/utils/ring_buffer.c
)

# ============================================
# 项目头文件目录
# ============================================
set(PROJECT_INCLUDES
    ${CMAKE_CURRENT_LIST_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/include/config
    ${CMAKE_CURRENT_LIST_DIR}/include/pwm
    ${CMAKE_CURRENT_LIST_DIR}/include/servo
    ${CMAKE_CURRENT_LIST_DIR}/include/communication
    ${CMAKE_CURRENT_LIST_DIR}/include/motion
    ${CMAKE_CURRENT_LIST_DIR}/include/storage
    ${CMAKE_CURRENT_LIST_DIR}/include/tasks
    ${CMAKE_CURRENT_LIST_DIR}/include/utils
)

# ============================================
# 创建可执行文件
# ============================================
add_executable(${PROJECT_NAME}
    ${PROJECT_SOURCES}
)

# 添加头文件路径
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_INCLUDES}
)

# ============================================
# 链接库
# ============================================
target_link_libraries(${PROJECT_NAME}
    # Pico SDK库
    pico_stdlib              # 标准库
    pico_multicore          # 多核支持
    hardware_pwm            # PWM硬件
    hardware_flash          # Flash存储
    hardware_sync           # 同步原语
    hardware_irq            # 中断控制
    
    # FreeRTOS-Kernel
    FreeRTOS-Kernel-Heap4   # FreeRTOS堆管理（Heap4）
)

# ============================================
# 编译选项
# ============================================
target_compile_options(${PROJECT_NAME} PRIVATE
    -Wall
    -Wextra
    -Wno-unused-parameter
    -Wno-unused-variable
    -O2
    -g                      # 调试信息
)

# ============================================
# USB输出配置
# ============================================
# 使能USB串口输出（虚拟串口）
pico_enable_stdio_usb(${PROJECT_NAME} 1)
# 禁用UART输出
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# ============================================
# 生成额外输出文件
# ============================================
# 生成 UF2 文件（用于拖放烧录）
pico_add_extra_outputs(${PROJECT_NAME})

# ============================================
# 构建信息打印
# ============================================
message(STATUS "==========================================")
message(STATUS "18通道舵机控制系统 - CMake配置")
message(STATUS "==========================================")
message(STATUS "项目名称: ${PROJECT_NAME}")
message(STATUS "目标板卡: Raspberry Pi Pico 2 (RP2350)")
message(STATUS "FreeRTOS: ${FREERTOS_KERNEL_PATH}")
message(STATUS "编译器: ${CMAKE_C_COMPILER}")
message(STATUS "构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "==========================================")


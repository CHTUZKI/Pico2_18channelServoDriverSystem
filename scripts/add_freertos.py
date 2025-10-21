"""
PlatformIO Build Script - Add FreeRTOS-Kernel SMP Support
Mimics the picosdk.py implementation style
"""
from os.path import join, isdir, isfile
from SCons.Script import Import
import sys

Import("env")

print("=" * 60)
print("Adding FreeRTOS-Kernel SMP to build")
print("=" * 60)

# Get FreeRTOS library path
# Build path: PROJECT_DIR/.pio/libdeps/PIOENV/FreeRTOS-Kernel
project_dir = env.subst("$PROJECT_DIR")
env_name = env.subst("$PIOENV")
FREERTOS_DIR = join(project_dir, ".pio", "libdeps", env_name, "FreeRTOS-Kernel")

# Check if FreeRTOS exists
if not isdir(FREERTOS_DIR):
    sys.stderr.write("ERROR: FreeRTOS-Kernel library not found!\n")
    sys.stderr.write("Path: %s\n" % FREERTOS_DIR)
    sys.stderr.write("Please ensure lib_deps contains FreeRTOS-Kernel\n")
    env.Exit(1)

print("Found FreeRTOS-Kernel: %s" % FREERTOS_DIR)

# Detect RP2040 or RP2350
board = env.BoardConfig()
mcu = board.get('build.mcu')
is_rp2350 = "rp2350" in mcu if mcu else False

print("Target MCU: %s" % mcu)
print("SMP Mode: %s" % ("RP2350 Dual-Core" if is_rp2350 else "RP2040 Dual-Core"))

# FreeRTOS port layer path - use RP2350 or RP2040 specific port
# RP2350 has dedicated ports: RP2350_ARM_NTZ (ARM) and RP2350_RISC-V (RISC-V)
if is_rp2350:
    # Use RP2350 ARM Non-TrustZone port
    FREERTOS_PORT_DIR = join(FREERTOS_DIR, "portable", "ThirdParty", "Community-Supported-Ports", "GCC", "RP2350_ARM_NTZ", "non_secure")
    print("Using RP2350_ARM_NTZ port for RP2350")
else:
    # Use RP2040 port
    FREERTOS_PORT_DIR = join(FREERTOS_DIR, "portable", "ThirdParty", "Community-Supported-Ports", "GCC", "RP2040")
    print("Using RP2040 port")

# Check if port layer exists
if not isdir(FREERTOS_PORT_DIR):
    sys.stderr.write("ERROR: FreeRTOS port layer not found!\n")
    sys.stderr.write("Expected path: %s\n" % FREERTOS_PORT_DIR)
    env.Exit(1)

print("Found port layer: %s" % FREERTOS_PORT_DIR)

# Add FreeRTOS header paths (mimicking picosdk.py style)
# RP2350 port has portmacro.h directly in non_secure/ directory (no include subdir)
env.Append(
    CPPPATH=[
        # Project src directory (for FreeRTOSConfig.h)
        join(project_dir, "src"),
        
        # FreeRTOS core headers
        join(FREERTOS_DIR, "include"),
        
        # RP2040/RP2350 SMP port headers (portmacro.h is here)
        FREERTOS_PORT_DIR,
    ],
    
    CPPDEFINES=[
        # FreeRTOS SMP configuration
        ("FREERTOS_KERNEL_SMP", 1),
        ("LIB_FREERTOS_KERNEL", 1),
        ("configNUMBER_OF_CORES", 2),
        
        # Pico SDK multicore support (required by FreeRTOS port)
        ("LIB_PICO_MULTICORE", 1),
    ]
)

print("Added header paths:")
print("   - %s (FreeRTOSConfig.h)" % join(project_dir, "src"))
print("   - %s" % join(FREERTOS_DIR, "include"))
print("   - %s (portmacro.h)" % FREERTOS_PORT_DIR)

# Build FreeRTOS core source files (mimicking picosdk.py BuildSources approach)
# Core component list
freertos_core_components = [
    ("tasks", "+<tasks.c>"),
    ("queue", "+<queue.c>"),
    ("list", "+<list.c>"),
    ("timers", "+<timers.c>"),
    ("event_groups", "+<event_groups.c>"),
    ("stream_buffer", "+<stream_buffer.c>"),
]

print("Building FreeRTOS core components:")
for component_name, src_filter in freertos_core_components:
    comp_src = join(FREERTOS_DIR, component_name + ".c")
    if isfile(comp_src):
        env.BuildSources(
            join("$BUILD_DIR", "FreeRTOS_" + component_name),
            FREERTOS_DIR,
            src_filter
        )
        print("   + %s" % component_name)
    else:
        # File might be in root directory
        env.BuildSources(
            join("$BUILD_DIR", "FreeRTOS"),
            FREERTOS_DIR,
            src_filter
        )
        print("   + %s (root)" % component_name)

# Build memory management (Heap 4 - supports malloc/free)
print("Building memory management:")
env.BuildSources(
    join("$BUILD_DIR", "FreeRTOS_MemMang"),
    join(FREERTOS_DIR, "portable", "MemMang"),
    "+<heap_4.c>"
)
print("   + heap_4 (dynamic allocation)")

# Build RP2040/RP2350 SMP port layer
print("Building RP2040/RP2350 SMP port:")
env.BuildSources(
    join("$BUILD_DIR", "FreeRTOS_Port_RP2040"),
    FREERTOS_PORT_DIR,
    "+<*.c>"  # port.c and other files
)
print("   + port.c (SMP port layer)")

# Build additional Pico SDK components needed by FreeRTOS port
print("Building required Pico SDK components:")

# Get Pico SDK path from platform
platform = env.PioPlatform()
FRAMEWORK_DIR = platform.get_package_dir("framework-picosdk")

if FRAMEWORK_DIR and isdir(FRAMEWORK_DIR):
    # hardware_exception is needed by FreeRTOS port for exception_set_exclusive_handler
    env.BuildSources(
        join("$BUILD_DIR", "PicoSDKFreeRTOS_hardware_exception"),
        join(FRAMEWORK_DIR, "src", "rp2_common", "hardware_exception"),
        "+<*.c>"
    )
    print("   + hardware_exception (exception handlers)")

print("=" * 60)
print("FreeRTOS-Kernel SMP configuration complete!")
print("   - Core files: 6")
print("   - Memory: heap_4")
print("   - Port: RP2350 ARM NTZ")
print("   - Dual-core: Enabled")
print("   - SDK components: hardware_exception")
print("=" * 60)


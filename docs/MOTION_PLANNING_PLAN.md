# Motion Planning Implementation Plan

## Overview
Implement trapezoid velocity curve and multi-point trajectory planning (up to 50 points) for 18-channel servo control system.

## Goals
1. **Trapezoid Velocity Curve**: Enable velocity and acceleration control instead of time-based motion
2. **Multi-Point Trajectory**: Support up to 50 waypoints with automatic trajectory planning

---

## Phase 1: Data Structure Design

### 1.1 Motion Parameters Structure
```c
typedef struct {
    float max_velocity;      // Maximum velocity (degrees/second)
    float acceleration;      // Acceleration (degrees/second^2)
    float deceleration;      // Deceleration (degrees/second^2)
    interp_type_t curve;     // Curve type: LINEAR, S_CURVE, TRAPEZOID
} motion_params_t;
```

### 1.2 Trajectory Point Structure
```c
#define MAX_TRAJECTORY_POINTS 50

typedef struct {
    float position;          // Target position (degrees)
    float velocity;          // Velocity at this point (optional, 0 = use default)
    uint8_t blend_mode;      // 0 = stop, 1 = blend to next point
} trajectory_point_t;

typedef struct {
    trajectory_point_t points[MAX_TRAJECTORY_POINTS];
    uint8_t point_count;     // Current number of points
    uint8_t current_index;   // Current executing point
    bool is_running;         // Trajectory execution status
} trajectory_queue_t;
```

### 1.3 Extended Interpolator Structure
```c
typedef struct {
    // Existing fields
    float start_pos;
    float target_pos;
    float current_pos;
    // ... existing fields ...
    
    // New fields for trapezoid curve
    float max_velocity;      // Maximum velocity (degrees/second)
    float acceleration;      // Acceleration (degrees/second^2)
    float current_velocity;  // Current velocity (degrees/second)
    
    // Trapezoid curve phases
    uint32_t t_accel;        // Acceleration time (ms)
    uint32_t t_const;        // Constant velocity time (ms)
    uint32_t t_decel;        // Deceleration time (ms)
    
    // Phase tracking
    uint8_t current_phase;   // 0=accel, 1=const, 2=decel
} interpolator_t;
```

---

## Phase 2: Trapezoid Velocity Curve Algorithm

### 2.1 Motion Planning Algorithm

**Input**: 
- Start position: `P0`
- Target position: `P1`
- Max velocity: `Vmax` (deg/s)
- Acceleration: `A` (deg/s²)

**Calculate**:
1. Distance: `D = |P1 - P0|`
2. Acceleration distance: `Da = Vmax² / (2A)`
3. Check if can reach Vmax:
   - If `D >= 2Da`: Full trapezoid (accel → const → decel)
   - If `D < 2Da`: Triangle (accel → decel, no const phase)

**Full Trapezoid**:
```
t_accel = Vmax / A
t_decel = Vmax / A
t_const = (D - 2*Da) / Vmax
```

**Triangle (limited by distance)**:
```
Vmax_actual = sqrt(D * A)
t_accel = Vmax_actual / A
t_decel = Vmax_actual / A
t_const = 0
```

### 2.2 Position Calculation During Motion

**Acceleration Phase** (0 ≤ t < t_accel):
```
P(t) = P0 + 0.5 * A * t²
V(t) = A * t
```

**Constant Velocity Phase** (t_accel ≤ t < t_accel + t_const):
```
t' = t - t_accel
P(t) = P0 + Da + Vmax * t'
V(t) = Vmax
```

**Deceleration Phase** (t_accel + t_const ≤ t < total_time):
```
t' = t - (t_accel + t_const)
t_remain = t_decel - t'
P(t) = P1 - 0.5 * A * t_remain²
V(t) = A * t_remain
```

---

## Phase 3: Multi-Point Trajectory Planning

### 3.1 Trajectory Queue Management
- Add point: Check queue not full
- Remove point: Shift remaining points
- Clear queue: Reset all points
- Start execution: Begin from point 0

### 3.2 Trajectory Execution Flow
```
1. Start at point[0]
2. Plan motion: point[i] → point[i+1]
3. Execute motion with trapezoid curve
4. Check blend_mode:
   - If blend=0: Stop at point, wait for next command
   - If blend=1: Smoothly transition to next point
5. Move to next point (i++)
6. Repeat until all points executed
```

### 3.3 Velocity Blending Between Points
For smooth transitions between waypoints:
- Calculate exit velocity at current point
- Calculate entry velocity at next point
- Use S-curve or polynomial for velocity transition

---

## Phase 4: Communication Protocol Extension

### 4.1 New Commands

**CMD_MOVE_TRAPEZOID (0x30)**
```c
// Payload: [servo_id(1)] [target_angle(2)] [max_vel(2)] [accel(2)]
// Example: Move servo 5 to 90° with 45°/s, 90°/s²
```

**CMD_TRAJECTORY_ADD_POINT (0x31)**
```c
// Payload: [servo_id(1)] [point_index(1)] [angle(2)] [blend_mode(1)]
```

**CMD_TRAJECTORY_START (0x32)**
```c
// Payload: [servo_id(1)] [max_vel(2)] [accel(2)]
```

**CMD_TRAJECTORY_CLEAR (0x33)**
```c
// Payload: [servo_id(1)]
```

**CMD_TRAJECTORY_STATUS (0x34)**
```c
// Query trajectory execution status
// Response: [point_count(1)] [current_index(1)] [is_running(1)]
```

### 4.2 Protocol Format
All commands follow existing protocol:
```
[STX(1)] [LEN(2)] [CMD(1)] [PAYLOAD(N)] [CRC16(2)]
```

---

## Phase 5: Upper Computer UI Extension

### 5.1 Velocity Control Panel
- Add sliders: Max Velocity (0-180°/s)
- Add sliders: Acceleration (0-360°/s²)
- Add dropdown: Curve Type (Linear/S-Curve/Trapezoid)

### 5.2 Trajectory Editor Interface
- Table view: Point list (position, velocity, blend mode)
- Buttons: Add Point, Delete Point, Clear All
- Buttons: Load Trajectory, Save Trajectory
- Execute button: Start trajectory with parameters
- Status display: Current point, progress

---

## Phase 6: Implementation Steps

### Step 1: Core Algorithm (MCU)
1. Extend interpolator structure ✓
2. Implement trapezoid planning function ✓
3. Implement trapezoid update function ✓
4. Add trajectory queue structure ✓
5. Implement queue management functions ✓

### Step 2: Integration (MCU)
1. Add new command handlers ✓
2. Integrate with AO_Motion ✓
3. Add debug output for verification ✓
4. Test single-axis trapezoid ✓
5. Test multi-axis synchronized trapezoid ✓

### Step 3: Upper Computer (Python)
1. Update protocol definitions ✓
2. Add new command functions ✓
3. Create velocity control UI ✓
4. Create trajectory editor UI ✓
5. Add visualization (optional) ✓

### Step 4: Testing
1. Single servo trapezoid test
2. Multi-servo synchronized test
3. Multi-point trajectory test (5/20/50 points)
4. Performance and memory analysis
5. Edge case testing

---

## Memory Analysis

### Trajectory Queue Memory
```
Per servo:
- 50 points × 6 bytes = 300 bytes
- Queue metadata = 4 bytes
Total per servo = 304 bytes

All 18 servos:
- 18 × 304 = 5,472 bytes (~5.3 KB)
```

### Total System Memory (RP2350: 264 KB SRAM)
- Current usage: ~50 KB
- Trajectory queues: ~5.5 KB
- Remaining: ~208 KB ✓ Plenty of space

---

## Performance Analysis

### CPU Load Estimation
- Interpolation period: 20ms (50 Hz)
- Trapezoid calculation: ~100 CPU cycles
- 18 servos: ~1,800 cycles
- RP2350 @ 150 MHz: 0.012 ms
- **CPU usage: < 0.1%** ✓ Very low

---

## Risk Assessment

### Low Risk
- ✓ Memory sufficient
- ✓ CPU performance adequate
- ✓ Architecture supports extension
- ✓ Existing S-curve can be reused

### Medium Risk
- ⚠ Complex trajectory blending algorithm
- ⚠ Multi-point synchronization accuracy
- ⚠ UI complexity increases

### Mitigation
- Start with simple trajectory (no blending)
- Extensive testing before adding blending
- Modular UI design

---

## Success Criteria

1. ✓ Single servo can execute trapezoid velocity curve
2. ✓ Velocity and acceleration match specified values (±5%)
3. ✓ 18 servos can synchronize with trapezoid curves
4. ✓ Can store and execute 50-point trajectory
5. ✓ Upper computer can edit and send trajectories
6. ✓ System remains stable under continuous operation
7. ✓ Response time < 50ms for command processing

---

## Timeline Estimate

- **Phase 1-2**: Data structures + Trapezoid algorithm (2-3 hours)
- **Phase 3**: Multi-point trajectory (2 hours)
- **Phase 4**: Protocol extension (1 hour)
- **Phase 5**: UI development (2-3 hours)
- **Phase 6**: Testing and optimization (2 hours)

**Total: ~10-12 hours of development**

---

## Next Actions

1. Start with Phase 1: Extend interpolator.h
2. Implement trapezoid planning algorithm
3. Test with single servo
4. Expand to multi-point trajectory
5. Integrate with upper computer

Let's begin! 🚀


# 调试工具使用说明

## 日志系统架构

### 1. 日志文件 (`logs/motor_control_latest.log`)
- **最详细的调试信息**（DEBUG级别）
- **每次运行自动覆盖旧日志**
- **记录所有操作、数据流、状态变化**
- **供AI深度调试和问题追踪**

### 2. 应用程序日志窗口
- **INFO级别及以上**
- **显示重要的应用操作**
- **不显示状态报告**（避免刷屏）

### 3. 串口通信日志窗口
- **完整的串口工具**
- **显示所有发送和接收的数据**
- **包括状态报告**

---

## 快速调试工具

### 方法1：命令行快速查看

```bash
# 查看日志摘要（总行数、错误数、警告数等）
python quick_debug.py

# 查看所有错误
python quick_debug.py error

# 查看所有警告
python quick_debug.py warning

# 查看状态报告
python quick_debug.py status

# 查看G代码操作
python quick_debug.py gcode

# 搜索关键词
python quick_debug.py search "串口"
python quick_debug.py search "部件"

# 查看最后50行
python quick_debug.py last 50

# 查看指定范围（如100-200行）
python quick_debug.py range 100 200

# 按日志级别过滤
python quick_debug.py level ERROR
python quick_debug.py level INFO
```

### 方法2：Python代码中使用

```python
from tools.log_analyzer import LogAnalyzer

# 创建分析器
analyzer = LogAnalyzer()

# 快速摘要
analyzer.quick_summary()

# 搜索错误
errors = analyzer.search_error()
for line_num, content in errors:
    print(f"行{line_num}: {content}")

# 搜索关键词
results = analyzer.search("串口连接")
analyzer.print_results(results)

# 获取最后100行
last_lines = analyzer.get_last_n_lines(100)

# 获取指定范围
range_lines = analyzer.get_lines_range(100, 200)

# 按级别过滤
debug_logs = analyzer.filter_by_level('DEBUG')
info_logs = analyzer.filter_by_level('INFO')
```

### 方法3：交互式模式

```bash
python quick_debug.py interactive
```

然后可以输入命令：
```
> search 状态报告
> error
> last 50
> range 100 200
> quit
```

---

## 日志搜索常用场景

### 调试串口通信问题
```bash
python quick_debug.py search "串口"
python quick_debug.py search "接收"
python quick_debug.py search "发送"
```

### 调试G代码生成问题
```bash
python quick_debug.py gcode
python quick_debug.py search "G1"
python quick_debug.py search "生成"
```

### 调试组件操作问题
```bash
python quick_debug.py component
python quick_debug.py search "正转"
python quick_debug.py search "添加部件"
```

### 调试状态报告问题
```bash
python quick_debug.py status
python quick_debug.py search "Idle"
python quick_debug.py search "MPos"
```

### 查看错误和警告
```bash
python quick_debug.py error
python quick_debug.py warning
```

---

## Python REPL中快速调试

```python
# 在Python REPL中
from tools.log_analyzer import LogAnalyzer
a = LogAnalyzer()

# 快速查看
a.quick_summary()

# 搜索
a.print_results(a.search("串口"))

# 最后10行
for line in a.get_last_n_lines(10):
    print(line)
```

---

## 日志级别说明

- **DEBUG** - 详细的调试信息（原始字节、解析过程等）
- **INFO** - 一般信息（连接成功、操作完成等）
- **WARNING** - 警告信息
- **ERROR** - 错误信息

---

## 提示

1. **日志文件很大时**，使用 `range` 或 `level` 过滤
2. **搜索时**，可以使用正则表达式
3. **查看实时日志**，使用 `tail -f logs/motor_control_latest.log` (Linux/Mac) 或在编辑器中打开
4. **AI调试时**，先运行 `python quick_debug.py` 查看摘要，再针对性搜索


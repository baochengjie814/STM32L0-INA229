# INA229 功率计 — 串口通讯协议

## 物理层

| 参数 | 值 |
|------|-----|
| 接口 | USART2 (PA2 TX, PA3 RX) |
| 波特率 | 2,000,000 bps |
| 数据位 | 8 |
| 停止位 | 1 |
| 校验 | 无 |
| 流控 | 无 |
| 帧尾 | `\r\n` (回车换行) |

## 命令列表

### CT=N — 设置 ADC 转换时间

```
发送: CT=5\r\n
响应: OK\r\n
```

| N | 单通道耗时 | 3通道总耗时 | 适用场景 |
|---|----------|------------|---------|
| 0 | 50 μs | 150 μs | 最高速，噪声大 |
| 1 | 84 μs | 252 μs | |
| 2 | 150 μs | 450 μs | |
| 3 | 280 μs | 840 μs | |
| 4 | 540 μs | 1.6 ms | |
| **5** | **1052 μs** | **3.2 ms** | **默认，中等折中** |
| 6 | 2074 μs | 6.2 ms | |
| 7 | 4120 μs | 12.4 ms | 最低噪声 |

### AVG=N — 设置平均次数

```
发送: AVG=2\r\n
响应: OK\r\n
```

| N | 次数 | 当前总周期 (CT5) | 等效采样率 |
|---|-----|-----------------|-----------|
| 0 | 1 | 3.2 ms | ~300 Hz |
| 1 | 4 | 12.6 ms | ~80 Hz |
| **2** | **16** | **50.5 ms** | **~20 Hz (默认)** |
| 3 | 64 | 202 ms | ~5 Hz |
| 4 | 128 | 404 ms | ~2.5 Hz |
| 5 | 256 | 807 ms | ~1.2 Hz |
| 6 | 512 | 1.6 s | ~0.6 Hz |
| 7 | 1024 | 3.2 s | ~0.3 Hz |

> 公式: 总周期 = CT_time × 3(通道) × AVG_count
> MCU 读取速率自动跟随, 无需额外配置

### LCD=ON / LCD=OFF — 开关 LCD 刷新

```
发送: LCD=OFF\r\n
响应: OK\r\n

发送: LCD=ON\r\n
响应: OK\r\n
```

关闭后 LCD 不再刷新, 可降低 CPU 负载和 SPI 带宽占用。

### STATUS — 查询当前配置

```
发送: STATUS\r\n
响应:
CT=5\r\n
AVG=2\r\n
LCD=ON\r\n
```

### HELP — 显示帮助

```
发送: HELP\r\n
响应:
Commands:\r\n
  CT=0..7   Set conversion time\r\n
  AVG=0..7  Set averaging\r\n
  LCD=ON|OFF  Toggle LCD\r\n
  STATUS    Show status\r\n
```

## 错误处理

| 响应 | 含义 |
|------|------|
| `OK\r\n` | 命令执行成功 |
| `ERR\r\n` | 格式错误或参数越界 |

## 典型用法示例

```
# 1. 查看当前状态
STATUS
→ CT=5  AVG=2  LCD=ON

# 2. 切换到高速模式 (300Hz)
CT=0
AVG=0
→ OK, 采样率 ≈300Hz

# 3. 关掉 LCD 减少干扰 (纯数据采集模式)
LCD=OFF

# 4. 切回默认
CT=5
AVG=2
LCD=ON
```

## 数据帧 (自动推送)

串口同时会周期性输出测量数据 (由 `INA229_PRINTF_ENABLE` 宏控制):

```
[INA229] Vsh=123.400uV  Vbus=12000.000mV  I=6.170mA  P=74.040mW  T=28.600dC
```

| 字段 | 含义 | 单位 |
|------|------|------|
| Vsh | 分流电压 | μV |
| Vbus | 总线电压 | mV |
| I | 电流 | mA |
| P | 功率 | mW |
| T | 芯片温度 | dC (0.1°C) |

### MODE=TEXT / MODE=FLOAT — 切换输出格式

```
发送: MODE=FLOAT\r\n
响应: OK\r\n
```

| 模式 | 帧大小 | 适用场景 |
|------|--------|----------|
| `MODE=TEXT` (默认) | ~100 字节 | 人类可读, 调试 |
| `MODE=FLOAT` | 24 字节 | 高频采集, 带宽节省 4x |

**JUSTFLOAT 帧格式** (MODE=FLOAT, 共 24 字节):

```
字节 0~3:   Vshunt   float32 小端序 (V)
字节 4~7:   Vbus     float32 小端序 (V)
字节 8~11:  Current  float32 小端序 (A)
字节 12~15: Power    float32 小端序 (W)
字节 16~19: Temp     float32 小端序 (°C)
字节 20~23: 帧尾     0x00 0x00 0x80 0x7F (+Inf 小端)
```

解析示例 (Python):
```python
import struct
frame = ser.read(24)
vsh, vbus, curr, power, temp = struct.unpack('<5f', frame[:20])
print(f'{vsh=:.6f} {vbus=:.3f} {curr=:.6f} {power=:.3f} {temp=:.1f}')
```

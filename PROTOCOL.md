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
| 模式 | DMA+IDLE 接收 (帧尾 `\r\n`)，DMA 发送 |

## 命令列表

### CT=N — ADC 转换时间

| N | 单通道耗时 | 3通道总耗时 |
|---|----------|------------|
| 0 | 50 μs | 150 μs |
| 1 | 84 μs | 252 μs |
| 2 | 150 μs | 450 μs |
| 3 | 280 μs | 840 μs |
| 4 | 540 μs | 1.6 ms |
| 5 | 1052 μs | 3.2 ms |
| **6** | **2074 μs** | **6.2 ms (默认)** |
| 7 | 4120 μs | 12.4 ms |

### AVG=N — 平均次数

| N | 次数 | 默认周期 (CT6) | 等效采样率 |
|---|-----|---------------|-----------|
| 0 | 1 | 6.2 ms | ~160 Hz |
| 1 | 4 | 24.9 ms | ~40 Hz |
| **2** | **16** | **99.6 ms** | **~10 Hz (默认)** |
| 3 | 64 | 398 ms | ~2.5 Hz |
| 4 | 128 | 796 ms | ~1.25 Hz |
| 5 | 256 | 1.6 s | ~0.6 Hz |
| 6 | 512 | 3.2 s | ~0.3 Hz |
| 7 | 1024 | 6.4 s | ~0.16 Hz |

> 公式: 总周期 = CT_time × 3通道 × AVG_count
> MCU 状态机自动跟随，可通过 UART 实时修改

### LCD=ON / LCD=OFF — LCD 刷新开关

```
发送: LCD=OFF\r\n → OK\r\n
发送: LCD=ON\r\n  → OK\r\n
```

关闭后 LCD 不刷新, 降低 SPI/CPU 负载。JUSTFLOAT 模式下还会跳过温度 SPI 读以提速。

### MODE=TEXT / MODE=FLOAT — 输出格式

```
发送: MODE=FLOAT\r\n → OK\r\n
发送: MODE=TEXT\r\n  → OK\r\n
```

| 模式 | 格式 | 帧大小 | 最高速率 | 适用 |
|------|------|--------|----------|------|
| TEXT (默认) | ASCII 可读 | ~90 字节 | 200 Hz | 调试 |
| FLOAT | JUSTFLOAT 二进制 | 24 字节 | 5 kHz | 高速采集 |

### STATUS — 查询当前配置

```
→ CT=6  AVG=2  LCD=ON  MODE=TEXT\r\n
```

### HELP — 命令帮助

```
→ Commands:\r\n
  CT=0..7     Set conversion time\r\n
  AVG=0..7    Set averaging\r\n
  LCD=ON|OFF  Toggle LCD\r\n
  MODE=TEXT|FLOAT  Output format\r\n
  STATUS      Show status\r\n
```

### 错误处理

| 响应 | 含义 |
|------|------|
| `OK\r\n` | 命令执行成功 |
| `ERR\r\n` | 格式错误或参数越界 |

## 数据帧

### TEXT 模式 (自动推送, 由 INA229_PRINTF_ENABLE 宏控制)

```
[INA229]:Vbus=12000.000mV,I=6.170mA,P=74.040mW,T=28.6dC\r\n
FPS=10\r\n
```

| 字段 | 含义 | 单位 |
|------|------|------|
| Vbus | 总线电压 | mV |
| I | 电流 | mA |
| P | 功率 | mW |
| T | 温度 | dC (0.1°C) |
| FPS | 每秒实测帧数 | — |

### JUSTFLOAT 帧格式 (MODE=FLOAT, 24 字节)

```
字节 0~3:   Vbus     float32 LE (V)
字节 4~7:   Current  float32 LE (A)
字节 8~11:  Power    float32 LE (W)
字节 12~15: Temp     float32 LE (°C)
字节 16~19: FPS      float32 LE (Hz)
字节 20~23: 帧尾     0x00 0x00 0x80 0x7F
```

 Python 解析:
```python
import struct
frame = ser.read(24)
vbus, curr, pwr, temp, fps = struct.unpack('<5f', frame[:20])
print(f'Vbus={vbus:.3f}V  I={curr:.3f}A  P={pwr:.3f}W  T={temp:.1f}C  FPS={fps:.0f}')
```

## 典型用法

```
# 调试 → 文本模式
MODE=TEXT
CT=6
AVG=2
LCD=ON

# 高速采集 → 二进制模式
MODE=FLOAT
CT=0
AVG=0
LCD=OFF
→ 5 kHz, 24B/帧, ~960 kbps
```

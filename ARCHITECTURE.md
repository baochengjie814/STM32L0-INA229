# INA229_POWER_MMT 项目架构文档

> 更新: 2026-07-27   MCU: STM32L030G6U6 (Cortex-M0+, 32 MHz)
> IDE: Keil MDK-ARM v5 (ARMCC V5.06) | CubeMX HAL

---

## 一、用途

基于 TI INA229 (85V/20位 SPI 功率监控器) 的功率测量仪表。软 SPI 采集 Vbus/I/P/Temp，ST7789V LCD (240×135) 实时显示，USART2 2Mbps 支持 TEXT/JUSTFLOAT 双模式输出 + 远程命令配置。

---

## 二、硬件外设

| 外设 | 引脚 | 用途 |
|------|------|------|
| 软 SPI | PB0(SCK), PA9(MOSI), PB1(MISO), PA10(CS) | INA229 通信 (Mode 1) |
| SPI1 (硬件) | PA5(SCK), PA7(MOSI), PA4(CS), PA6(RS) | LCD 显示 |
| USART2 | PA2(TX), PA3(RX) | 调试 + 命令 + 数据 (DMA RX/TX) |
| TIM2 | 内部 | 10 kHz 系统时基 (100 μs tick) |

---

## 三、目录结构

```
USER/
├── INA229/
│   ├── ina229.h/.c          # 驱动: 寄存器读写/测量/配置/Init(error检测)
│   ├── soft_spi.h/.c        # 驱动: 软 SPI (HAL GPIO + 内联展开, 零延时)
│   └── INA229_Task.h/.c     # 应用: 动态速率状态机 + DMA 串口发送 + FPS 统计
├── LcdControl/
│   ├── lcd_drv.h/.c         # ST7789V 240×135, 硬件 SPI1 缓冲发送
│   ├── lcd_mid.h/.c         # 差量刷新 + 自动单位切换
│   ├── lcd_task.h/.c        # 10 Hz 状态机 (可由 LCD=OFF 关闭)
│   └── font_drv.h           # 字库 (仅 24px 启用, 其余 #if 0)
├── UART/
│   └── uart_protocol.h/.c   # DMA+IDLE 接收, 命令解析, 双模式输出
├── GlobalControl/
│   └── global_control.h/.c  # Global_Init, TIM2 10kHz 回调
└── SYSTEM/
    ├── sys.h/.c / system.h/.c   # 类型/寄存器宏/myabs/printf→USART2
    └── delay.h/.c               # SysTick 延时
```

---

## 四、模块详解

### 4.1 INA229 驱动

| 层 | 文件 | 关键实现 |
|----|------|---------|
| 物理 | soft_spi.c | HAL GPIO 位翻转, Mode 1, 内联展开(无函数调用), 零位间延时 |
| 驱动 | ina229.c | 寄存器读写, Current 直接用芯片内置 LSB 换算, Energy/Charge 按需读 |
| 应用 | INA229_Task.c | 动态速率状态机(20→30), 读写速率自动匹配 CT/AVG, DMA 发送 |

#### 关键优化
- SPI: 零延时 + 内联循环展开 → **~4 μs/byte**
- 读操作: 仅 Vbus + Current + Temp(=condition) → **8~11 字节/帧**
- 状态机: TIM2 10kHz, interval 缓存, 仅 CT/AVG/Mode 变化时重算
- 发送: DMA 非阻塞, 忙则跳帧

### 4.2 LCD 显示

- LCD: ST7789V 240×135, 横屏, 硬件 SPI1 缓冲(256B)
- 差量刷新: 值不变跳过整行, 稳时省 ~80% 带宽
- 自动单位: <1V→mV, <1A→mA, <1W→mW
- 刷新率: 10 Hz (TIM2 1000 ticks = 100 ms)
- 开关: `g_LcdTaskEnable` (串口 `LCD=OFF`)

### 4.3 串口协议

- DMA+IDLE 接收 (128B), 命令: CT/N, AVG/N, LCD=ON/OFF, MODE=TEXT/FLOAT, STATUS, HELP
- 双模式输出: TEXT (整数格式化, ~90B/帧), FLOAT (JUSTFLOAT, 24B/帧)
- 全局变量: `g_CtSetting`, `g_AvgSetting`, `g_OutputMode`, `g_LcdTaskEnable`
- FPS 统计: TEXT 模式每秒打印 `FPS=N`

### 4.4 系统

- TIM2: 10 kHz 时基 → `LcdTaskTim/INA229_TaskTim/number_count` 每 100 μs +1
- main: `UART_Process → Lcd_Task(if on) → INA229_Task`

---

## 五、数据流

```
TIM2 10kHz ──→ LcdTaskTim++ / INA229_TaskTim++ / number_count++

main while(1):
  ├─ UART_Protocol_Process()     DMA 空闲中断接收命令
  ├─ Lcd_Task() [if ON]          100 ms 差量刷新
  └─ INA229_Task()
       ├─ TEXT  → int snprintf → DMA 发送
       └─ FLOAT → 5×float 帧 → DMA 发送
```

---

## 六、关键参数

| 参数 | 值 | 说明 |
|------|-----|------|
| Rshunt | 20 mΩ | 分流电阻 |
| Imax | 10 A | SHUNT_CAL 计算基准 |
| Current LSB | 19.07 μA | Imax / 2^19 |
| ADC 模式 | CONT_ALL | 三通道连续 |
| 默认 CT | 6 (2074 μs) | |
| 默认 AVG | 2 (16次) | |
| INA229 周期 | ~100 ms | ≈ 10 Hz |
| TEXT 最高 | 200 Hz | 限速 50 ticks |
| FLOAT 最高 | 5 kHz | 限速 2 ticks |
| LCD 刷新 | 10 Hz | 100 ms |
| UART | 2 Mbps | DMA |
| TIM2 | 10 kHz | 100 μs |
| Flash 占用 | ~21 KB / 32 KB | 剩余 ~11 KB |

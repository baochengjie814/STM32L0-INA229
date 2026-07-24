# INA229_POWER_MMT 项目架构文档

> 最后更新: 2026-07-24
> MCU: STM32L030G6U6 (Cortex-M0+, 32MHz HSI-PLL)
> IDE: Keil MDK-ARM v5 (ARMCC V5.06) | CubeMX HAL
> 编码: 源码 UTF-8, 含 GBK 字符串的文件 (font_drv.h/lcd_mid.c) 保持 GBK

---

## 一、项目用途

基于 INA229 (TI 85V/20位 SPI 功率监控器) 的功率测量仪表。软件 SPI 采集 V/I/P/Temp, LCD (ST7789V 240×135) 实时显示, 串口 (USART2 2Mbps) 支持文本/JUSTFLOAT 双模式输出, 支持远程命令配置。

---

## 二、硬件外设

| 外设 | 用途 | 引脚 |
|------|------|------|
| SPI1 (硬件) | LCD 显示 | PA4(CS), PA6(RS/DC), PA5(SCK), PA7(MOSI) |
| 软SPI | INA229 通信 | PB0(SCK), PA9(MOSI), PB1(MISO), PA10(CS) |
| USART2 | 调试输出 + 命令接收 | PA2(TX), PA3(RX) |
| TIM2 | 1kHz 系统时基 | 内部 |
| GPIO | LCD 背光/复位 | PA0(LEDA), PA1(RES) |

---

## 三、目录结构

```
INA229_POWER_MMT/
├── Core/                        # CubeMX 生成 (一般不改)
│   ├── Inc/ (main.h gpio.h spi.h tim.h usart.h ...)
│   └── Src/ (main.c gpio.c spi.c tim.c usart.c ...)
├── Drivers/                     # HAL 库 + CMSIS (CMSIS-DSP 已从工程移除)
├── MDK-ARM/                     # Keil 工程、启动文件
├── USER/
│   ├── INA229/                  # INA229 功率监控驱动 + 任务
│   │   ├── ina229.h/.c          # 驱动层: 寄存器读写、测量、配置
│   │   ├── soft_spi.h/.c        # 驱动层: 软件 SPI (Mode 1)
│   │   └── INA229_Task.h/.c     # 应用层: 动态速率状态机读取
│   ├── LcdControl/              # LCD 显示 (ST7789V 240×135)
│   │   ├── lcd_drv.h/.c         # 驱动层: SPI 发送、画点线字图
│   │   ├── lcd_mid.h/.c         # 中间层: 页面逻辑 (差量刷新 + 自动单位)
│   │   ├── lcd_task.h/.c        # 应用层: 定时状态机刷新
│   │   └── font_drv.h           # ASCII 字库 (仅 24px 启用, 其余 #if 0)
│   ├── UART/                    # 串口通讯协议
│   │   └── uart_protocol.h/.c   # DMA+IDLE 接收, 命令解析
│   ├── GlobalControl/           # 全局初始化 + 系统时基
│   │   └── global_control.h/.c  # Global_Init, TIM2 回调
│   └── SYSTEM/
│       ├── sys.h/.c             # 寄存器宏、类型定义
│       ├── system.h/.c          # 总头文件、myabs、printf 重定向
│       └── delay.h/.c           # SysTick 微秒/毫秒延时
├── ARCHITECTURE.md              # 本文档
├── PROTOCOL.md                  # 串口协议文档
└── INA229_POWER_MMT.ioc         # CubeMX 项目
```

---

## 四、模块详解

### 4.1 INA229 驱动 (`USER/INA229/`)

**分层**: soft_spi (物理层) → ina229 (驱动层) → INA229_Task (应用层)

#### soft_spi.h/.c
- 软件 SPI, Mode 1 (CPOL=0, CPHA=1), ~166kHz
- API: `Soft_SPI_Init`, `CS_Low/High`, `Transfer`, `TransferBuf`, `DelayUs`

#### ina229.h/.c
- 完整寄存器映射 (14 个寄存器) + 位定义
- 读写: `ReadReg16/24/40`, `WriteReg16`
- 测量: `ReadVShunt/VBus/Temp/Current/Power/Energy/Charge/All`
- 配置: `SetMode/ConversionTime/Averaging/ShuntCal/ADCRange/TempComp`
- 报警阈值: 6 个阈值设置
- 初始化: `INA229_Init(rshunt, imax)` — 复位→ID校验→默认ADC→SHUNT_CAL
- 当前分流电阻: **20mΩ**, 最大电流: **10A**

#### INA229_Task.h/.c
- 动态速率状态机
- 状态 20: 等待采样间隔 (自动匹配 CT+AVG 配置)
- 状态 30: 读 Vsh/Vbus/I/P/Temp → 存入 `INA229_Data`
- 采样间隔 = CT_time × 3 × AVG_count / 1000 ms (最小 1ms)
- 通过串口改 CT/AVG 后速率自动跟随
- 输出: TEXT 模式 printf, FLOAT 模式 24 字节 JUSTFLOAT 帧

### 4.2 LCD 显示 (`USER/LcdControl/`)

**分层**: lcd_drv (物理层) → lcd_mid (中间层) → lcd_task (应用层)

#### lcd_drv.h/.c — ST7789V 240×135
- 硬件 SPI1, 缓冲发送 (256B)
- ST7789V 初始化序列 (电源/时钟/VCOM/Gamma/MADCTL)
- CASET/RASET: X+40, Y+52 偏移
- 绘图: Fill, DrawPoint/Line/Rectangle/Circle
- 显示: ShowChar/String/Printf/FloatNum1/IntNum/Picture
- ShowChinese 系列已 `#if 0` (字库禁用)
- USE_HORIZONTAL=3 (横屏), LCD_W=240 LCD_H=135

#### lcd_mid.h/.c
- Page1: VBUS/CURR/PWR/TEMP 四行, 24px 字体
- 差量刷新: 值不变则跳过整行, 稳定时省 ~80% SPI 带宽
- 自动单位: <1 时切换到 mV/mA/mW
- 固定宽度格式化 (%-8s + %-3s) 消除残留
- Logo 已 `#if 0` 禁用

#### lcd_task.h/.c
- 定时状态机: 状态 10 等 100ms → 状态 20 调用 LCD_Display_Page1
- 可通过 `g_LcdTaskEnable` 关闭 (串口 `LCD=OFF`)

#### font_drv.h
- ASCII 字库: ascii_1206/1608/3216 → `#if 0`, 仅 ascii_2412 启用
- 中文字库: tfont12/16/24/32 → `#if 0`
- Logo 数据: gImage_hs_logo → `#if 0`
- 文件编码: GBK (含中文注释)

### 4.3 串口协议 (`USER/UART/`)

#### uart_protocol.h/.c
- USART2, 2Mbps, DMA+IDLE 接收 (DMA1_Channel5)
- 命令: CT/N, AVG/N, LCD=ON/OFF, MODE=TEXT/FLOAT, STATUS, HELP
- 响应: OK / ERR
- 全局变量: `g_OutputMode`, `g_LcdTaskEnable`, `g_CtSetting`, `g_AvgSetting`

### 4.4 全局控制 (`USER/GlobalControl/`)

#### global_control.h/.c
- `Global_Init()`: Soft_SPI_Init → INA229_Init → LCD_Init → TIM2 启动 → UART_Protocol_Init
- `HAL_TIM_PeriodElapsedCallback`: 每 1ms 递增 LcdTaskTim, INA229_TaskTim, number_count

### 4.5 系统工具 (`USER/SYSTEM/`)

- sys.h/.c: 寄存器宏 (PAup/PAdown/PAin), 类型定义 (u8/u16/u32)
- system.h/.c: 总头文件, myabs, printf→USART2 重定向
- delay.h/.c: SysTick 微秒/毫秒阻塞延时

### 4.6 Core 层

- main.c: Global_Init → while(1){ UART_Protocol_Process; if(LCD) Lcd_Task; INA229_Task; }
- stm32l0xx_it.c: SysTick/TIM2/USART2/DMA1_Channel4_5_6_7 中断处理
- DMA 已从 CubeMX 移除, dma.h 引用已注释

---

## 五、数据流

```
TIM2 (1kHz) ──→ LcdTaskTim++ / INA229_TaskTim++ / number_count++

main() while(1):
  ├── UART_Protocol_Process()    # 处理串口命令
  ├── Lcd_Task() (if enabled)    # 100ms 刷新 LCD
  │     └── LCD_Display_Page1()  # 差量刷新 4 行
  │           └── SPI1 → LCD
  └── INA229_Task()              # 动态速率读取
        ├── TEXT mode  → printf → USART2
        └── FLOAT mode → 24B 帧 → USART2 (DMA)
        └── 软SPI → INA229
```

---

## 六、关键常量

| 常量 | 值 | 说明 |
|------|------|------|
| SystemCoreClock | 32 MHz | HSI×PLLMUL4÷PLLDIV2 |
| 分流电阻 Rshunt | 20 mΩ | INA229_Init 参数 |
| 最大电流 Imax | 10 A | SHUNT_CAL 计算用 |
| INA229 ADC 模式 | CONT_ALL | 三通道连续 |
| 默认 CT | 1052 μs | INA229_CT_1052US |
| 默认 AVG | 16 | INA229_AVG_16 |
| INA229 更新周期 | ~50 ms | ≈ 20 Hz |
| LCD 刷新 | 100 ms | Lcd_Task |
| USART2 波特率 | 2 Mbps | |
| LCD 分辨率 | 240×135 | ST7789V 横屏 |
| LCD 字体 | 24 px | |
| Flash 占用 | ~21 KB / 32 KB | 剩余 ~11 KB |

---

## 七、编码规则

| 文件类型 | 编码 | 原因 |
|----------|------|------|
| 纯代码文件 | UTF-8 | ARMCC V5 忽略注释编码 |
| font_drv.h | **GBK** | 含 GBK 字库字节数据 |
| lcd_mid.c | **GBK** | 含中文字符串字面量 (已改用英文, 但保留 GBK 兼容) |

ARMCC V5 按系统区域 (GBK/CP936) 解析源文件, 中文字符串字面量必须 GBK 编码。

---

## 八、Flash 优化历程

| 阶段 | Flash | 措施 |
|------|-------|------|
| 初始 | ~33 KB | 含全部字库+Logo+CMSIS-DSP |
| 去中文字库 | ~31 KB | tfont12/16/24/32 #if 0 |
| 去 Logo | ~23 KB | gImage_hs_logo #if 0 |
| 去 CMSIS-DSP | ~30 KB | 移除 arm_math, 链接器自动剔除 |
| 去旧 ASCII 字库 | **~21 KB** | ascii_1206/1608/3216 #if 0 |
| **当前** | **~21 KB** | **剩余 ~11 KB (35%)** |

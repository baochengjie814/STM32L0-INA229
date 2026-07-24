# INA229_POWER_MMT 项目架构文档

> 最后更新: 2026-07-23
> MCU: STM32L030G6U6 (Cortex-M0+, 32MHz HSI-PLL)
> IDE: Keil MDK-ARM v5 | 生成工具: STM32CubeMX (HAL)
> 编码: UTF-8

---

## 一、项目用途

基于 INA229（TI 85V/20位 SPI 功率监控器）的功率测量仪表。通过软件 SPI 周期性采集电压/电流/功率/电能数据，经 CMSIS-DSP 计算后显示在 LCD 屏幕上。

---

## 二、硬件外设总览

| 外设    | 用途                | 引脚                        |
|---------|---------------------|-----------------------------|
| SPI1    | LCD 显示 (硬件SPI)  | PA4(CS), PA6(RS/DC), PA7(MOSI), PA5(SCK) |
| 软SPI   | INA229 通信         | PB0(SCK), PA9(MOSI), PB1(MISO), PA10(CS) |
| USART2  | 调试串口 (printf)   | PA2(TX), PA3(RX)            |
| TIM2    | 1kHz 系统时基       | 内部定时器                  |
| SysTick | HAL 时基 / delay    | 系统内核                    |
| GPIO    | LCD 背光/复位       | PA0(LEDA), PA1(RES)         |

**注意**: INA229 使用软件模拟 SPI (SPI Mode 1, CPOL=0/CPHA=1), LCD 使用硬件 SPI1 (HAL)，两者引脚独立互不冲突。

---

## 三、目录结构

```
INA229_POWER_MMT/
├── Core/                        # CubeMX 生成 (一般不改动)
│   ├── Inc/
│   │   ├── main.h               # 引脚宏定义、HAL 外设声明
│   │   ├── gpio.h / spi.h / tim.h / usart.h
│   │   ├── stm32l0xx_hal_conf.h # HAL 模块裁剪
│   │   └── stm32l0xx_it.h       # 中断声明
│   └── Src/
│       ├── main.c               # 入口 (CubeMX 模板，待填用户代码)
│       ├── gpio.c               # GPIO 初始化 (MX_GPIO_Init)
│       ├── spi.c                # SPI1 初始化 (MX_SPI1_Init)
│       ├── tim.c                # TIM2 初始化 (MX_TIM2_Init)
│       ├── usart.c              # USART2 初始化 (MX_USART2_UART_Init)
│       ├── stm32l0xx_it.c       # 中断向量表 (TIM2/SysTick/NMI/HardFault)
│       └── system_stm32l0xx.c   # 系统时钟初始化
├── Drivers/                     # HAL 库 + CMSIS (不动)
├── MDK-ARM/                     # Keil 工程文件、启动文件、编译产物
├── USER/
│   ├── INA229/                  # INA229 功率监控驱动 + 任务
│   │   ├── ina229.h/.c          # 驱动层: 寄存器读写、数据处理、配置
│   │   ├── soft_spi.h/.c        # 驱动层: 软件模拟 SPI 总线
│   │   └── INA229_Task.h/.c     # 应用层: 定时状态机读取任务
│   ├── LcdControl/              # LCD 显示驱动 + 任务
│   │   ├── lcd_drv.h/.c         # 驱动层: SPI 发送、画点/线/字/图
│   │   ├── lcd_mid.h/.c         # 中间层: 页面逻辑 (Page1/Page2)
│   │   ├── lcd_task.h/.c        # 应用层: 定时状态机刷新任务
│   │   └── font_drv.h           # 字库数据 (汉字点阵)
│   ├── GlobalControl/           # 全局初始化 + 系统时基
│   │   └── global_control.h/.c  # Global_Init, TIM2 回调 (LcdTaskTim++等)
│   └── SYSTEM/
│       ├── sys.h/.c             # 寄存器操作宏、类型定义 (u8/u16/u32等)
│       ├── system.h/.c          # 总头文件、myabs、signed_sqrt、printf重定向
│       └── delay.h/.c           # SysTick 微秒/毫秒延时
└── INA229_POWER_MMT.ioc         # CubeMX 项目配置
```

---

## 四、模块详解

### 4.1 INA229 驱动 (`USER/INA229/`)

**分层**: soft_spi (物理层) → ina229 (驱动层) → INA229_Task (应用层)

#### soft_spi.h/.c
- 软件模拟 SPI 总线，不依赖 MCU 硬件 SPI 外设
- 支持 CPOL/CPHA 四种模式，当前配置 **Mode 1** (CPOL=0, CPHA=1)
- 速度: `SOFT_SPI_DELAY_US=2` → ~166kHz
- 对外 API: `Soft_SPI_Init()`, `Soft_SPI_CS_Low/High()`, `Soft_SPI_Transfer()`, `Soft_SPI_TransferBuf()`, `Soft_SPI_DelayUs()`
- 引脚定义在 `soft_spi.h` 用户配置区 (已对齐 main.h 的 INA 引脚)

#### ina229.h/.c
- 完整 INA229 寄存器映射和位定义 (14个寄存器: CONFIG~PWR_LIMIT + ID)
- 寄存器读写: `ReadReg16/24/40`, `WriteReg16`
- 24bit 有符号数据自动符号扩展, 40bit CHARGE 符号扩展
- 测量读取: `ReadVShunt()`, `ReadVBus()`, `ReadTemperature()`, `ReadCurrent()`, `ReadPower()`, `ReadEnergy()`, `ReadCharge()`, `ReadAll()`
- 配置 API: `SetADCRange()`, `SetMode()`, `SetConversionTime()`, `SetAveraging()`, `SetShuntCal()`, `EnableTempComp()`
- 报警阈值: 6 个阈值设置函数 (SOVL/SUVL/BOVL/BUVL/TEMP_LIMIT/PWR_LIMIT)
- 初始化: `INA229_Init(rshunt_ohm, i_max_a)` — 软复位→ID校验(0x2291)→默认ADC配置→SHUNT_CAL计算写入
- SHUNT_CAL 公式: `13107200000 × Rshunt(Ω) × Imax(A)`，上限 32767 (0x7FFF)

#### INA229_Task.h/.c
- 定时状态机，模仿 lcd_task 模式
- 全局变量: `g_INA229_Data` (测量数据), `INA229_TaskTim` (ms计数器)
- 状态机:
  ```
  状态20: 等待 200ms 采样间隔
  状态30: 轮询 INA229_IsConversionReady() → INA229_ReadAll() → 回状态20
  ```
- 初始化不在状态机内，由 Global_Init 外部调用

### 4.2 LCD 显示 (`USER/LcdControl/`)

**分层**: lcd_drv (物理层) → lcd_mid (中间层) → lcd_task (应用层)

#### lcd_drv.h/.c
- 硬件 SPI1 驱动 ST7735 类 LCD (160×80 横屏)
- 缓冲发送模式: 256B DMA 缓冲 → `LCD_Flush_buffer()` 批量 SPI 发送
- 单字节直发模式: `LCD_Send_Byte()` 用于命令
- 画点/线/矩形/圆、填充、字符/字符串/汉字(12/16/24/32px)、整数/浮点数显示
- 颜色: RGB565 宏定义 + 预定义 20+ 颜色常量
- `LCD_Init()` — 硬件初始化序列

#### lcd_mid.h/.c
- 页面管理: `LCD_Display_Page1()` (当前为空壳), `LCD_Display_Page2()` (注释掉)
- `LCD_Clear()` — 全屏填充黑色
- `LCD_Display_Logo()` — 启动时显示浩盛 Logo 1.5秒
- **当前状态**: 大部分页面显示逻辑已被注释，仅保留空壳函数

#### lcd_task.h/.c
- 定时状态机刷新
- 全局变量: `LcdTaskTim` (ms计数器)
- 状态机:
  ```
  状态10: 等待 100ms
  状态20: 调用 LCD_Display_Page1() → 回状态10
  ```

### 4.3 全局控制 (`USER/GlobalControl/`)

#### global_control.h/.c
- `Global_Init()` — 上电初始化入口，按顺序:
  1. `HAL_Delay(100)`
  2. `Soft_SPI_Init()` — 软件 SPI 引脚
  3. `INA229_Init(0.001f, 10.0f)` — INA229 (1mΩ/10A)
  4. `LCD_Init()` — LCD 初始化
  5. `HAL_TIM_Base_Start_IT(&htim2)` — 启动 1kHz 定时器
- `HAL_TIM_PeriodElapsedCallback()` — TIM2 每 1ms 调用一次:
  - `LcdTaskTim++` (LCD 刷新计时)
  - `INA229_TaskTim++` (INA229 采样计时)

### 4.4 系统工具 (`USER/SYSTEM/`)

#### sys.h/.c
- 寄存器直接操作宏: `PAup/PAdown/PAin` (通过 BSRR/BRR/IDR)
- 类型别名: `u8/u16/u32`, `s8/s16/s32` 等
- `.c` 文件为空 (仅注释掉的 ARM 汇编函数)

#### system.h/.c
- **总头文件**: 集中所有 include (HAL→用户模块→标准库→CMSIS-DSP)
- 数学宏: `SATURATE(IN,MIN,MAX)`, `PI`, `LOG`
- 函数: `myabs()` (32位绝对值), `signed_sqrt_f32()` (CMSIS-DSP 带符号开根号)
- `fputc()` 重定向 printf → USART2

#### delay.h/.c
- 基于 SysTick 的阻塞延时 (不依赖 HAL_Delay)
- `delay_init()` — 初始化 fac_us 系数
- `delay_us(us)` — 微秒延时
- `delay_ms(ms)` — 毫秒延时

### 4.5 Core 层 (`Core/`)

#### main.c
- 标准 CubeMX 模板，User Code 区全部为空
- 初始化顺序: `HAL_Init()` → `SystemClock_Config()` → `MX_GPIO_Init()` → `MX_USART2_UART_Init()` → `MX_SPI1_Init()` → `MX_TIM2_Init()`
- **主循环 `while(1)` 为空**，需填入任务调用 (INA229_Task, Lcd_Task 等)

#### stm32l0xx_it.c
- 中断服务: NMI_Handler, HardFault_Handler, SVC_Handler, PendSV_Handler, SysTick_Handler, TIM2_IRQHandler
- `SysTick_Handler`: 调用 `HAL_IncTick()`
- `TIM2_IRQHandler`: 调用 `HAL_TIM_IRQHandler(&htim2)` → 触发 `HAL_TIM_PeriodElapsedCallback`

#### main.h
- 引脚宏定义 (CubeMX 生成):
  - LCD: PA0(LEDA), PA1(RES), PA4(CS), PA6(RS)
  - INA: PB0(SCLK), PA9(MOSI), PB1(MISO), PA10(CS), PA8(ALE)

---

## 五、数据流

```
┌─────────────────────────────────────────────────────────┐
│                     TIM2 (1kHz)                         │
│                         │                               │
│            HAL_TIM_PeriodElapsedCallback()              │
│               │                    │                    │
│         LcdTaskTim++        INA229_TaskTim++           │
│               │                    │                    │
│    ┌──────────▼──────┐   ┌────────▼─────────┐          │
│    │   Lcd_Task()    │   │  INA229_Task()   │          │
│    │                  │   │                   │          │
│    │  状态10:等100ms  │   │  状态20:等200ms   │          │
│    │  状态20:刷Page1  │   │  状态30:读INA229  │          │
│    │       │          │   │       │           │          │
│    │  LCD_Display_    │   │  INA229_ReadAll() │          │
│    │  Page1()         │   │       │           │          │
│    │       │          │   │  g_INA229_Data   │          │
│    │  LCD_DrawXXX()   │   │  (V/A/W/TEMP)    │          │
│    │       │          │   │       │           │          │
│    │  SPI1 → LCD      │   │  软SPI → INA229  │          │
│    └──────────────────┘   └──────────────────┘          │
│                                                         │
│   未来: Lcd_Task 读取 g_INA229_Data 显示功率数据        │
└─────────────────────────────────────────────────────────┘
```

---

## 六、关键常量

| 常量 | 值 | 说明 |
|------|------|------|
| `SystemCoreClock` | 32 MHz | HSI(16MHz) × PLLMUL4 ÷ PLLDIV2 |
| TIM2 频率 | 1 kHz (1ms) | 系统时基 |
| INA229 采样间隔 | 200 ms | INA229_Task 状态20 |
| LCD 刷新间隔 | 100 ms | Lcd_Task 状态10 |
| INA229 ADC 模式 | 连续全通道 | MODE=0xF |
| INA229 转换时间 | 1052 μs/通道 | 精度与速度折中 |
| INA229 平均次数 | 16 | 降噪 |
| 分流电阻 Rshunt | 1 mΩ | INA229_Init 参数 |
| 最大预期电流 Imax | 10 A | SHUNT_CAL 计算用 |
| 软SPI 速度 | ~166 kHz | SOFT_SPI_DELAY_US=2 |

---

## 七、当前状态 & 待办

### 已完成
- [x] INA229 完整驱动 (寄存器读写、测量、配置、报警)
- [x] 软件 SPI 驱动 (引脚已对齐)
- [x] INA229 定时状态机任务
- [x] LCD 显示驱动 (完整绘图 API)
- [x] LCD 定时刷新任务
- [x] 全局初始化流程 (Global_Init)
- [x] 系统工具函数 (delay, myabs, signed_sqrt, printf)
- [x] 所有文件 GBK→UTF-8 编码转换

### 待完成
- [ ] `main.c` 主循环：调用 `INA229_Task()` + `Lcd_Task()` + 其他
- [ ] `main.c` User Code 区：调用 `Global_Init()` + `delay_init()`
- [ ] `LCD_Display_Page1()` — 填入实际的 INA229 数据显示逻辑
- [ ] `INA229_ReadCurrent/Power` — TODO: 实现安培/瓦特工程单位换算
- [ ] 可能需要清理 `lcd_mid.c` 中大量注释掉的 FOC 电机控制旧代码

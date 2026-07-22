/**
 * @file    soft_spi.c
 * @brief   软件模拟 SPI 总线驱动实现
 *
 * @author  (你的名字)
 * @date    (日期)
 *
 * @note    通过 HAL GPIO 函数控制引脚电平, 模拟 SPI 时序。
 *          支持 CPOL/CPHA 四种模式 (Mode 0/1/2/3)。
 *
 *   SPI 每位传输流程 (Mode 1, CPOL=0, CPHA=1):
 *     1. MOSI 输出数据位
 *     2. 延时 t_setup
 *     3. SCK 上升沿 (移位边沿, INA229 锁存 MOSI)
 *     4. 延时 t_hold
 *     5. 读取 MISO 电平
 *     6. SCK 下降沿 (采样边沿)
 *     7. 延时
 *
 *   注意: 不同 SPI 模式下的 EDGE_SHIFT / EDGE_SAMPLE 定义不同,
 *         但代码结构统一通过条件编译处理。
 */

#include "soft_spi.h"
#include "system.h"   /* SystemCoreClock */

/*---------------------------------------------------------------------------
 * 内部宏 (仅本文件可见)
 *
 *   将 HAL GPIO 操作封装为简洁宏, 提高代码可读性。
 *   注意: SCLK_H/SCLK_L 的电平含义取决于 CPOL 配置:
 *     CPOL=0: SCLK_H=SET (高有效), SCLK_L=RESET (低有效)
 *     CPOL=1: SCLK_H=RESET, SCLK_L=SET (极性反转)
 *---------------------------------------------------------------------------*/

/** @brief MOSI 输出高电平 */
#define MOSI_H() HAL_GPIO_WritePin(SOFT_SPI_MOSI_PORT, SOFT_SPI_MOSI_PIN, GPIO_PIN_SET)
/** @brief MOSI 输出低电平 */
#define MOSI_L() HAL_GPIO_WritePin(SOFT_SPI_MOSI_PORT, SOFT_SPI_MOSI_PIN, GPIO_PIN_RESET)
/** @brief 读取 MISO 引脚电平 (1=高, 0=低) */
#define MISO_R() HAL_GPIO_ReadPin(SOFT_SPI_MISO_PORT, SOFT_SPI_MISO_PIN)

/*
 * SCK 电平定义: 根据 CPOL 决定 "高" "低" 对应的 GPIO 操作
 *   CPOL=0: 空闲低 → SCLK_H=置高, SCLK_L=置低
 *   CPOL=1: 空闲高 → SCLK_H=置低, SCLK_L=置高 (反转)
 */
#if SOFT_SPI_CPOL == 0
  #define SCLK_H() HAL_GPIO_WritePin(SOFT_SPI_SCLK_PORT, SOFT_SPI_SCLK_PIN, GPIO_PIN_SET)
  #define SCLK_L() HAL_GPIO_WritePin(SOFT_SPI_SCLK_PORT, SOFT_SPI_SCLK_PIN, GPIO_PIN_RESET)
#else
  #define SCLK_H() HAL_GPIO_WritePin(SOFT_SPI_SCLK_PORT, SOFT_SPI_SCLK_PIN, GPIO_PIN_RESET)
  #define SCLK_L() HAL_GPIO_WritePin(SOFT_SPI_SCLK_PORT, SOFT_SPI_SCLK_PIN, GPIO_PIN_SET)
#endif

/*
 * 根据 CPOL/CPHA 组合定义移位边沿和采样边沿:
 *
 *   Mode 0 (CPOL=0, CPHA=0): 空闲低, 上升沿采样, 下降沿移位
 *     → 先采样(SCK↑), 后移位(SCK↓)
 *     → MISO 在上升沿被从设备更新, 主设备在上升沿后读取
 *
 *   Mode 1 (CPOL=0, CPHA=1): 空闲低, 下降沿采样, 上升沿移位
 *     → 先移位(SCK↑), 后采样(SCK↓)
 *     → MISO 在下降沿被从设备更新, 主设备在下降沿前读取
 *     → INA229 使用此模式!
 *
 *   Mode 2 (CPOL=1, CPHA=0): 空闲高, 下降沿采样, 上升沿移位
 *   Mode 3 (CPOL=1, CPHA=1): 空闲高, 上升沿采样, 下降沿移位
 */
#if   (SOFT_SPI_CPOL == 0 && SOFT_SPI_CPHA == 0)   /* Mode 0 */
  #define EDGE_SHIFT()   SCLK_L()    /**< 移位边沿 = 下降沿 */
  #define EDGE_SAMPLE()  SCLK_H()    /**< 采样边沿 = 上升沿 */
  #define READ_ON_RISING  1          /**< MISO 在上升沿后读取 */
#elif (SOFT_SPI_CPOL == 0 && SOFT_SPI_CPHA == 1)   /* Mode 1 ← INA229 */
  #define EDGE_SHIFT()   SCLK_H()
  #define EDGE_SAMPLE()  SCLK_L()
  #define READ_ON_RISING  0          /**< MISO 在下降沿后读取 */
#elif (SOFT_SPI_CPOL == 1 && SOFT_SPI_CPHA == 0)   /* Mode 2 */
  #define EDGE_SHIFT()   SCLK_H()
  #define EDGE_SAMPLE()  SCLK_L()
  #define READ_ON_RISING  0
#elif (SOFT_SPI_CPOL == 1 && SOFT_SPI_CPHA == 1)   /* Mode 3 */
  #define EDGE_SHIFT()   SCLK_L()
  #define EDGE_SAMPLE()  SCLK_H()
  #define READ_ON_RISING  1
#endif

/*---------------------------------------------------------------------------*/

/**
 * @brief  微秒级粗略延时 (基于 CPU 空指令循环)
 *
 *   计数值 = us × SystemCoreClock / 8000000
 *   其中 8000000 是经验系数, 约等于每微秒的 NOP 循环次数,
 *   需根据实际 MCU 频率微调。
 *
 *   @note  此延时精度较低, 仅用于 SPI 时序控制, 不适用于精确定时。
 *          如有硬件定时器, 建议替换为定时器延时。
 *
 * @param  us 延时微秒数
 */
void Soft_SPI_DelayUs(uint32_t us)
{
    /*
     * 8000000 为经验值, 使得 count 近似等于 us 微秒对应的 NOP 循环次数。
     * 例如 SystemCoreClock=72MHz → us=1 → count=9 → 约 1μs
     * 实际延时需用示波器校准。
     */
    uint32_t count = us * (SystemCoreClock / 8000000);
    while (count--) {
        __NOP();  /* 单周期空操作 */
    }
}

/**
 * @brief  软件 SPI 初始化
 *
 *   将所有 SPI 引脚置为初始状态:
 *     - SCK  = 空闲电平 (CPOL 决定)
 *     - MOSI = 低
 *     - CS   = 高 (不选中从设备)
 *
 *   @note  以下被注释掉的 HAL GPIO 初始化代码仅供参考,
 *          实际 GPIO 初始化请在 main.c 或其他初始化函数中完成,
 *          或取消注释以下代码并确保 __HAL_RCC_xxx_CLK_ENABLE 正确。
 */
void Soft_SPI_Init(void)
{
    /*
     * ====== 可选: HAL GPIO 初始化 (若不在此处初始化, 请在外部完成) ======
     *
     *   如果 GPIO 已在 CubeMX 生成的 MX_GPIO_Init() 中初始化,
     *   以下代码无需取消注释。
     *
     *   取消注释前请确认:
     *     - RCC 时钟已使能
     *     - 引脚模式正确 (SCK/MOSI/CS=推挽输出, MISO=浮空输入)
     *
     *   GPIO_InitTypeDef g = {0};
     *   __HAL_RCC_GPIOA_CLK_ENABLE();
     *   __HAL_RCC_GPIOB_CLK_ENABLE();
     *
     *   g.Pin   = SOFT_SPI_SCLK_PIN;
     *   g.Mode  = GPIO_MODE_OUTPUT_PP;
     *   g.Pull  = GPIO_NOPULL;
     *   g.Speed = GPIO_SPEED_FREQ_HIGH;
     *   HAL_GPIO_Init(SOFT_SPI_SCLK_PORT, &g);
     *
     *   g.Pin = SOFT_SPI_MOSI_PIN;
     *   HAL_GPIO_Init(SOFT_SPI_MOSI_PORT, &g);
     *
     *   g.Pin  = SOFT_SPI_MISO_PIN;
     *   g.Mode = GPIO_MODE_INPUT;
     *   HAL_GPIO_Init(SOFT_SPI_MISO_PORT, &g);
     *
     *   g.Pin  = SOFT_SPI_CS_PIN;
     *   g.Mode = GPIO_MODE_OUTPUT_PP;
     *   HAL_GPIO_Init(SOFT_SPI_CS_PORT, &g);
     */

    /* 设置初始电平: SCK 空闲, MOSI 低, CS 高 (不选中) */
    SCLK_L();           /* 时钟置空闲电平 */
    MOSI_L();           /* MOSI 置低     */
    Soft_SPI_CS_High(); /* CS 拉高 (不选中) */
    Soft_SPI_DelayUs(10); /* 稳定延时 */
}

/**
 * @brief  拉低片选信号 (CS=0), 开始 SPI 通信
 * @note   拉低后延时 1μs 以满足从设备的 CS 建立时间 (t_CSSC)
 */
void Soft_SPI_CS_Low(void)
{
    HAL_GPIO_WritePin(SOFT_SPI_CS_PORT, SOFT_SPI_CS_PIN, GPIO_PIN_RESET);
    Soft_SPI_DelayUs(1);  /* CS 建立时间 */
}

/**
 * @brief  拉高片选信号 (CS=1), 结束 SPI 通信
 * @note   拉高前延时 1μs 以满足从设备的数据保持时间 (t_CSH),
 *         拉高后延时 1μs 以满足 CS 高电平最小宽度。
 */
void Soft_SPI_CS_High(void)
{
    Soft_SPI_DelayUs(1);  /* CS 保持时间 (最后一位数据稳定) */
    HAL_GPIO_WritePin(SOFT_SPI_CS_PORT, SOFT_SPI_CS_PIN, GPIO_PIN_SET);
    Soft_SPI_DelayUs(1);  /* CS 高电平最小宽度 */
}

/**
 * @brief  单字节 SPI 全双工传输 (MSB first)
 *
 *   每字节 8 个时钟周期, 每个周期:
 *     1. MOSI 输出当前位 (从 MSB 开始)
 *     2. 根据 READ_ON_RISING 决定采样/移位顺序:
 *        上升沿读取: 先采样 → 读 MISO → 后移位
 *        下降沿读取: 先移位 → 读 MISO → 后采样
 *     3. 循环直到 8 位传输完成
 *
 * @param  tx 要发送的 8-bit 数据
 * @return 同时接收到的 8-bit 数据
 */
uint8_t Soft_SPI_Transfer(uint8_t tx)
{
    uint8_t rx = 0;   /* 接收数据缓存 */
    uint8_t i;        /* 位计数器     */

    for (i = 0; i < 8; i++)
    {
        /* 1. 输出当前最高位到 MOSI */
        if (tx & 0x80) MOSI_H(); else MOSI_L();
        tx <<= 1;  /* 左移, 准备下一位 */

#if READ_ON_RISING
        /* 上升沿采样模式 (Mode 0 / Mode 3):
         *   SCK 上升沿时 MISO 数据已稳定 → 先采样再移位 */
        Soft_SPI_DelayUs(SOFT_SPI_DELAY_US);
        EDGE_SAMPLE();                /* 产生采样边沿    */
        rx <<= 1;                     /* 接收寄存器左移  */
        if (MISO_R()) rx |= 0x01;    /* 读取 MISO 电平  */
        Soft_SPI_DelayUs(SOFT_SPI_DELAY_US);
        EDGE_SHIFT();                 /* 产生移位边沿    */
#else
        /* 下降沿采样模式 (Mode 1 / Mode 2) ← INA229:
         *   SCK 下降沿时 MISO 数据已稳定 → 先移位再采样 */
        EDGE_SHIFT();                 /* 产生移位边沿 (上升沿, 从设备锁存 MOSI) */
        Soft_SPI_DelayUs(SOFT_SPI_DELAY_US);
        rx <<= 1;
        if (MISO_R()) rx |= 0x01;    /* 读取 MISO 电平 */
        EDGE_SAMPLE();                /* 产生采样边沿 (下降沿) */
#endif
        Soft_SPI_DelayUs(SOFT_SPI_DELAY_US);
    }
    return rx;
}

/**
 * @brief  多字节 SPI 批量传输
 *
 *   依次调用 Soft_SPI_Transfer() 传输 len 字节。
 *   tx 为 NULL 时全部发送 0x00 (仅接收模式),
 *   rx 为 NULL 时丢弃接收数据 (仅发送模式)。
 *
 * @param  tx  发送缓冲区 (可为 NULL)
 * @param  rx  接收缓冲区 (可为 NULL)
 * @param  len 传输字节数
 */
void Soft_SPI_TransferBuf(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        /*
         * 发送: tx 非空 → tx[i], 否则 → 0x00 (填充)
         * 接收: rx 非空 → 存入 rx[i], 否则 → 丢弃
         */
        uint8_t r = Soft_SPI_Transfer(tx ? tx[i] : 0x00);
        if (rx) rx[i] = r;
    }
}

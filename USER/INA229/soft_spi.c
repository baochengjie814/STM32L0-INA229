/**
  ******************************************************************************
  * 文件名程: soft_spi.c
  * 作    者:
  * 版    本: V1.0
  * 编写日期:
  * 功    能: 软件模拟 SPI 总线驱动实现 (HAL GPIO 位翻转)
  *
  * 每位传输流程 (Mode 1, CPOL=0, CPHA=1, INA229 模式):
  *   1. MOSI 输出当前位
  *   2. SCK 上升沿 (移位边沿, INA229 锁存 MOSI)
  *   3. 读 MISO 电平
  *   4. SCK 下降沿 (采样边沿)
  ******************************************************************************
  */

/* 头文件 ---------------------------------------------------------------*/
#include "soft_spi.h"
#include "system.h"

/*===========================================================================
 * 内部宏: GPIO 操作封装
 *===========================================================================*/

#define MOSI_H()  HAL_GPIO_WritePin(SOFT_SPI_MOSI_PORT, SOFT_SPI_MOSI_PIN, GPIO_PIN_SET)
#define MOSI_L()  HAL_GPIO_WritePin(SOFT_SPI_MOSI_PORT, SOFT_SPI_MOSI_PIN, GPIO_PIN_RESET)
#define MISO_R()  HAL_GPIO_ReadPin(SOFT_SPI_MISO_PORT, SOFT_SPI_MISO_PIN)

#if SOFT_SPI_CPOL == 0
  #define SCLK_H()  HAL_GPIO_WritePin(SOFT_SPI_SCLK_PORT, SOFT_SPI_SCLK_PIN, GPIO_PIN_SET)
  #define SCLK_L()  HAL_GPIO_WritePin(SOFT_SPI_SCLK_PORT, SOFT_SPI_SCLK_PIN, GPIO_PIN_RESET)
#else
  #define SCLK_H()  HAL_GPIO_WritePin(SOFT_SPI_SCLK_PORT, SOFT_SPI_SCLK_PIN, GPIO_PIN_RESET)
  #define SCLK_L()  HAL_GPIO_WritePin(SOFT_SPI_SCLK_PORT, SOFT_SPI_SCLK_PIN, GPIO_PIN_SET)
#endif

/* 根据 CPOL/CPHA 定义移位边沿和采样边沿 */
#if   (SOFT_SPI_CPOL == 0 && SOFT_SPI_CPHA == 0)   /* Mode 0: 上升沿采样, 下降沿移位 */
  #define EDGE_SHIFT()   SCLK_L()
  #define EDGE_SAMPLE()  SCLK_H()
  #define READ_ON_RISING  1
#elif (SOFT_SPI_CPOL == 0 && SOFT_SPI_CPHA == 1)   /* Mode 1: 上升沿移位, 下降沿采样 (INA229) */
  #define EDGE_SHIFT()   SCLK_H()
  #define EDGE_SAMPLE()  SCLK_L()
  #define READ_ON_RISING  0
#elif (SOFT_SPI_CPOL == 1 && SOFT_SPI_CPHA == 0)   /* Mode 2: 下降沿采样, 上升沿移位 */
  #define EDGE_SHIFT()   SCLK_H()
  #define EDGE_SAMPLE()  SCLK_L()
  #define READ_ON_RISING  0
#elif (SOFT_SPI_CPOL == 1 && SOFT_SPI_CPHA == 1)   /* Mode 3: 下降沿移位, 上升沿采样 */
  #define EDGE_SHIFT()   SCLK_L()
  #define EDGE_SAMPLE()  SCLK_H()
  #define READ_ON_RISING  1
#endif

/*===========================================================================
 * 延时
 *===========================================================================*/

/**
 * 微秒级粗略延时 (NOP 空循环)
 * 公式: count = us × SystemCoreClock / 8000000
 * 8000000 为经验系数, 实际精度需示波器校准
 */
void Soft_SPI_DelayUs(uint32_t us)
{
    uint32_t count = us * (SystemCoreClock / 8000000);
    while (count--) {
        __NOP();
    }
}

/*===========================================================================
 * 初始化
 *===========================================================================*/

/**
 * 将所有 SPI 引脚置为初始状态:
 *   SCK  = 空闲电平 (CPOL 决定)
 *   MOSI = 低
 *   CS   = 高 (不选中)
 *
 * 注意: GPIO 初始化已在 CubeMX 的 MX_GPIO_Init() 中完成,
 *       此处仅设置电平, 不做 GPIO 模式配置
 */
void Soft_SPI_Init(void)
{
    SCLK_L();
    MOSI_L();
    Soft_SPI_CS_High();
}

/*===========================================================================
 * 片选控制
 *===========================================================================*/

void Soft_SPI_CS_Low(void)
{
    HAL_GPIO_WritePin(SOFT_SPI_CS_PORT, SOFT_SPI_CS_PIN, GPIO_PIN_RESET);
}

void Soft_SPI_CS_High(void)
{
    HAL_GPIO_WritePin(SOFT_SPI_CS_PORT, SOFT_SPI_CS_PIN, GPIO_PIN_SET);
}

/*===========================================================================
 * SPI 传输
 *===========================================================================*/

/* 单次位传输宏 (Mode 1: SCK↑移位 → 读MISO → SCK↓) */
#define SPI_BIT(tx_bit, rx_var)  do {           \
    if (tx_bit) MOSI_H(); else MOSI_L();         \
    SCLK_H();                                    \
    rx_var <<= 1;                                \
    if (MISO_R()) rx_var |= 0x01;                \
    SCLK_L();                                    \
} while(0)

/* 单次位传输宏 (Mode 1: SCK↑移位 → 读MISO → SCK↓) */
#define SPI_BIT(tx_bit, rx_var)  do {           \
    if (tx_bit) MOSI_H(); else MOSI_L();         \
    SCLK_H();                                    \
    rx_var <<= 1;                                \
    if (MISO_R()) rx_var |= 0x01;                \
    SCLK_L();                                    \
} while(0)

/**
 * 多字节 SPI 批量传输 (内联展开, 无反调)
 */
void Soft_SPI_TransferBuf(const uint8_t *tx, uint8_t *rx, uint16_t len)
{
    uint16_t i;
    for (i = 0; i < len; i++) {
        uint8_t dat = tx ? tx[i] : 0x00;
        uint8_t r   = 0;
        SPI_BIT(dat & 0x80, r); dat <<= 1;
        SPI_BIT(dat & 0x80, r); dat <<= 1;
        SPI_BIT(dat & 0x80, r); dat <<= 1;
        SPI_BIT(dat & 0x80, r); dat <<= 1;
        SPI_BIT(dat & 0x80, r); dat <<= 1;
        SPI_BIT(dat & 0x80, r); dat <<= 1;
        SPI_BIT(dat & 0x80, r); dat <<= 1;
        SPI_BIT(dat & 0x80, r);
        if (rx) rx[i] = r;
    }
}

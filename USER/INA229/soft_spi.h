/**
  ******************************************************************************
  * 文件名程: soft_spi.h
  * 作    者:
  * 版    本: V1.0
  * 编写日期:
  * 功    能: 软件模拟 SPI 总线驱动 — 通过 GPIO 翻转模拟 SPI 时序
  *
  * 支持 CPOL/CPHA 四种模式 (Mode 0/1/2/3), MSB first.
  * INA229 要求: SPI Mode 1 (CPOL=0, CPHA=1)
  *
  * 模式对照:
  *   Mode 0: CPOL=0, CPHA=0 → 空闲SCK=低, 上升沿采样
  *   Mode 1: CPOL=0, CPHA=1 → 空闲SCK=低, 下降沿采样  ← INA229
  *   Mode 2: CPOL=1, CPHA=0 → 空闲SCK=高, 下降沿采样
  *   Mode 3: CPOL=1, CPHA=1 → 空闲SCK=高, 上升沿采样
  *
  * 速度: SOFT_SPI_DELAY_US=2 → 约 166kHz
  *       INA229 最高支持 ~10MHz, 但软件 SPI 受 MCU 主频限制
  ******************************************************************************
  */

#ifndef __SOFT_SPI_H__
#define __SOFT_SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 头文件 ---------------------------------------------------------------*/
#include "main.h"
#include <stdint.h>

/*===========================================================================
 * 用户配置: 引脚定义 (与 main.h 中 INA 引脚对齐)
 *===========================================================================*/

#define SOFT_SPI_SCLK_PORT          GPIOB
#define SOFT_SPI_SCLK_PIN           GPIO_PIN_0       /* PB0 = INA_SCLK */

#define SOFT_SPI_MOSI_PORT          GPIOA
#define SOFT_SPI_MOSI_PIN           GPIO_PIN_9       /* PA9 = INA_MOSI */

#define SOFT_SPI_MISO_PORT          GPIOB
#define SOFT_SPI_MISO_PIN           GPIO_PIN_1       /* PB1 = INA_MISO */

#define SOFT_SPI_CS_PORT            GPIOA
#define SOFT_SPI_CS_PIN             GPIO_PIN_10      /* PA10 = INA_CS */

/*===========================================================================
 * 用户配置: SPI 模式与速度
 *===========================================================================*/

#define SOFT_SPI_CPOL               0    /* 时钟极性: 0=空闲低 */
#define SOFT_SPI_CPHA               1    /* 时钟相位: 1=第二个边沿采样 */
#define SOFT_SPI_DELAY_US           0    /* 位间延时 0μs (测试极限) */

/*===========================================================================
 * API
 *===========================================================================*/

void     Soft_SPI_DelayUs(uint32_t us);
void     Soft_SPI_Init(void);
void     Soft_SPI_CS_Low(void);
void     Soft_SPI_CS_High(void);
uint8_t  Soft_SPI_Transfer(uint8_t tx);
void     Soft_SPI_TransferBuf(const uint8_t *tx, uint8_t *rx, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __SOFT_SPI_H__ */

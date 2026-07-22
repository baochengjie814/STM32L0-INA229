/**
 * @file    soft_spi.h
 * @brief   软件模拟 SPI 总线驱动 (头文件)
 *
 * @author  (你的名字)
 * @date    (日期)
 *
 * @note    通过 GPIO 位翻转模拟 SPI 时序, 不依赖 MCU 硬件 SPI 外设。
 *          支持 CPOL/CPHA 四种模式 (Mode 0/1/2/3)。
 *
 *   使用前请在下方 "用户配置区" 修改引脚和模式定义,
 *   以匹配你的硬件连接和 INA229 的时序要求。
 *
 *   INA229 要求: SPI Mode 1 (CPOL=0, CPHA=1), MSB first。
 *
 *   SPI 模式总结:
 *     Mode 0: CPOL=0, CPHA=0  → 空闲 SCK=低, 上升沿采样
 *     Mode 1: CPOL=0, CPHA=1  → 空闲 SCK=低, 下降沿采样  ← INA229
 *     Mode 2: CPOL=1, CPHA=0  → 空闲 SCK=高, 下降沿采样
 *     Mode 3: CPOL=1, CPHA=1  → 空闲 SCK=高, 上升沿采样
 *
 *   速度说明:
 *     SOFT_SPI_DELAY_US 控制每位之间的延迟 (μs),
 *     2μs → ~250kHz, 1μs → ~500kHz。
 *     INA229 最高支持 ~10MHz, 但软件 SPI 受 MCU 速度限制。
 */

#ifndef __SOFT_SPI_H__
#define __SOFT_SPI_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/*===========================================================================
 * 用户配置区 (根据实际硬件连接修改以下宏)
 *===========================================================================*/

/** @brief SCK 时钟引脚 - GPIO 端口 */
#define SOFT_SPI_SCLK_PORT          GPIOA
/** @brief SCK 时钟引脚 - GPIO 引脚号 */
#define SOFT_SPI_SCLK_PIN           GPIO_PIN_5

/** @brief MOSI 主出从入引脚 - GPIO 端口 */
#define SOFT_SPI_MOSI_PORT          GPIOA
/** @brief MOSI 主出从入引脚 - GPIO 引脚号 */
#define SOFT_SPI_MOSI_PIN           GPIO_PIN_7

/** @brief MISO 主入从出引脚 - GPIO 端口 */
#define SOFT_SPI_MISO_PORT          GPIOA
/** @brief MISO 主入从出引脚 - GPIO 引脚号 */
#define SOFT_SPI_MISO_PIN           GPIO_PIN_6

/** @brief CS 片选引脚 - GPIO 端口 */
#define SOFT_SPI_CS_PORT            GPIOB
/** @brief CS 片选引脚 - GPIO 引脚号 */
#define SOFT_SPI_CS_PIN             GPIO_PIN_0

/**
 * @brief SPI 时钟极性 (CPOL)
 *   0 = 空闲时 SCK 为低电平
 *   1 = 空闲时 SCK 为高电平
 */
#define SOFT_SPI_CPOL               0

/**
 * @brief SPI 时钟相位 (CPHA)
 *   0 = 第一个时钟边沿采样
 *   1 = 第二个时钟边沿采样
 */
#define SOFT_SPI_CPHA               1

/**
 * @brief 每位之间的延迟 (μs)
 *   控制 SPI 时钟频率: f_SCK ≈ 1 / (DELAY_US × 3) Hz
 *   例如 DELAY_US=2 → f_SCK ≈ 166kHz
 */
#define SOFT_SPI_DELAY_US           2

/*===========================================================================
 * 公共函数声明
 *===========================================================================*/

/**
 * @brief  软件 SPI 微秒级延时
 * @param  us 延时时间 (μs)
 * @note   使用 SystemCoreClock 进行粗略延时, 不依赖定时器
 */
void     Soft_SPI_DelayUs(uint32_t us);

/**
 * @brief  软件 SPI 初始化
 * @note   将所有引脚设为初始电平:
 *         SCK=低, MOSI=低, CS=高 (空闲)
 *         如需 HAL 库 GPIO 初始化, 请在外部完成或取消注释内部代码
 */
void     Soft_SPI_Init(void);

/**
 * @brief  拉低片选 (CS=0), 开始 SPI 传输
 * @note   拉低前后各有 1μs 延时, 确保满足 INA229 的 CS 建立/保持时间
 */
void     Soft_SPI_CS_Low(void);

/**
 * @brief  拉高片选 (CS=1), 结束 SPI 传输
 * @note   拉高前后各有 1μs 延时
 */
void     Soft_SPI_CS_High(void);

/**
 * @brief  单字节 SPI 传输 (同时收发)
 * @param  tx 发送的 8-bit 数据 (MSB first)
 * @return 接收到的 8-bit 数据
 * @note   发送和接收同时进行, MSB first
 */
uint8_t  Soft_SPI_Transfer(uint8_t tx);

/**
 * @brief  多字节 SPI 传输 (可选收发)
 * @param  tx  发送缓冲区指针 (可为 NULL, 则全部发送 0x00)
 * @param  rx  接收缓冲区指针 (可为 NULL, 则丢弃接收数据)
 * @param  len 传输字节数
 * @note   发送和接收同时进行, 每字节 MSB first
 */
void     Soft_SPI_TransferBuf(const uint8_t *tx, uint8_t *rx, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* __SOFT_SPI_H__ */

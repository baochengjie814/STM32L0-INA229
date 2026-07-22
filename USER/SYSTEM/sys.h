#ifndef __SYS_H
#define __SYS_H	 

#include "stm32l0xx_hal.h"   // 修改：G4 的 HAL 头文件
#include "stdio.h"

// PA 端口
#define PAup(n)   (GPIOA->BSRR = (1U << (n)))     // 置高
#define PAdown(n) (GPIOA->BRR  = (1U << (n)))     // 置低
#define PAin(n)   (((GPIOA->IDR) >> (n)) & 1U)    // 读取电平

// PB 端口
#define PBup(n)   (GPIOB->BSRR = (1U << (n)))
#define PBdown(n) (GPIOB->BRR  = (1U << (n)))
#define PBin(n)   (((GPIOB->IDR) >> (n)) & 1U)

// PC 端口
#define PCup(n)   (GPIOC->BSRR = (1U << (n)))
#define PCdown(n) (GPIOC->BRR  = (1U << (n)))
#define PCin(n)   (((GPIOC->IDR) >> (n)) & 1U)

//// PD 端口
//#define PDup(n)   (GPIOD->BSRR = (1U << (n)))
//#define PDdown(n) (GPIOD->BRR  = (1U << (n)))
//#define PDin(n)   (((GPIOD->IDR) >> (n)) & 1U)

//// PE 端口
//#define PEup(n)   (GPIOE->BSRR = (1U << (n)))
//#define PEdown(n) (GPIOE->BRR  = (1U << (n)))
//#define PEin(n)   (((GPIOE->IDR) >> (n)) & 1U)
////// 系统是否支持 OS（沿用原定义）
////#define SYSTEM_SUPPORT_OS		0		

// 数据类型定义（保持不变）
typedef int32_t  s32;
typedef int16_t s16;
typedef int8_t  s8;
typedef const int32_t sc32;
typedef const int16_t sc16;
typedef const int8_t sc8;
typedef __IO int32_t  vs32;
typedef __IO int16_t  vs16;
typedef __IO int8_t   vs8;
typedef __I int32_t vsc32;
typedef __I int16_t vsc16;
typedef __I int8_t vsc8;
typedef uint32_t  u32;
typedef uint16_t u16;
typedef uint8_t  u8;
typedef const uint32_t uc32;
typedef const uint16_t uc16;
typedef const uint8_t uc8;
typedef __IO uint32_t  vu32;
typedef __IO uint16_t vu16;
typedef __IO uint8_t  vu8;
typedef __I uint32_t vuc32;
typedef __I uint16_t vuc16;
typedef __I uint8_t vuc8;




#endif

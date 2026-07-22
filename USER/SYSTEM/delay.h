#ifndef __DELAY_H
#define __DELAY_H

#include <stdint.h>      // 使用 uint32_t 等类型
// #include "sys.h"       // 如果 sys.h 中有冲突定义，可以注释掉或保留

// 注意：改为 delay_init()，不再使用 delay_us_init()
// 因为 SysTick 方案不需要返回 HAL_StatusTypeDef
void delay_init(void);
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);   // 新增毫秒延时，方便使用

#endif

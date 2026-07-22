#ifndef __SYSTEM_H
#define __SYSTEM_H

/*===========================================================================
 * 系统宏定义
 *===========================================================================*/

#define SATURATE(_IN, _MIN, _MAX)       \
    {                                   \
        if ((_IN) <= (_MIN))            \
            (_IN) = (_MIN);             \
        else if ((_IN) >= (_MAX))       \
            (_IN) = (_MAX);             \
    }

#define PI  3.14159265358979323846f
#define LOG 1

/*===========================================================================
 * HAL 外设层 (CubeMX 生成)
 *===========================================================================*/

#include "main.h"
#include "gpio.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"

/*===========================================================================
 * 用户驱动模块
 *===========================================================================*/

/* INA229 功率监控 */
#include "ina229.h"
#include "soft_spi.h"
#include "INA229_Task.h"

/* LCD 显示 */
#include "lcd_drv.h"
#include "lcd_mid.h"
#include "lcd_task.h"

/* 全局控制 */
#include "global_control.h"

/* 系统基础 */
#include "sys.h"
#include "delay.h"

/*===========================================================================
 * 标准库
 *===========================================================================*/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <math.h>

#include "arm_math.h"

/*===========================================================================
 * 函数声明
 *===========================================================================*/

u32         myabs(long int a);
int         fputc(int ch, FILE *f);
arm_status  signed_sqrt_f32(float32_t x, float32_t *result);

#endif /* __SYSTEM_H__ */

#ifndef __INA229_TASK_H
#define __INA229_TASK_H

#include "main.h"
#include "sys.h"
#include "ina229.h"

/* 分流电阻 (Ω) — 与实际硬件一致 */
#define INA229_RSHUNT_OHM    0.02f

/* 串口打印开关: 1=启用, 0=关闭 */
#define INA229_PRINTF_ENABLE  1

/* 全局测量数据 (含实际工程单位, 供 LCD 等模块读取) */
extern volatile INA229_Data_t INA229_Data;
extern volatile u16            INA229_TaskTim;

void INA229_Task(void);

#endif

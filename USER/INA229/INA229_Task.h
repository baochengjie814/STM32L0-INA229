#ifndef __INA229_TASK_H
#define __INA229_TASK_H

#include "main.h"
#include "sys.h"
#include "ina229.h"

/* 全局测量数据 (供其他模块如 LCD 显示使用) */
extern INA229_Data_t g_INA229_Data;
extern volatile u16    INA229_TaskTim;

void INA229_Task(void);

#endif

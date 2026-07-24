/**
  ******************************************************************************
  * 文件名程: INA229_Task.c
  * 作    者:
  * 版    本: V1.0
  * 编写日期: 2026-07-23
  * 功    能: INA229 定时状态机读取任务
  *           状态 20: 等待 200ms 采样间隔
  *           状态 30: 读取原始数据 → 换算工程单位 → 存入全局结构体
  ******************************************************************************
  */
/* 头文件 ---------------------------------------------------------------*/
#include "INA229_Task.h"
#include "ina229.h"
#include "system.h"
#include <stdio.h>

/* 全局变量 ---------------------------------------------------------------*/
volatile INA229_Data_t INA229_Data;          /* 最新测量数据, 供外部模块读取  */
volatile u16            INA229_TaskId  = 20; /* 当前任务状态                    */
volatile u16            INA229_TaskTim = 0;  /* 状态计时器 (ms), 由 TIM2 递增  */

/**
  * 函数功能: INA229 定时状态机任务
  * 输入参数: 无
  * 返 回 值: 无
  * 说    明: 在主循环中周期性调用
  *           状态 20:  等待 200ms 采样间隔
  *           状态 30:  轮询转换完成 → 读原始数据 → 计算工程单位 → 存全局变量
  */
void INA229_Task(void)
{
    switch (INA229_TaskId)
    {
        /*==============================================================
         * 状态 20: 等待采样间隔 (100ms)
         *==============================================================*/
        case 20:
        {
            if (INA229_TaskTim >= 100)
            {
                INA229_TaskTim = 0;
                INA229_TaskId = 30;
            }
        }
        break;

        /*==============================================================
         * 状态 30: 读取并计算 (连续模式, 直接读寄存器即可)
         *==============================================================*/
        case 30:
        {
            /* 读取原始测量值 */
            float vshunt = INA229_ReadVShunt(0);      /* V */
            float vbus   = INA229_ReadVBus();          /* V */
            float temp   = INA229_ReadTemperature();   /* °C */

            /* 计算电流: I = Vshunt / Rshunt */
            float current = vshunt / INA229_RSHUNT_OHM;

            /* 计算功率: P = Vbus × I */
            float power = vbus * current;

            /* 存入全局结构体 */
            INA229_Data.vshunt      = -vshunt;
            INA229_Data.vbus        = vbus;
            INA229_Data.temperature = temp;
            INA229_Data.current     = -current;
            INA229_Data.power       = -power;
            INA229_Data.energy      = INA229_ReadEnergy();
            INA229_Data.charge      = INA229_ReadCharge();

#if INA229_PRINTF_ENABLE
            printf("[INA229] Vsh=%.3fuV  Vbus=%.3fmV  I=%.3fmA  P=%.3fmW  T=%.3fdC\r\n",
                   vshunt * 1e6f, vbus * 1e3f,
                   current * 1e3f, power * 1e3f,
                   temp * 10.0f);
#endif

            INA229_TaskId = 20;
        }
        break;

        default:
            break;
    }
}

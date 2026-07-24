/**
  ******************************************************************************
  * 文件名程: INA229_Task.c
  * 作    者:
  * 版    本: V1.0
  * 编写日期: 2026-07-24
  * 功    能: INA229 动态速率读取任务
  *           状态 20: 等待采样间隔 (自动匹配 CT+AVG 配置)
  *           状态 30: 读取数据 → 存入全局变量
  ******************************************************************************
  */
/* 头文件 ---------------------------------------------------------------*/
#include "INA229_Task.h"
#include "ina229.h"
#include "system.h"
#include "uart_protocol.h"
#include <stdio.h>

/*===========================================================================
 * 转换时间 / 平均次数 查找表
 *===========================================================================*/

static const uint16_t _ct_us[]   = {50, 84, 150, 280, 540, 1052, 2074, 4120};
static const uint16_t _avg_cnt[] = {1, 4, 16, 64, 128, 256, 512, 1024};

/**
 * 计算当前配置下的完整转换周期 (ms)
 *   总时间 = CT_us × 3 通道 × AVG 次数 ÷ 1000, 最小 1ms
 */
static u16 _CalcCycleMs(void)
{
    uint32_t us = (uint32_t)_ct_us[g_CtSetting] * 3 * _avg_cnt[g_AvgSetting];
    u16 ms = (u16)((us + 999) / 1000);   /* 向上取整 */
    return ms > 0 ? ms : 1;
}

/*===========================================================================
 * 全局变量
 *===========================================================================*/

volatile INA229_Data_t INA229_Data;          /* 最新测量数据                   */
volatile u16            INA229_TaskId  = 20; /* 当前任务状态                   */
volatile u16            INA229_TaskTim = 0;  /* ms 计数器, TIM2 回调递增         */

/**
  * 函数功能: INA229 动态速率读取
  * 说    明: 采样间隔自动根据 CT/AVG 设定变化
  *           CT_1052US + AVG_16 → 51ms → ≈20Hz
  *           CT_50US   + AVG_1  →  1ms → 1kHz (最大)
  */
void INA229_Task(void)
{
    static u16 _cycle_ms = 51;              /* 缓存上次计算值, 默认 51ms */

    switch (INA229_TaskId)
    {
        /*==============================================================
         * 状态 20: 等待动态采样间隔
         *==============================================================*/
        case 20:
        {
            _cycle_ms = _CalcCycleMs();

            if (INA229_TaskTim >= _cycle_ms) {
                INA229_TaskTim = 0;
                INA229_TaskId = 30;
            }
        }
        break;

        /*==============================================================
         * 状态 30: 读取数据
         *==============================================================*/
        case 30:
        {
            float vshunt  = INA229_ReadVShunt(0);
            float vbus    = INA229_ReadVBus();
            float temp    = INA229_ReadTemperature();
            float current = vshunt / INA229_RSHUNT_OHM;
            float power   = vbus * current;

            INA229_Data.vshunt      = vshunt;
            INA229_Data.vbus        = vbus;
            INA229_Data.temperature = temp;
            INA229_Data.current     = current;
            INA229_Data.power       = power;
            INA229_Data.energy      = INA229_ReadEnergy();
            INA229_Data.charge      = INA229_ReadCharge();

#if INA229_PRINTF_ENABLE
            if (g_OutputMode == OUTPUT_FLOAT) {
                /* JUSTFLOAT: 5×float 小端 + 帧尾 = 24 字节 */
                uint8_t frame[24];
                float  *pf = (float *)frame;
                pf[0] = vshunt;
                pf[1] = vbus;
                pf[2] = current;
                pf[3] = power;
                pf[4] = temp;
                frame[20] = 0x00; frame[21] = 0x00;
                frame[22] = 0x80; frame[23] = 0x7F;
                HAL_UART_Transmit(&huart2, frame, 24, 100);
            } else {
                printf("[INA229]:Vsh=%.3fuV,Vbus=%.3fmV,I=%.3fmA,P=%.3fmW,T=%.3fdC\r\n",
                       vshunt * 1e6f, vbus * 1e3f,
                       current * 1e3f, power * 1e3f,
                       temp * 10.0f);
            }
#endif

            INA229_TaskId = 20;
        }
        break;

        default:
            break;
    }
}

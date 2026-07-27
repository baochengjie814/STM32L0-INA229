/**
  ******************************************************************************
  * INA229_Task.c — V2.3 全速优化
  * 优化: _CalcInterval 缓存 / TEXT 整数字符串 / 帧尾静态化
  ******************************************************************************
  */
#include "INA229_Task.h"
#include "ina229.h"
#include "system.h"
#include "uart_protocol.h"
#include <stdio.h>

/* 查找表 */
static const uint16_t _ct_us[]   = {50, 84, 150, 280, 540, 1052, 2074, 4120};
static const uint16_t _avg_cnt[] = {1, 4, 16, 64, 128, 256, 512, 1024};

/* JUSTFLOAT 帧尾 (小端 +Inf) */
static const uint8_t _frame_tail[4] = {0x00, 0x00, 0x80, 0x7F};

static u16 _CalcInterval(void)
{
    uint32_t us   = (uint32_t)_ct_us[g_CtSetting] * 3 * _avg_cnt[g_AvgSetting];
    u16      tick = (u16)((us + 99) / 100);
    if (tick < 1) tick = 1;
    if (g_OutputMode == OUTPUT_FLOAT) { if (tick < 2) tick = 2; }
    else                              { if (tick < 50) tick = 50; }
    return tick;
}

/*===========================================================================
 * DMA 发送
 *===========================================================================*/
#define TX_BUF_SIZE  128
static uint8_t         tx_buf[TX_BUF_SIZE];
static volatile bool   tx_busy = false;
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart == &huart2) tx_busy = false;
}

/*===========================================================================
 * 全局变量
 *===========================================================================*/
volatile INA229_Data_t INA229_Data;
volatile u16            INA229_TaskId  = 20;
volatile u16            INA229_TaskTim = 0;
extern volatile u32     number_count;

/*===========================================================================
 * 主任务
 *===========================================================================*/
void INA229_Task(void)
{
    /* 缓存: CT/AVG/Mode 变化时才重算 interval */
    static u16  _interval     = 998;   /* 默认 CT6+AVG2 */
    static u8   _last_ct      = 0xFF;
    static u8   _last_avg     = 0xFF;
    static u8   _last_mode    = 0xFF;
    static u16  _frame_cnt    = 0;
    static u16  _fps_val      = 0;
    static u32  _last_report  = 0;

    /*---------- CT/AVG/Mode 变化时重算间隔 ----------*/
    u8 ct   = (u8)g_CtSetting;
    u8 avg  = (u8)g_AvgSetting;
    u8 mode = (u8)g_OutputMode;
    if (ct != _last_ct || avg != _last_avg || mode != _last_mode) {
        _last_ct   = ct;
        _last_avg  = avg;
        _last_mode = mode;
        _interval  = _CalcInterval();
    }

    /*---------- 每秒统计帧率 ----------*/
    if (number_count - _last_report >= 10000) {
        _last_report = number_count;
        _fps_val     = _frame_cnt;
        _frame_cnt   = 0;
        if (mode == OUTPUT_TEXT) {
            char buf[16];
            int len = snprintf(buf, sizeof(buf), "FPS=%d\r\n", (int)_fps_val);
            if (!tx_busy && len > 0) {
                tx_busy = true;
                HAL_UART_Transmit_DMA(&huart2, (uint8_t *)buf, (uint16_t)len);
            }
        }
    }

    switch (INA229_TaskId)
    {
        case 20:
        {
            if (INA229_TaskTim >= _interval) {
                INA229_TaskTim = 0;
                INA229_TaskId = 30;
            }
        }
        break;

        case 30:
        {
            float vbus    = INA229_ReadVBus();
            float temp    = 0.0f;
            if (g_LcdTaskEnable || mode != OUTPUT_FLOAT)
                temp = INA229_ReadTemperature();
            float current = INA229_ReadCurrent(0);
            float power   = vbus * current;

            INA229_Data.vbus        = vbus;
            INA229_Data.temperature = temp;
            INA229_Data.current     = current;
            INA229_Data.power       = power;

            _frame_cnt++;

#if INA229_PRINTF_ENABLE
            if (mode == OUTPUT_FLOAT) {
                uint8_t frame[24];
                float  *pf = (float *)frame;
                pf[0] = vbus;
                pf[1] = current;
                pf[2] = power;
                pf[3] = temp;
                pf[4] = (float)_fps_val;
                memcpy(&frame[20], _frame_tail, 4);
                if (!tx_busy) {
                    memcpy(tx_buf, frame, 24);
                    tx_busy = true;
                    HAL_UART_Transmit_DMA(&huart2, tx_buf, 24);
                }
            } else {
                /* 整数格式化, 无浮点 snprintf (M0+ 提速) */
                int vi = (int)(vbus    * 1000.0f + 0.5f);
                int ci = (int)(current * 1000.0f + 0.5f);
                int pi = (int)(power   * 1000.0f + 0.5f);
                int ti = (int)(temp    * 10.0f   + 0.5f);
                int len = snprintf((char *)tx_buf, TX_BUF_SIZE,
                           "[INA229]:Vbus=%d.%03dmV,I=%d.%03dmA,P=%d.%03dmW,T=%d.%01ddC\r\n",
                           vi/1000, vi%1000, ci/1000, ci%1000,
                           pi/1000, pi%1000, ti/10, ti%10);
                if (!tx_busy && len > 0) {
                    tx_busy = true;
                    HAL_UART_Transmit_DMA(&huart2, tx_buf, (uint16_t)len);
                }
            }
#endif

            INA229_TaskId = 20;
        }
        break;

        default:
            break;
    }
}

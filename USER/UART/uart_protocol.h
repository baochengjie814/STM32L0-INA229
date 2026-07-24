/**
  ******************************************************************************
  * 文件名程: uart_protocol.h
  * 作    者:
  * 版    本: V1.0
  * 编写日期: 2026-07-24
  * 功    能: 串口通讯协议 — DMA+IDLE 接收, 命令解析
  *
  * 通讯协议:
  *   波特率 2M, 8N1, 命令以 \r\n 结尾
  *
 *   命令列表:
 *     CT=N      设置转换时间 (N=0~7)
 *     AVG=N     设置平均次数  (N=0~7)
 *     LCD=ON    开启 LCD 刷新任务
 *     LCD=OFF   关闭 LCD 刷新任务
 *     MODE=TEXT 文本输出模式 (默认)
 *     MODE=FLOAT JUSTFLOAT 二进制模式
 *     STATUS    查看当前状态
 *     HELP      显示帮助
 *
 *   JUSTFLOAT 帧格式 (MODE=FLOAT):
 *     20 字节: float Vsh + float Vbus + float I + float P (小端) + 0x00,0x00,0x80,0x7F
 *
 *   响应:
 *     OK\r\n        命令执行成功
 *     ERR\r\n       命令格式错误
 *     CT=5\r\n      当前转换时间
 *     AVG=2\r\n     当前平均次数
 *     LCD=ON\r\n    LCD 状态
 *     MODE=TEXT\r\n 当前输出模式
 ******************************************************************************
 */

#ifndef __UART_PROTOCOL_H
#define __UART_PROTOCOL_H

#include "main.h"
#include "sys.h"
#include "ina229.h"
#include <stdbool.h>

/* DMA 接收缓冲区大小 */
#define UART_RX_BUF_SIZE    128

/* 输出模式 */
typedef enum {
    OUTPUT_TEXT  = 0,   /* 文本 printf */
    OUTPUT_FLOAT = 1    /* JUSTFLOAT 二进制帧 */
} OutputMode_t;

extern volatile OutputMode_t g_OutputMode;

/* LCD 任务开关 (外部模块读取) */
extern volatile bool g_LcdTaskEnable;

/* 当前 INA229 配置 (供 INA229_Task 计算采样周期) */
extern INA229_ConvTime_t  g_CtSetting;
extern INA229_AvgCount_t  g_AvgSetting;

void UART_Protocol_Init(void);
void UART_Protocol_Process(void);

#endif

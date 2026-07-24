/**
  ******************************************************************************
  * 文件名程: uart_protocol.c
  * 作    者:
  * 版    本: V1.0
  * 编写日期: 2026-07-24
  * 功    能: 串口 DMA+IDLE 接收 + 命令解析
  *
  * 工作流程:
  *   1. 上电启动 DMA 循环接收 (IDLE 中断检测帧尾)
  *   2. 收到完整帧 → 回调中解析命令 → 执行 → 回复
  *   3. 重启 DMA 接收下一帧
  ******************************************************************************
  */

#include "uart_protocol.h"
#include "system.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/*===========================================================================
 * 全局变量
 *===========================================================================*/

volatile bool g_LcdTaskEnable = true;
volatile OutputMode_t g_OutputMode = OUTPUT_TEXT;  /* 默认文本模式 */

/* DMA 接收缓冲区 */
static uint8_t  rx_buf[UART_RX_BUF_SIZE];
static uint8_t  rx_line[UART_RX_BUF_SIZE];       /* 完整一行的副本 */
static volatile bool rx_done = false;               /* 一帧接收完成标志 */

/* 当前 INA229 设定 (上电默认值) */
INA229_ConvTime_t  g_CtSetting  = INA229_CT_1052US;
INA229_AvgCount_t  g_AvgSetting = INA229_AVG_16;

/*===========================================================================
 * 内部: 发送响应字符串
 *===========================================================================*/

static void _Reply(const char *msg)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), 100);
}

/*===========================================================================
 * 内部: 命令解析与执行
 *===========================================================================*/

static void _ParseCmd(char *cmd)
{
    int val;

    /*---------- CT=N: 设置转换时间 ----------*/
    if (strncmp(cmd, "CT=", 3) == 0) {
        val = atoi(cmd + 3);
        if (val >= 0 && val <= 7) {
            g_CtSetting = (INA229_ConvTime_t)val;
            INA229_SetConversionTime(g_CtSetting, g_CtSetting, g_CtSetting);
            _Reply("OK\r\n");
        } else {
            _Reply("ERR\r\n");
        }
        return;
    }

    /*---------- AVG=N: 设置平均次数 ----------*/
    if (strncmp(cmd, "AVG=", 4) == 0) {
        val = atoi(cmd + 4);
        if (val >= 0 && val <= 7) {
            g_AvgSetting = (INA229_AvgCount_t)val;
            INA229_SetAveraging(g_AvgSetting);
            _Reply("OK\r\n");
        } else {
            _Reply("ERR\r\n");
        }
        return;
    }

    /*---------- LCD=ON/OFF: 开关 LCD 任务 ----------*/
    if (strcmp(cmd, "LCD=ON") == 0) {
        g_LcdTaskEnable = true;
        _Reply("OK\r\n");
        return;
    }
    if (strcmp(cmd, "LCD=OFF") == 0) {
        g_LcdTaskEnable = false;
        _Reply("OK\r\n");
        return;
    }

    /*---------- MODE=TEXT/FLOAT: 切换输出模式 ----------*/
    if (strcmp(cmd, "MODE=TEXT") == 0) {
        g_OutputMode = OUTPUT_TEXT;
        _Reply("OK\r\n");
        return;
    }
    if (strcmp(cmd, "MODE=FLOAT") == 0) {
        g_OutputMode = OUTPUT_FLOAT;
        _Reply("OK\r\n");
        return;
    }

    /*---------- STATUS: 查询当前状态 ----------*/
    if (strcmp(cmd, "STATUS") == 0) {
        char buf[64];
        snprintf(buf, sizeof(buf),
                 "CT=%d\r\nAVG=%d\r\nLCD=%s\r\nMODE=%s\r\n",
                 (int)g_CtSetting, (int)g_AvgSetting,
                 g_LcdTaskEnable ? "ON" : "OFF",
                 g_OutputMode == OUTPUT_FLOAT ? "FLOAT" : "TEXT");
        _Reply(buf);
        return;
    }

    /*---------- HELP ----------*/
    if (strcmp(cmd, "HELP") == 0) {
        _Reply("Commands:\r\n"
               "  CT=0..7     Set conversion time\r\n"
               "  AVG=0..7    Set averaging\r\n"
               "  LCD=ON|OFF  Toggle LCD\r\n"
               "  MODE=TEXT|FLOAT  Output format\r\n"
               "  STATUS      Show status\r\n");
        return;
    }

    /* 未知命令 */
    _Reply("ERR\r\n");
}

/*===========================================================================
 * HAL 回调: DMA+IDLE 接收完成
 *===========================================================================*/

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart == &huart2 && Size > 0 && Size < UART_RX_BUF_SIZE) {
        /* 拷贝到行缓冲区, 确保以 \0 结尾 */
        memcpy(rx_line, rx_buf, Size);
        rx_line[Size] = '\0';

        /* 去掉尾部 \r\n */
        while (Size > 0 && (rx_line[Size - 1] == '\r' || rx_line[Size - 1] == '\n'))
            rx_line[--Size] = '\0';

        rx_done = true;
    }

    /* 重启 DMA 接收 */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_buf, UART_RX_BUF_SIZE);
}

/*===========================================================================
 * 初始化
 *===========================================================================*/

void UART_Protocol_Init(void)
{
    /* 启动 DMA + IDLE 中断接收 */
    HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_buf, UART_RX_BUF_SIZE);

    printf("[UART] Protocol started, baud=2M\r\n");
}

/*===========================================================================
 * 主循环调用: 处理收到的命令
 *===========================================================================*/

void UART_Protocol_Process(void)
{
    if (rx_done) {
        rx_done = false;
        _ParseCmd((char *)rx_line);
    }
}

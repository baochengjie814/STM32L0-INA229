#include "global_control.h"
#include "system.h"
#include "INA229_Task.h"
#include "lcd_task.h"

volatile uint32_t number_count;


void Global_Init(void)
{
    HAL_Delay(100);

    /* 软件 SPI 引脚初始化 */
    Soft_SPI_Init();

    /* INA229 初始化 (20mΩ分流电阻, 10A 最大预期电流) */
    if (INA229_Init(0.020f, 10.0f))
    {
        printf("[INA229] Init OK\r\n");
    }
    else
    {
        printf("[INA229] Init FAILED!\r\n");
    }

    /* 回读确认 ADC_CONFIG 写入是否生效 */
    {
        uint16_t adc_cfg = INA229_ReadReg16(INA229_REG_ADC_CONFIG);
        printf("[INA229] ADC_CONFIG readback=0x%04X\r\n", adc_cfg);
    }

    /* 额外诊断: 直接读 ID 寄存器 */
    {
        uint16_t manu_id = INA229_ReadManufacturerID();
        uint16_t dev_id  = INA229_ReadDeviceID();
        printf("[INA229] ManufacturerID=0x%04X (expect 0x5449), DeviceID=0x%04X (expect 0x2291)\r\n",
               manu_id, dev_id);

        if (manu_id == 0x5449 && dev_id == 0x2291)
            printf("[INA229] SPI communication OK\r\n");
        else
            printf("[INA229] SPI communication FAILED -- check wiring/power\r\n");
    }

    /* LCD 初始化 */
    LCD_Init();

    /* 启动 1kHz 定时器 */
    HAL_TIM_Base_Start_IT(&htim2);

    /* 启动串口通讯协议 (DMA + IDLE 接收) */
    UART_Protocol_Init();
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == htim2.Instance)                        //1KHZ 1ms
    {
        LcdTaskTim++;
				number_count++;
        INA229_TaskTim++;
    }
}


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

    /* INA229 初始化 (1mΩ分流电阻, 10A 最大预期电流) */
    INA229_Init(0.001f, 10.0f);

    /* LCD 初始化 */
    LCD_Init();

    /* 启动 1kHz 定时器 */
    HAL_TIM_Base_Start_IT(&htim2);
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == htim2.Instance)                        //1KHZ 1ms
    {
        LcdTaskTim++;
			
        INA229_TaskTim++;
    }
}


#include "global_control.h"
#include "system.h"



volatile uint32_t number_count;


void Global_Init(void)
{
    HAL_Delay(100);                                         
                 
		HAL_TIM_Base_Start_IT(&htim2);   

}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if(htim->Instance == htim2.Instance)                        //1KHZ 1ms
    {
			
    }

}


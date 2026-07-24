/**

  ******************************************************************************

  * 文件名程: lcd_task.c

  * 作    者: 

  * 版    本: V1.0

  * 编写日期: 

  * 功    能: 定时进行lcd的显示任务

  ******************************************************************************

  */

/* 包含头文件 ---------------------------------------------------------------*/

#include "lcd_task.h"

#include "lcd_drv.h"

#include "lcd_mid.h"

#include "system.h"



volatile u16 LcdTaskId = 10;

volatile u16 LcdTaskTim = 0;



extern u8 KeyNum;

extern s8 YCursor;

extern s8 YCursorLast;

extern u8 InitPage1;

extern u8 InitPage2;





/**

  * 函数功能: 定时进行lcd的显示任务

  * 输入参数:

  * 返 回 值: 

  * 说    明:

  */

	

void Lcd_Task(void)

{

    switch(LcdTaskId)

    {

			   case 10:

        {

            if(LcdTaskTim>=100)        //100ms

            {

                LcdTaskTim = 0;

                LcdTaskId = 20;

            }

        }

				break;

        case 20:

        {                

            LCD_Display_Page1();

            LcdTaskId = 10; 

				}

		}

}


/**
  ******************************************************************************
  * 文件名程: lcd_task.c
  * 作    者: 浩盛科技
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
//void Lcd_Task(void)
//{
//    switch(LcdTaskId)
//    {
//        case 10:
//        {
//            if(LcdTaskTim>=1000)        //200ms
//            {
//                LcdTaskTim = 0;
//                LcdTaskId = 20;
//            }
//        }
//        break;
//        
//        case 20:
//        {                
//            LCD_Display_Page1();
//            LcdTaskId = 10; 
//            if(KeyNum == 4)
//            {
//                KeyNum = 0;
//                InitPage2 = 0;
//                LCD_Clear();
//                LcdTaskId = 30;
//            }                    
//            if(KeyNum == 2)
//            {
//                KeyNum = 0;
////                InitPage1 = 0;
////                LCD_Clear();
//                Speed_Set_Dir = -Speed_Set_Dir;
//                LcdTaskId = 10;            
//            }
//        }
//        break;
//        
//        case 30:
//        { 
//             LCD_Display_Page2();
//            if(KeyNum == 1)
//            {
//                YCursorLast = YCursor;
//                KeyNum = 0;
//                YCursor += 16;
//                if(YCursor > 64)YCursor = 0;
//            }        

//            if(KeyNum == 3)
//            {
//                YCursorLast = YCursor;
//                KeyNum = 0;
//                YCursor -= 16;
//                if(YCursor < 0)YCursor = 64;        
//            }
//            
//            if(KeyNum == 2)
//            {
//                KeyNum = 0;
//                InitPage1 = 0;
//                LCD_Clear();
//                LcdTaskId = 10;            
//            }

//            if(KeyNum == 4)
//            {
//                KeyNum = 0;
//                InitPage1 = 0;
//                LCD_Clear();
//                LcdTaskId = 10;    

//                switch(YCursor)
//                {
//                    case 0:
//                    {
//                        MC.Motor.RunState = MOTOR_SENSORUSE;
//                        MC.Motor.RunMode = CURRENT_CLOSE_LOOP;
//                    }break;
//                    
//                    case 16:
//                    {
//                        MC.Motor.RunState = MOTOR_SENSORUSE;
//                        MC.Motor.RunMode = SPEED_CURRENT_LOOP;
//                    }break;
//        
//                    case 32:
//                    {
//                        MC.Motor.RunState = MOTOR_SENSORUSE;
//                        MC.Motor.RunMode = POS_SPEED_CURRENT_LOOP;
//                    }break;
//                    
//                    case 48:
//                    {
//                        MC.Motor.RunState = MOTOR_SENSORLESS;
//                        MC.Motor.RunMode = STRONG_DRAG_SMO_SPEED_CURRENT_LOOP;
//                    }break;
//                    
//                    case 64:
//                    {
//                        MC.Motor.RunState = MOTOR_SENSORLESS;
//                        MC.Motor.RunMode = HFI_SMO_SPEED_CURRENT_CLOSE;
//                        MC.HFI.NSDFlag = 0;
//                    }break;
//                }        
//            }                
//        }
//        break;
//        
//    default:
//      break;            
//    }
//}




#ifndef __SYSTEM_H
#define __SYSTEM_H


//extern MotorCmd_t motor_send[3];
//extern MotorData_t motor_ricive[3];

#define SATURATE(_IN, _MIN, _MAX) \
	{                             \
		if ((_IN) <= (_MIN))      \
			(_IN) = (_MIN);       \
		else if ((_IN) >= (_MAX)) \
			(_IN) = (_MAX);       \
	}

#define PI 3.14159265358979323846
#define Log 1



#include "main.h"
#include "usart.h"
//#include "dma.h"
#include "gpio.h"
#include "tim.h"
//#include "adc.h"
//#include "dac.h"
//#include "i2c.h"
#include "spi.h"
	
#include "ina229.h"
#include "soft_spi.h"


#include "delay.h"
#include "sys.h"

////	

//#include "lcd_drv.h"
//#include "lcd_mid.h"

//#include "key_drv.h"

//#include "global_control.h"

//#include "led_task.h"
//#include "key_task.h"
////#include "usart_task.h"
//#include "lcd_task.h"
//#include "as7343_task.h"

#include <stdio.h> 
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "stdarg.h"
#include <stdbool.h>

#include "arm_math.h"

u32 myabs(long int a);
int fputc(int ch, FILE *f);
arm_status signed_sqrt_f32(float32_t x, float32_t *result);
#endif 

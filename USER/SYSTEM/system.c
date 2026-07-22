#include "system.h"

u32 myabs(long int a)
{ 		   
	  u32 temp;
		if(a<0)  temp=-a;  
	  else temp=a;
	  return temp;
}

int fputc(int ch, FILE *f)
{
		uint8_t temp[1] = {ch};
		HAL_UART_Transmit(&huart2, temp, 1, 2);//huart1??????????	HAL_MAX_DELAY
		return ch;
}
#include "arm_math.h"

/*
 * 带符号开根号：
 *  x >= 0 -> result = +sqrt(x)
 *  x <  0 -> result = -sqrt(|x|)
 *
 * 返回值：
 *  ARM_MATH_SUCCESS        成功
 *  ARM_MATH_ARGUMENT_ERROR  输入或输出指针非法
 */
arm_status signed_sqrt_f32(float32_t x, float32_t *result)
{
    arm_status status;
    float32_t abs_x;

    if (result == NULL)
    {
        return ARM_MATH_ARGUMENT_ERROR;
    }

    // 取绝对值
    if (x >= 0.0f)
    {
        abs_x = x;
    }
    else
    {
        abs_x = -x;
    }

    // 调用 CMSIS-DSP 平方根函数
    status = arm_sqrt_f32(abs_x, result);
    if (status != ARM_MATH_SUCCESS)
    {
        return status;
    }

    // 如果原数是负数，结果也加负号
    if (x < 0.0f)
    {
        *result = -(*result);
    }

    return ARM_MATH_SUCCESS;
}

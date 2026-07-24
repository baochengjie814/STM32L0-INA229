/*===========================================================================
 * 系统工具函数
 *===========================================================================*/

#include "system.h"
#include "arm_math.h"

/*===========================================================================
 * 数学工具
 *===========================================================================*/

/**
 * @brief  取绝对值 (32-bit)
 * @param  a 有符号整数
 * @return 绝对值
 */
u32 myabs(long int a)
{
    u32 temp;

    if (a < 0)
        temp = -a;
    else
        temp = a;

    return temp;
}

/**
 * @brief  带符号的平方根
 * @param  x       输入值 (正数→ +sqrt(x), 负数→ -sqrt(|x|))
 * @param  result  输出结果指针
 * @return ARM_MATH_SUCCESS / ARM_MATH_ARGUMENT_ERROR
 */
arm_status signed_sqrt_f32(float32_t x, float32_t *result)
{
    arm_status status;
    float32_t  abs_x;

    if (result == NULL)
        return ARM_MATH_ARGUMENT_ERROR;

    /* 取绝对值 */
    abs_x = (x >= 0.0f) ? x : -x;

    /* 调用 CMSIS-DSP 平方根函数 */
    status = arm_sqrt_f32(abs_x, result);
    if (status != ARM_MATH_SUCCESS)
        return status;

    /* 原数为负 → 结果取反 */
    if (x < 0.0f)
        *result = -(*result);

    return ARM_MATH_SUCCESS;
}

/*===========================================================================
 * 串口重定向 (printf → USART2)
 *===========================================================================*/

/**
 * @brief  重定向 printf 到 USART2
 * @param  ch 待发送字符
 * @param  f  文件流 (未使用)
 * @return 发送的字符
 */
int fputc(int ch, FILE *f)
{
    uint8_t temp[1] = {ch};
    HAL_UART_Transmit(&huart2, temp, 1, 2);
    return ch;
}

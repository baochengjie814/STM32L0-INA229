// delay.c - 使用 SysTick 实现微秒延时
#include "delay.h"
#include "stm32l0xx_hal.h"

static uint32_t fac_us = 0;   // 每微秒对应的 SysTick 计数

/**
 * @brief 初始化微秒延时（在 main 中调用一次）
 */
void delay_init(void)
{
    // 获取 CPU 频率（Hz）
    uint32_t cpu_hz = SystemCoreClock;
    
    // 计算每微秒需要的 SysTick 计数
    // 注意：SysTick->LOAD 最大值 0xFFFFFF，所以 cpu_hz/1e6 必须小于 0xFFFFFF
    // 对于 32MHz，cpu_hz/1e6 = 32，完全没问题
    fac_us = cpu_hz / 1000000U;
    
    // 配置 SysTick：使用 HCLK，关闭中断，不使能
    SysTick->CTRL = 0;                    // 先关闭
    SysTick->LOAD = 0xFFFFFF;             // 设置最大装载值（方便任意长度延时）
    SysTick->VAL  = 0;                    // 清空当前值
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;  // 使能，无中断
}

/**
 * @brief 微秒延时（阻塞式）
 * @param us 延时微秒数
 */
void delay_us(uint32_t us)
{
    if (us == 0) return;
    
    // 计算目标计数值
    uint32_t ticks = us * fac_us;
    uint32_t start = SysTick->VAL;
    uint32_t current;
    
    do
    {
        current = SysTick->VAL;
        // 处理溢出：如果 current <= start，正常差值；如果 current > start，说明发生了溢出
        if (current > start)
        {
            // 发生了溢出，需要加上 LOAD 值
            if ((SysTick->LOAD - current + start) >= ticks)
                break;
        }
        else
        {
            if ((start - current) >= ticks)
                break;
        }
    } while (1);
}

/**
 * @brief 毫秒延时（阻塞式）
 * @param ms 延时毫秒数
 */
void delay_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        delay_us(1000);
    }
}
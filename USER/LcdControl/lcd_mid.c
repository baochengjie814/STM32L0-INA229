/**
  ******************************************************************************
  * 文件名程: lcd_mid.c
  * 作    者:
  * 版    本: V1.0
  * 编写日期:
  * 功    能: LCD 页面显示 (Page1: INA229 差量刷新)
  ******************************************************************************
  */
/* 头文件 ---------------------------------------------------------------*/
#include "lcd_mid.h"
#include "lcd_drv.h"
#include "system.h"
#include "INA229_Task.h"
#include <math.h>

static u8  InitPage1 = 0;

#define FONT_SIZE  24
#define VAL_X      72
#define ROW1_Y     0
#define ROW2_Y     30
#define ROW3_Y     60
#define ROW4_Y     90

/* 差量刷新阈值 (LSD: 显示最小位变化) */
#define THR_V   0.001f   /* 1mV / 1mV  */
#define THR_I   0.001f   /* 1mA / 1mA  */
#define THR_P   0.001f   /* 1mW / 1mW  */
#define THR_T   0.05f    /* 0.05°C      */

/* 上一帧缓存 */
static float _last_v = -999.0f, _last_i = -999.0f, _last_p = -999.0f, _last_t = -999.0f;
static u8    _last_unit_v = 0, _last_unit_i = 0, _last_unit_p = 0;

/**
 * 自动单位切换 + 差量判断
 * 返回 1=已刷新, 0=跳过(值未变)
 */
static int _ShowAutoUnit(u16 x, u16 y, float raw_val, float threshold,
                         const char *ub, const char *us,
                         float *last_val, u8 *last_unit,
                         u16 fc, u16 bc, u8 sizey)
{
    const char *unit;
    float display_val;

    /* 自动单位切换 */
    if (raw_val < 1.0f && raw_val > -1.0f) {
        display_val = raw_val * 1000.0f;
        unit = us;
        if (*last_unit != 1) { *last_unit = 1; *last_val = -999.0f; }  /* 单位变了, 强制刷新 */
    } else {
        display_val = raw_val;
        unit = ub;
        if (*last_unit != 0) { *last_unit = 0; *last_val = -999.0f; }
    }

    /* 差量判断: 值与上帧相同则跳过 */
    if (fabsf(display_val - *last_val) < threshold)
        return 0;

    *last_val = display_val;

    /* 数值: 左对齐, 8字符宽 */
    char buf[10];
    snprintf(buf, sizeof(buf), "%.3f", display_val);
    LCD_ShowPrintf(x,      y, fc, bc, sizey, 0, "%-8s", buf);

    /* 单位: 左对齐, 3字符宽 */
    LCD_ShowPrintf(x + 96, y, fc, bc, sizey, 0, "%-3s", unit);

    return 1;
}

void LCD_Display_Page1(void)
{
    InitPage1++;
    if (InitPage1 == 1)
    {
        /* 静态标签 */
        LCD_ShowString(0, ROW1_Y, (u8 *)"VBUS", WHITE, RED,    FONT_SIZE, 0);
        LCD_ShowString(0, ROW2_Y, (u8 *)"CURR", WHITE, YELLOW, FONT_SIZE, 0);
        LCD_ShowString(0, ROW3_Y, (u8 *)"PWR",  WHITE, LIGHTBLUE, FONT_SIZE, 0);
        LCD_ShowString(0, ROW4_Y, (u8 *)"TEMP", WHITE, GREEN,  FONT_SIZE, 0);
    }

    if (InitPage1 >= 2)
    {
        InitPage1 = 2;

        /* VBUS: <1V → mV */
        _ShowAutoUnit(VAL_X, ROW1_Y, INA229_Data.vbus, THR_V, "V", "mV",
                      &_last_v, &_last_unit_v, ROSE_PINK, BLACK, FONT_SIZE);

        /* CURR: <1A → mA */
        _ShowAutoUnit(VAL_X, ROW2_Y, INA229_Data.current, THR_I, "A", "mA",
                      &_last_i, &_last_unit_i, ROSE_PINK, BLACK, FONT_SIZE);

        /* PWR: <1W → mW */
        _ShowAutoUnit(VAL_X, ROW3_Y, INA229_Data.power, THR_P, "W", "mW",
                      &_last_p, &_last_unit_p, ROSE_PINK, BLACK, FONT_SIZE);

        /* TEMP: 固定 °C, 差量刷新 */
        if (InitPage1 == 2 || fabsf(INA229_Data.temperature - _last_t) >= THR_T) {
            _last_t = INA229_Data.temperature;
            LCD_ShowPrintf(VAL_X, ROW4_Y, ROSE_PINK, BLACK, FONT_SIZE, 0,
                           "%.1f C", INA229_Data.temperature);
        }
    }
}

void LCD_Display_Page2(void) {}
void LCD_Clear(void) { LCD_Fill(0, 0, LCD_W, LCD_H, BLACK); }

void LCD_Display_Logo(void)
{
#if 0  /* Logo 禁用, 节省 Flash */
    extern const unsigned char gImage_hs_logo[8052];
    LCD_Init();
    LCD_ShowPicture(89, 34, 61, 66, gImage_hs_logo);
    HAL_Delay(1500);
#endif
    LCD_Fill(0, 0, LCD_W, LCD_H, BLACK);
}

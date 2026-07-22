#ifndef __LCD_DRV_H
#define __LCD_DRV_H


#include "main.h"
#include "sys.h"

#define USE_HORIZONTAL 3  //设置横屏或者竖屏显示 0或1为竖屏 2或3为横屏


#if USE_HORIZONTAL==0||USE_HORIZONTAL==1
#define LCD_W 80
#define LCD_H 160

#else
#define LCD_W 160
#define LCD_H 80
#endif

//-----------------LCD端口定义---------------- 


#define	LCD_PWR(n)		(n?HAL_GPIO_WritePin(LCD_LEDA_GPIO_Port,LCD_LEDA_Pin,GPIO_PIN_SET):HAL_GPIO_WritePin(LCD_LEDA_GPIO_Port,LCD_LEDA_Pin,GPIO_PIN_RESET))

#define LCD_RES_Clr()  HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin,GPIO_PIN_RESET)//DC
#define LCD_RES_Set()  HAL_GPIO_WritePin(LCD_RES_GPIO_Port, LCD_RES_Pin,GPIO_PIN_SET)
#define LCD_DC_Clr()   HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin,GPIO_PIN_RESET)//DC
#define LCD_DC_Set()   HAL_GPIO_WritePin(LCD_RS_GPIO_Port, LCD_RS_Pin,GPIO_PIN_SET)
#define LCD_CS_Clr()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin,GPIO_PIN_RESET)//DC
#define LCD_CS_Set()   HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin,GPIO_PIN_SET)

// ==================== RGB565 颜色转换宏 ====================
// 将 0~255 的 RGB 值转换为 16位 RGB565 格式
// R: 0~255, G: 0~255, B: 0~255
#define RGB565(r, g, b)  ((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3))

// ==================== 画笔颜色定义 ====================
// 基础颜色（使用 RGB565 宏）
#define WHITE      RGB565(255, 255, 255)  // 0xFFFF
#define BLACK      RGB565(0,   0,   0)    // 0x0000
#define RED        RGB565(255, 0,   0)    // 0xF800
#define GREEN      RGB565(0,   255, 0)    // 0x07E0
#define BLUE       RGB565(0,   0,   255)  // 0x001F
#define YELLOW     RGB565(255, 255, 0)    // 0xFFE0
#define CYAN       RGB565(0,   255, 255)  // 0x7FFF
#define MAGENTA    RGB565(255, 0,   255)  // 0xF81F

// 扩展颜色
#define BROWN      RGB565(165, 42,  42)   // 0xBC40 棕色
#define BRRED      RGB565(252, 7,   7)    // 0xFC07 棕红色
#define GRAY       RGB565(128, 128, 128)  // 0x8430 灰色
#define DARKBLUE   RGB565(0,   0,   128)  // 0x01CF 深蓝色
#define LIGHTBLUE  RGB565(173, 216, 230)  // 0x7D7C 浅蓝色
#define GRAYBLUE   RGB565(84,  88,  88)   // 0x5458 灰蓝色
#define LIGHTGREEN RGB565(132, 31,  31)   // 0x841F 浅绿色
#define LGRAY      RGB565(192, 192, 192)  // 0xC618 浅灰色
#define LGRAYBLUE  RGB565(166, 81,  81)   // 0xA651 浅灰蓝色
#define LBBLUE     RGB565(43,  18,  18)   // 0x2B12 浅棕蓝色
#define ROSE_PINK  RGB565(252, 243, 243)  // 0xFCF3 玫瑰粉
#define PINK       RGB565(255, 192, 203)  // 0xFE19 标准粉
#define RED_ORANGE RGB565(252, 128, 0)    // 0xFC80 红橙色 (FXL专用)

// ==================== 颜色别名（保留原有命名习惯） ====================
#define BRED       BRRED        // 兼容旧代码
#define GRED       RGB565(255, 224, 0)    // 0xFFE0 绿红色（实际是黄绿色）
#define GBLUE      RGB565(7,   255, 255)  // 0x07FF 蓝绿色（实际是青色）

// ==================== 常用颜色参考（供调试使用） ====================
#define MY_RED     RED          // 纯红色
#define MY_GREEN   GREEN        // 纯绿色
#define MY_BLUE    BLUE         // 纯蓝色
#define ORANGE     RGB565(255, 165, 0)    // 0xFDA0 橙色

// ==================== AS7343 通道专用颜色 ====================
#define COLOR_FZ   BLUE         // 蓝色光谱 → 蓝底白字
#define COLOR_FY   YELLOW       // 黄色光谱 → 黄底黑字
#define COLOR_FXL  RED_ORANGE   // 红橙色光谱 → 红橙底白字
#define COLOR_NIR  GRAY         // 近红外 → 灰底黑字
#define COLOR_VIS  CYAN         // 全可见光 → 青底黑字
#define COLOR_FD   LGRAY        // 闪烁检测 → 浅灰底黑字



void LCD_Writ_Bus(u8 dat);//模拟SPI时序
void LCD_WR_DATA8(u8 dat);//写入一个字节
void LCD_WR_DATA(u16 dat);//写入两个字节
void LCD_WR_REG(u8 dat);//写入一个指令
void LCD_Address_Set(u16 x1,u16 y1,u16 x2,u16 y2);//设置坐标函数
void LCD_Init(void);//LCD初始化

void LCD_Fill(u16 xsta,u16 ysta,u16 xend,u16 yend,u16 color);//指定区域填充颜色
void LCD_DrawPoint(u16 x,u16 y,u16 color);//在指定位置画一个点
void LCD_DrawLine(u16 x1,u16 y1,u16 x2,u16 y2,u16 color);//在指定位置画一条线
void LCD_DrawRectangle(u16 x1, u16 y1, u16 x2, u16 y2,u16 color);//在指定位置画一个矩形
void Draw_Circle(u16 x0,u16 y0,u8 r,u16 color);//在指定位置画一个圆

void LCD_ShowChinese(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);//显示汉字串
void LCD_ShowChinese12x12(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);//显示单个12x12汉字
void LCD_ShowChinese16x16(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);//显示单个16x16汉字
void LCD_ShowChinese24x24(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);//显示单个24x24汉字
void LCD_ShowChinese32x32(u16 x,u16 y,u8 *s,u16 fc,u16 bc,u8 sizey,u8 mode);//显示单个32x32汉字

void LCD_ShowChar(u16 x,u16 y,u8 num,u16 fc,u16 bc,u8 sizey,u8 mode);//显示一个字符
void LCD_ShowString(u16 x,u16 y,const u8 *p,u16 fc,u16 bc,u8 sizey,u8 mode);//显示字符串
u32 mypow(u8 m,u8 n);//求幂
void LCD_ShowIntNum(u16 x,u16 y,u16 num,u8 len,u16 fc,u16 bc,u8 sizey);//显示整数变量
void LCD_ShowFloatNum1(u16 x,u16 y,float num,u8 len,u16 fc,u16 bc,u8 sizey);//显示两位小数变量

void LCD_ShowPicture(u16 x,u16 y,u16 length,u16 width,const u8 pic[]);//显示图片
void LCD_ShowPrintf(u16 x, u16 y, u16 fc, u16 bc, u8 sizey, u8 mode, const char *fmt, ...);

#endif


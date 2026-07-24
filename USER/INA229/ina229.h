/**
  ******************************************************************************
  * 文件名程: ina229.h
  * 作    者:
  * 版    本: V1.0
  * 编写日期:
  * 功    能: INA229 85V/20位 超精密功率监控器 驱动 (软件 SPI 接口)
  *
  * SPI 帧格式 (读取):
  *   MOSI: | ADDR[5:0] | 0 | R/W=1 | ... 填充 ...              |
  *   MISO: | 8-bit 状态字节 | DATA[(N-1):0]                         |
  *
  * SPI 帧格式 (写入):
  *   MOSI: | ADDR[5:0] | 0 | R/W=0 | DATA[15:0]                     |
  *   MISO: | 8-bit 状态字节 | OLD_DATA[15:0]                        |
  *
  * 状态字节 (MISO 首字节):
  *   BIT[7]   = ALRT    (任一报警置位)
  *   BIT[6]   = CRDY    (所有使能通道转换完成)
  *   BIT[5]   = MATHOF  (算术溢出)
  *   BIT[4]   = RESERVED
  *   BIT[3]   = MEMSTAT (校验和错误)
  *   BIT[2:0] = RESERVED
  *
  * 寄存器宽度:
  *   16-bit: CONFIG, ADC_CONFIG, SHUNT_CAL, SHUNT_TEMPCO,
  *           DIETEMP, DIAG_ALRT, SOVL, SUVL, BOVL, BUVL,
  *           TEMP_LIMIT, PWR_LIMIT, MANUFACTURER_ID, DEVICE_ID
  *   24-bit: VSHUNT, VBUS, CURRENT, POWER
  *   40-bit: ENERGY, CHARGE
  *
  * 参考: INA229 数据手册 (SBOS938A)
  ******************************************************************************
  */

#ifndef __INA229_H__
#define __INA229_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 头文件 ---------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/*===========================================================================
 * 寄存器地址
 *===========================================================================*/

#define INA229_REG_CONFIG            0x00    /* 配置寄存器                (R/W, 16-bit) */
#define INA229_REG_ADC_CONFIG        0x01    /* ADC 配置寄存器            (R/W, 16-bit) */
#define INA229_REG_SHUNT_CAL         0x02    /* 分流校准寄存器            (R/W, 16-bit) */
#define INA229_REG_SHUNT_TEMPCO      0x03    /* 分流温度系数寄存器        (R/W, 16-bit) */
#define INA229_REG_VSHUNT            0x04    /* 分流电压测量值            (R,   24-bit) */
#define INA229_REG_VBUS              0x05    /* 总线电压测量值            (R,   24-bit) */
#define INA229_REG_DIETEMP           0x06    /* 芯片温度测量值            (R,   16-bit) */
#define INA229_REG_CURRENT           0x07    /* 电流计算结果              (R,   24-bit) */
#define INA229_REG_POWER             0x08    /* 功率计算结果              (R,   24-bit) */
#define INA229_REG_ENERGY            0x09    /* 电能累积结果              (R,   40-bit) */
#define INA229_REG_CHARGE            0x0A    /* 电荷累积结果              (R,   40-bit) */
#define INA229_REG_DIAG_ALRT         0x0B    /* 诊断标志与报警             (R/W, 16-bit) */
#define INA229_REG_SOVL              0x0C    /* 分流过压阈值              (R/W, 16-bit) */
#define INA229_REG_SUVL              0x0D    /* 分流欠压阈值              (R/W, 16-bit) */
#define INA229_REG_BOVL              0x0E    /* 总线过压阈值              (R/W, 16-bit) */
#define INA229_REG_BUVL              0x0F    /* 总线欠压阈值              (R/W, 16-bit) */
#define INA229_REG_TEMP_LIMIT        0x10    /* 过温阈值                  (R/W, 16-bit) */
#define INA229_REG_PWR_LIMIT         0x11    /* 过功率阈值                (R/W, 16-bit) */
#define INA229_REG_MANUFACTURER_ID   0x3E    /* 制造商 ID (TI=0x5449)     (R,   16-bit) */
#define INA229_REG_DEVICE_ID         0x3F    /* 器件 ID   (INA229=0x2291) (R,   16-bit) */

/*===========================================================================
 * CONFIG 寄存器 (0x00) 位定义
 *
 *   BIT[15]    = RST      复位, 写1自清除
 *   BIT[14]    = RSTACC   清除累加器, 写1自清除
 *   BIT[13:6]  = CONVDLY  转换延迟, 0~255, 步长 2ms
 *   BIT[5]     = TEMPCOMP 分流温度补偿使能
 *   BIT[4]     = ADCRANGE 0=±163.84mV, 1=±40.96mV
 *   BIT[3:0]   = RESERVED
 *===========================================================================*/

#define INA229_CONFIG_RST            (1 << 15)
#define INA229_CONFIG_RSTACC         (1 << 14)
#define INA229_CONFIG_CONVDLY_Pos    6
#define INA229_CONFIG_CONVDLY_Msk    (0xFF << 6)
#define INA229_CONFIG_TEMPCOMP       (1 << 5)
#define INA229_CONFIG_ADCRANGE       (1 << 4)

/*===========================================================================
 * ADC_CONFIG 寄存器 (0x01) 位定义
 *
 *   BIT[15:12] = MODE    工作模式
 *   BIT[11:9]  = VBUSCT  总线电压转换时间
 *   BIT[8:6]   = VSHCT   分流电压转换时间
 *   BIT[5:3]   = VTCT    温度转换时间
 *   BIT[2:0]   = AVG     平均值采样次数
 *===========================================================================*/

/*---------- 工作模式 ----------*/
#define INA229_ADC_MODE_Msk              (0xF << 12)
#define INA229_ADC_MODE_SHUTDOWN          (0x0 << 12)    /* 关断                              */
#define INA229_ADC_MODE_TRIG_VBUS         (0x1 << 12)    /* 触发: 仅总线电压                  */
#define INA229_ADC_MODE_TRIG_VSHUNT       (0x2 << 12)    /* 触发: 仅分流电压                  */
#define INA229_ADC_MODE_TRIG_VBUS_VSHUNT  (0x3 << 12)    /* 触发: 总线+分流                   */
#define INA229_ADC_MODE_TRIG_TEMP         (0x4 << 12)    /* 触发: 仅温度                      */
#define INA229_ADC_MODE_TRIG_TEMP_VBUS    (0x5 << 12)    /* 触发: 温度+总线                   */
#define INA229_ADC_MODE_TRIG_TEMP_VSHUNT  (0x6 << 12)    /* 触发: 温度+分流                   */
#define INA229_ADC_MODE_TRIG_ALL          (0x7 << 12)    /* 触发: 全部通道                    */
#define INA229_ADC_MODE_CONT_VBUS         (0x9 << 12)    /* 连续: 仅总线电压                  */
#define INA229_ADC_MODE_CONT_VSHUNT       (0xA << 12)    /* 连续: 仅分流电压                  */
#define INA229_ADC_MODE_CONT_VBUS_VSHUNT  (0xB << 12)    /* 连续: 总线+分流                   */
#define INA229_ADC_MODE_CONT_TEMP         (0xC << 12)    /* 连续: 仅温度                      */
#define INA229_ADC_MODE_CONT_VBUS_TEMP    (0xD << 12)    /* 连续: 总线+温度                   */
#define INA229_ADC_MODE_CONT_TEMP_VSHUNT  (0xE << 12)    /* 连续: 温度+分流                   */
#define INA229_ADC_MODE_CONT_ALL          (0xF << 12)    /* 连续: 全部通道                    */

/*---------- 转换时间字段 ----------*/
#define INA229_ADC_VBUSCT_Pos            9
#define INA229_ADC_VBUSCT_Msk            (0x7 << 9)
#define INA229_ADC_VSHCT_Pos             6
#define INA229_ADC_VSHCT_Msk             (0x7 << 6)
#define INA229_ADC_VTCT_Pos              3
#define INA229_ADC_VTCT_Msk              (0x7 << 3)
#define INA229_ADC_AVG_Msk               (0x7 << 0)

/* 转换时间选项 (更长=更高精度/更低噪声) */
typedef enum {
    INA229_CT_50US   = 0,   /* 50 μs                        */
    INA229_CT_84US   = 1,   /* 84 μs                        */
    INA229_CT_150US  = 2,   /* 150 μs                       */
    INA229_CT_280US  = 3,   /* 280 μs                       */
    INA229_CT_540US  = 4,   /* 540 μs                       */
    INA229_CT_1052US = 5,   /* 1052 μs (默认)               */
    INA229_CT_2074US = 6,   /* 2074 μs                      */
    INA229_CT_4120US = 7    /* 4120 μs (最慢, 噪声最小)     */
} INA229_ConvTime_t;

/* 平均值采样次数 (更大=更低噪声, 总时间=单次×次数) */
typedef enum {
    INA229_AVG_1    = 0,    /* 1 次                         */
    INA229_AVG_4    = 1,    /* 4 次                         */
    INA229_AVG_16   = 2,    /* 16 次 (默认)                 */
    INA229_AVG_64   = 3,    /* 64 次                        */
    INA229_AVG_128  = 4,    /* 128 次                       */
    INA229_AVG_256  = 5,    /* 256 次                       */
    INA229_AVG_512  = 6,    /* 512 次                       */
    INA229_AVG_1024 = 7     /* 1024 次 (最低噪声)           */
} INA229_AvgCount_t;

/*===========================================================================
 * DIAG_ALRT 寄存器 (0x0B) 位定义
 *
 *   BIT[15]  = ALATCH (报警锁存使能)
 *   BIT[14]  = CNVR   (转换就绪)
 *   BIT[13]  = SLOWALRT / POLARITY
 *   BIT[12]  = BUSBK  / BUSUL
 *   BIT[11]  = SHNTBK / SHNTUL
 *   BIT[10]  = TMPOL  / TMPOL
 *   BIT[9:5] = RESERVED
 *   BIT[4]   = MATHOF (算术溢出)
 *   BIT[3]   = RESERVED
 *   BIT[2]   = TMPOL  (过温报警)
 *   BIT[1]   = SHNTOL (分流过压报警)
 *   BIT[0]   = BUSUL  (总线欠压报警)
 *===========================================================================*/

#define INA229_DIAG_ALRT_ALATCH       (1 << 15)
#define INA229_DIAG_ALRT_CNVR         (1 << 14)
#define INA229_DIAG_ALRT_SLOWALRT     (1 << 13)
#define INA229_DIAG_ALRT_BUSBK        (1 << 12)
#define INA229_DIAG_ALRT_SHNTBK       (1 << 11)
#define INA229_DIAG_ALRT_MATHOF       (1 << 4)
#define INA229_DIAG_ALRT_TMPOL        (1 << 2)
#define INA229_DIAG_ALRT_SHNTOL       (1 << 1)
#define INA229_DIAG_ALRT_BUSUL        (1 << 0)

/*===========================================================================
 * 数据转换常量和公式
 *
 *   实际值 = 寄存器原始值 × LSB
 *
 *   VSHUNT/CURRENT/POWER: 二进制补码 (有符号)
 *   VBUS/DIETEMP/ENERGY:  无符号
 *===========================================================================*/

#define INA229_VSHUNT_LSB_ADCRANGE0    312.5e-9        /* ADCRANGE=0: ±163.84mV 量程 */
#define INA229_VSHUNT_LSB_ADCRANGE1    78.125e-9       /* ADCRANGE=1: ±40.96mV 量程  */
#define INA229_VBUS_LSB                195.3125e-6     /* 总线电压, 固定              */
#define INA229_TEMP_LSB                7.8125e-3       /* 芯片温度, m°C               */
#define INA229_SOVL_LSB_ADCRANGE0      5.0e-6          /* 分流过压阈值 LSB (range0)   */
#define INA229_SOVL_LSB_ADCRANGE1      1.25e-6         /* 分流过压阈值 LSB (range1)   */
#define INA229_BOVL_LSB                3.125e-3        /* 总线过压阈值 LSB            */

/*===========================================================================
 * SHUNT_CAL 校准计算
 *
 *   公式: SHUNT_CAL = 13107200000 × Rshunt(Ω) × Imax(A)
 *   上限: 0x7FFF (15-bit 无符号)
 *   若超限则除以 4 重新缩放并钳位到 32767
 *
 *   示例: Rshunt=1mΩ, Imax=10A → 13107200000×0.001×10 = 131072000
 *         > 32767 → 除4 → 32768000 → 钳位 32767
 *===========================================================================*/

#define INA229_SHUNT_CAL_MAX           0x7FFF

/*===========================================================================
 * 测量数据结构
 *===========================================================================*/

typedef struct {
    float     vshunt;       /* 分流电压     (V)              */
    float     vbus;         /* 总线电压     (V)              */
    float     temperature;  /* 芯片温度     (°C)             */
    float     current;      /* 电流         (A)              */
    float     power;        /* 功率         (W)              */
    uint64_t  energy;       /* 电能原始值   (LSB, 无符号)   */
    int64_t   charge;       /* 电荷原始值   (LSB, 补码)     */
} INA229_Data_t;

/*===========================================================================
 * API: 初始化与系统控制
 *===========================================================================*/

bool     INA229_Init(float shunt_resistance_ohm, float max_expected_current_a);
void     INA229_Reset(void);
void     INA229_ClearAccumulators(void);

/*===========================================================================
 * API: 底层寄存器读写
 *===========================================================================*/

uint16_t INA229_ReadReg16(uint8_t addr);
uint32_t INA229_ReadReg24(uint8_t addr);
uint64_t INA229_ReadReg40(uint8_t addr);
void     INA229_WriteReg16(uint8_t addr, uint16_t data);

/*===========================================================================
 * API: 测量数据读取
 *===========================================================================*/

float     INA229_ReadVShunt(uint8_t adc_range);
float     INA229_ReadVBus(void);
float     INA229_ReadTemperature(void);
float     INA229_ReadCurrent(uint16_t shunt_cal_value);
float     INA229_ReadPower(uint16_t shunt_cal_value);
void      INA229_ReadAll(INA229_Data_t *data, uint8_t adc_range, uint16_t shunt_cal);
uint64_t  INA229_ReadEnergy(void);
int64_t   INA229_ReadCharge(void);
bool      INA229_IsConversionReady(void);
uint16_t  INA229_ReadDiagAlrt(void);

/*===========================================================================
 * API: 配置
 *===========================================================================*/

void INA229_SetADCRange(uint8_t range);
void INA229_SetMode(uint16_t mode);
void INA229_SetConversionTime(INA229_ConvTime_t vbus_ct, INA229_ConvTime_t vshunt_ct, INA229_ConvTime_t temp_ct);
void INA229_SetAveraging(INA229_AvgCount_t avg);
void INA229_SetShuntCal(uint16_t shunt_cal);
void INA229_EnableTempComp(uint16_t tempco_ppm);

/*===========================================================================
 * API: 报警阈值
 *===========================================================================*/

void INA229_SetShuntOverVoltage(uint16_t threshold);
void INA229_SetShuntUnderVoltage(uint16_t threshold);
void INA229_SetBusOverVoltage(uint16_t threshold);
void INA229_SetBusUnderVoltage(uint16_t threshold);
void INA229_SetTempLimit(uint16_t threshold);
void INA229_SetPowerLimit(uint16_t threshold);

/*===========================================================================
 * API: 器件识别
 *===========================================================================*/

uint16_t INA229_ReadManufacturerID(void);
uint16_t INA229_ReadDeviceID(void);

#ifdef __cplusplus
}
#endif

#endif /* __INA229_H__ */

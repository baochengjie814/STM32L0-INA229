/**
 * @file    ina229.h
 * @brief   INA229 85V/20位 超精密功率监控器 驱动 (SPI 接口)
 *
 * @author  (你的名字)
 * @date    (日期)
 *
 * @note    依赖 soft_spi.h, 使用软件 SPI 与 INA229 通信
 *
 *   SPI 帧结构 (读取):
 *     MOSI: | ADDR[5:0] | 0 | R/W=1 | ... 填充数据 ...              |
 *     MISO: | 8-bit 状态字节 | DATA[(N-1):0]                         |
 *
 *   SPI 帧结构 (写入):
 *     MOSI: | ADDR[5:0] | 0 | R/W=0 | DATA[15:0]                     |
 *     MISO: | 8-bit 状态字节 | OLD_DATA[15:0]                        |
 *
 *   MISO 首个字节(状态字节)位定义:
 *     BIT[7]  = ALRT    (报警标志, 任一报警置位时有效)
 *     BIT[6]  = CRDY    (转换就绪, 1=所有使能的通道转换完成)
 *     BIT[5]  = MATHOF  (算术溢出)
 *     BIT[4]  = RESERVED
 *     BIT[3]  = MEMSTAT (存储器状态, 1=校验和错误)
 *     BIT[2:0] = RESERVED
 *
 *   寄存器宽度:
 *     16-bit: CONFIG, ADC_CONFIG, SHUNT_CAL, SHUNT_TEMPCO,
 *             DIETEMP, DIAG_ALRT, SOVL, SUVL, BOVL, BUVL,
 *             TEMP_LIMIT, PWR_LIMIT, MANUFACTURER_ID, DEVICE_ID
 *     24-bit: VSHUNT, VBUS, CURRENT, POWER
 *            (注意: 24-bit 寄存器通过 3 字节 SPI 读取,
 *                  实际有效数据为高 20-bit, 低 4-bit 始终为 0)
 *     40-bit: ENERGY, CHARGE
 *            (注意: 40-bit 寄存器通过 5 字节 SPI 读取,
 *                  实际有效数据为高 40-bit)
 *
 *   参考文档: INA229 数据手册 (SBOS938A)
 */

#ifndef __INA229_H__
#define __INA229_H__

#ifdef __cplusplus
extern "C" {
#endif

/*---------------------------------------------------------------------------
 * 包含
 *---------------------------------------------------------------------------*/
#include <stdint.h>
#include <stdbool.h>

/*===========================================================================
 * INA229 寄存器地址
 *===========================================================================*/
#define INA229_REG_CONFIG            0x00   /**< 配置寄存器                (R/W, 16-bit) */
#define INA229_REG_ADC_CONFIG        0x01   /**< ADC 配置寄存器            (R/W, 16-bit) */
#define INA229_REG_SHUNT_CAL         0x02   /**< 分流校准寄存器            (R/W, 16-bit) */
#define INA229_REG_SHUNT_TEMPCO      0x03   /**< 分流温度系数寄存器        (R/W, 16-bit) */
#define INA229_REG_VSHUNT            0x04   /**< 分流电压测量值            (R,   24-bit) */
#define INA229_REG_VBUS              0x05   /**< 总线电压测量值            (R,   24-bit) */
#define INA229_REG_DIETEMP           0x06   /**< 芯片温度测量值            (R,   16-bit) */
#define INA229_REG_CURRENT           0x07   /**< 电流计算结果              (R,   24-bit) */
#define INA229_REG_POWER             0x08   /**< 功率计算结果              (R,   24-bit) */
#define INA229_REG_ENERGY            0x09   /**< 电能累积结果              (R,   40-bit) */
#define INA229_REG_CHARGE            0x0A   /**< 电荷累积结果              (R,   40-bit) */
#define INA229_REG_DIAG_ALRT         0x0B   /**< 诊断标志与报警             (R/W, 16-bit) */
#define INA229_REG_SOVL              0x0C   /**< 分流过压阈值              (R/W, 16-bit) */
#define INA229_REG_SUVL              0x0D   /**< 分流欠压阈值              (R/W, 16-bit) */
#define INA229_REG_BOVL              0x0E   /**< 总线过压阈值              (R/W, 16-bit) */
#define INA229_REG_BUVL              0x0F   /**< 总线欠压阈值              (R/W, 16-bit) */
#define INA229_REG_TEMP_LIMIT        0x10   /**< 过温阈值                  (R/W, 16-bit) */
#define INA229_REG_PWR_LIMIT         0x11   /**< 过功率阈值                (R/W, 16-bit) */
#define INA229_REG_MANUFACTURER_ID   0x3E   /**< 制造商 ID (TI = 0x5449)   (R,   16-bit) */
#define INA229_REG_DEVICE_ID         0x3F   /**< 器件 ID   (INA229 = 0x2291) (R, 16-bit) */

/*===========================================================================
 * CONFIG 寄存器 (地址 0x00) 位定义
 *
 *   BIT[15]    = RST      (复位, 写1自动清除)
 *   BIT[14]    = RSTACC   (清除累加器, 写1自动清除)
 *   BIT[13:6]  = CONVDLY  (转换延迟, 0~255, 步长 2ms)
 *   BIT[5]     = TEMPCOMP (分流温度补偿使能)
 *   BIT[4]     = ADCRANGE (0=±163.84mV, 1=±40.96mV)
 *   BIT[3:0]   = RESERVED
 *===========================================================================*/
#define INA229_CONFIG_RST            (1 << 15)  /**< 系统复位 (写1, 自清除)              */
#define INA229_CONFIG_RSTACC         (1 << 14)  /**< 清除 ENERGY/CHARGE 累加器 (自清除) */
#define INA229_CONFIG_CONVDLY_Pos    6          /**< 转换延迟字段起始位                   */
#define INA229_CONFIG_CONVDLY_Msk    (0xFF << 6)/**< 转换延迟字段掩码, 步长 2ms           */
#define INA229_CONFIG_TEMPCOMP       (1 << 5)   /**< 启用分流温度补偿                     */
#define INA229_CONFIG_ADCRANGE       (1 << 4)   /**< ADC 量程: 0=±163.84mV, 1=±40.96mV  */

/*===========================================================================
 * ADC_CONFIG 寄存器 (地址 0x01) 位定义
 *
 *   BIT[15:12] = MODE    (工作模式)
 *   BIT[11:9]  = VBUSCT  (总线电压转换时间)
 *   BIT[8:6]   = VSHCT   (分流电压转换时间)
 *   BIT[5:3]   = VTCT    (温度转换时间)
 *   BIT[2:0]   = AVG     (平均值采样次数)
 *===========================================================================*/

/*---------- ADC 工作模式 (MODE[15:12]) ----------*/
#define INA229_ADC_MODE_Msk          (0xF << 12)
#define INA229_ADC_MODE_SHUTDOWN          (0x0 << 12)  /**< 关断 (最低功耗)                      */
#define INA229_ADC_MODE_TRIG_VBUS         (0x1 << 12)  /**< 触发: 仅总线电压                      */
#define INA229_ADC_MODE_TRIG_VSHUNT       (0x2 << 12)  /**< 触发: 仅分流电压                      */
#define INA229_ADC_MODE_TRIG_VBUS_VSHUNT  (0x3 << 12)  /**< 触发: 总线电压 + 分流电压             */
#define INA229_ADC_MODE_TRIG_TEMP         (0x4 << 12)  /**< 触发: 仅温度                          */
#define INA229_ADC_MODE_TRIG_TEMP_VBUS    (0x5 << 12)  /**< 触发: 温度 + 总线电压                 */
#define INA229_ADC_MODE_TRIG_TEMP_VSHUNT  (0x6 << 12)  /**< 触发: 温度 + 分流电压                 */
#define INA229_ADC_MODE_TRIG_ALL          (0x7 << 12)  /**< 触发: 全部通道                        */
#define INA229_ADC_MODE_CONT_VBUS         (0x9 << 12)  /**< 连续: 仅总线电压                      */
#define INA229_ADC_MODE_CONT_VSHUNT       (0xA << 12)  /**< 连续: 仅分流电压                      */
#define INA229_ADC_MODE_CONT_VBUS_VSHUNT  (0xB << 12)  /**< 连续: 总线电压 + 分流电压             */
#define INA229_ADC_MODE_CONT_TEMP         (0xC << 12)  /**< 连续: 仅温度                          */
#define INA229_ADC_MODE_CONT_VBUS_TEMP    (0xD << 12)  /**< 连续: 总线电压 + 温度                 */
#define INA229_ADC_MODE_CONT_TEMP_VSHUNT  (0xE << 12)  /**< 连续: 温度 + 分流电压                 */
#define INA229_ADC_MODE_CONT_ALL          (0xF << 12)  /**< 连续: 全部通道                        */

/*---------- 转换时间字段 ----------*/
#define INA229_ADC_VBUSCT_Pos        9           /**< 总线电压转换时间字段位偏移              */
#define INA229_ADC_VBUSCT_Msk        (0x7 << 9)  /**< 总线电压转换时间字段掩码                */
#define INA229_ADC_VSHCT_Pos         6           /**< 分流电压转换时间字段位偏移              */
#define INA229_ADC_VSHCT_Msk         (0x7 << 6)  /**< 分流电压转换时间字段掩码                */
#define INA229_ADC_VTCT_Pos          3           /**< 温度转换时间字段位偏移                  */
#define INA229_ADC_VTCT_Msk          (0x7 << 3)  /**< 温度转换时间字段掩码                    */
#define INA229_ADC_AVG_Msk           (0x7 << 0)  /**< 平均值采样次数字段掩码                  */

/**
 * @brief  ADC 转换时间选项 (VBUSCT / VSHCT / VTCT 通用)
 * @note   更长转换时间 = 更高精度, 更低的噪声
 */
typedef enum {
    INA229_CT_50US   = 0,   /**< 50 μs   (最快, 噪声最大)   */
    INA229_CT_84US   = 1,   /**< 84 μs                       */
    INA229_CT_150US  = 2,   /**< 150 μs                      */
    INA229_CT_280US  = 3,   /**< 280 μs                      */
    INA229_CT_540US  = 4,   /**< 540 μs                      */
    INA229_CT_1052US = 5,   /**< 1052 μs (默认值)            */
    INA229_CT_2074US = 6,   /**< 2074 μs                     */
    INA229_CT_4120US = 7    /**< 4120 μs (最慢, 噪声最小)   */
} INA229_ConvTime_t;

/**
 * @brief  平均值采样次数
 * @note   更大采样数 = 更低噪声, 但转换总时间 = 单次转换时间 × 采样次数
 */
typedef enum {
    INA229_AVG_1    = 0,   /**< 1 次   (无平均)               */
    INA229_AVG_4    = 1,   /**< 4 次                          */
    INA229_AVG_16   = 2,   /**< 16 次  (默认值)               */
    INA229_AVG_64   = 3,   /**< 64 次                         */
    INA229_AVG_128  = 4,   /**< 128 次                        */
    INA229_AVG_256  = 5,   /**< 256 次                        */
    INA229_AVG_512  = 6,   /**< 512 次                        */
    INA229_AVG_1024 = 7    /**< 1024 次 (最低噪声)            */
} INA229_AvgCount_t;

/*===========================================================================
 * DIAG_ALRT 寄存器 (地址 0x0B) 位定义
 *
 *   BIT[15]    = ALATCH (报警锁存使能)
 *   BIT[14]    = CNVR   (转换就绪标志)
 *   BIT[13]    = SLOWALRT / POLARITY
 *   BIT[12]    = BUSBK  / BUSUL
 *   BIT[11]    = SHNTBK / SHNTUL
 *   BIT[10]    = TMPOL  / TMPOL
 *   BIT[9:5]   = RESERVED
 *   BIT[4]     = MATHOF (算术溢出标志)
 *   BIT[3]     = RESERVED
 *   BIT[2]     = TMPOL  (过温报警)
 *   BIT[1]     = SHNTOL (分流过压报警)
 *   BIT[0]     = BUSUL  (总线欠压报警)
 *===========================================================================*/
#define INA229_DIAG_ALRT_ALATCH       (1 << 15)  /**< 报警锁存使能 (1=锁存直到读取)  */
#define INA229_DIAG_ALRT_CNVR         (1 << 14)  /**< 转换就绪 (1=所有通道转换完成)  */
#define INA229_DIAG_ALRT_SLOWALRT     (1 << 13)  /**< 慢报警 / 极性标志              */
#define INA229_DIAG_ALRT_BUSBK        (1 << 12)  /**< 总线断线检测 / 总线欠压标志    */
#define INA229_DIAG_ALRT_SHNTBK       (1 << 11)  /**< 分流断线检测 / 分流欠压标志    */
#define INA229_DIAG_ALRT_MATHOF       (1 << 4)   /**< 算术溢出 (电流/功率计算溢出)   */
#define INA229_DIAG_ALRT_TMPOL        (1 << 2)   /**< 过温报警标志                   */
#define INA229_DIAG_ALRT_SHNTOL       (1 << 1)   /**< 分流过压报警标志               */
#define INA229_DIAG_ALRT_BUSUL        (1 << 0)   /**< 总线欠压报警标志               */

/*===========================================================================
 * 数据转换常量 (LSB 值)
 *
 *   实际值 = 寄存器原始值 × LSB
 *
 *   注意: VSHUNT/CURRENT/POWER 为二进制补码格式 (有符号)
 *         VBUS/DIETEMP/ENERGY 为无符号格式
 *===========================================================================*/

/** @brief 分流电压 LSB: ADCRANGE=0 (±163.84mV 量程) */
#define INA229_VSHUNT_LSB_ADCRANGE0    312.5e-9
/** @brief 分流电压 LSB: ADCRANGE=1 (±40.96mV 量程) */
#define INA229_VSHUNT_LSB_ADCRANGE1    78.125e-9
/** @brief 总线电压 LSB (3.125mV × 16 = 195.3125μV, 固定) */
#define INA229_VBUS_LSB                195.3125e-6
/** @brief 芯片温度 LSB (7.8125 m°C, 二进制补码) */
#define INA229_TEMP_LSB                7.8125e-3
/** @brief 分流过压阈值 LSB: ADCRANGE=0 (5μV) */
#define INA229_SOVL_LSB_ADCRANGE0      5.0e-6
/** @brief 分流过压阈值 LSB: ADCRANGE=1 (1.25μV) */
#define INA229_SOVL_LSB_ADCRANGE1      1.25e-6
/** @brief 总线过压阈值 LSB (3.125mV) */
#define INA229_BOVL_LSB                3.125e-3

/*===========================================================================
 * SHUNT_CAL 计算相关常量
 *===========================================================================*/

/** @brief SHUNT_CAL 寄存器最大值 (15-bit 无符号) */
#define INA229_SHUNT_CAL_MAX           0x7FFF

/*===========================================================================
 * 公共数据结构
 *===========================================================================*/

/**
 * @brief  INA229 测量数据汇总
 * @note   energy/charge 为原始 LSB 值, 需根据 SHUNT_CAL 换算为焦耳/库仑
 */
typedef struct {
    float     vshunt;       /**< 分流电压     (V)              */
    float     vbus;         /**< 总线电压     (V)              */
    float     temperature;  /**< 芯片温度     (°C)             */
    float     current;      /**< 电流         (A)              */
    float     power;        /**< 功率         (W)              */
    uint64_t  energy;       /**< 电能原始值   (LSB, 无符号)   */
    int64_t   charge;       /**< 电荷原始值   (LSB, 二进制补码) */
} INA229_Data_t;

/*===========================================================================
 * 公共 API 函数声明
 *===========================================================================*/

/*---------- 初始化与系统 ----------*/

/**
 * @brief  初始化 INA229 驱动
 * @note   按顺序执行: 软件 SPI 引脚初始化 → 复位 → 器件 ID 检查 → 默认 ADC 配置 → SHUNT_CAL 计算与写入
 * @param  shunt_resistance_ohm  分流电阻阻值 (Ω)
 * @param  max_expected_current_a 最大预期电流  (A)
 * @retval true   初始化成功, 器件响应正常
 * @retval false  器件无响应 (ID 不匹配或 SPI 通信失败)
 */
bool INA229_Init(float shunt_resistance_ohm, float max_expected_current_a);

/**
 * @brief  软件复位 INA229 (写 CONFIG[RST] 位)
 * @note   复位后将延迟 1ms 等待器件重新初始化
 */
void INA229_Reset(void);

/**
 * @brief  清除电能/电荷累加器 (写 CONFIG[RSTACC] 位)
 * @note   RSTACC 位为自清除, 写后无需手动清除
 */
void INA229_ClearAccumulators(void);

/*---------- 底层寄存器读写 ----------*/

/**
 * @brief  读取 16-bit 寄存器
 * @param  addr 寄存器地址 (0x00~0x3F)
 * @return 16-bit 寄存器值
 */
uint16_t INA229_ReadReg16(uint8_t addr);

/**
 * @brief  读取 24-bit 寄存器 (如 VSHUNT, VBUS, CURRENT, POWER)
 * @param  addr 寄存器地址
 * @return 24-bit 原始值 (高 24-bit 有效, 低 8-bit 为 0)
 * @note   实际返回 32-bit, 但仅低 24-bit 有效
 */
uint32_t INA229_ReadReg24(uint8_t addr);

/**
 * @brief  读取 40-bit 寄存器 (如 ENERGY, CHARGE)
 * @param  addr 寄存器地址
 * @return 40-bit 原始值 (存放在 uint64_t 低 40-bit)
 */
uint64_t INA229_ReadReg40(uint8_t addr);

/**
 * @brief  写入 16-bit 寄存器
 * @param  addr 寄存器地址
 * @param  data 16-bit 数据
 */
void     INA229_WriteReg16(uint8_t addr, uint16_t data);

/*---------- 测量数据读取 ----------*/

/**
 * @brief  读取分流电压 (VSHUNT)
 * @param  adc_range ADCRANGE 位值 (0=±163.84mV, 1=±40.96mV)
 * @return 分流电压 (V), 有符号值
 * @note   内部对 20-bit 数据进行符号扩展到 24-bit, 再右移 4-bit
 */
float INA229_ReadVShunt(uint8_t adc_range);

/**
 * @brief  读取总线电压 (VBUS)
 * @return 总线电压 (V), 无符号值 (0~85V)
 */
float INA229_ReadVBus(void);

/**
 * @brief  读取芯片温度 (DIETEMP)
 * @return 芯片内部温度 (°C), 有符号值
 */
float INA229_ReadTemperature(void);

/**
 * @brief  读取电流 (CURRENT)
 * @param  shunt_cal_value SHUNT_CAL 寄存器值 (当前实现中暂未使用, 预留用于以后校准)
 * @return 电流原始值 (需外部根据 SHUNT_CAL 公式转换为 A)
 * @note   CURRENT 寄存器值 = 电流(A) × SHUNT_CAL / (4 × 分流电阻)
 *         当前函数返回原始有符号 20-bit 值, 由调用者自行换算
 */
float INA229_ReadCurrent(uint16_t shunt_cal_value);

/**
 * @brief  读取功率 (POWER)
 * @param  shunt_cal_value SHUNT_CAL 寄存器值 (当前实现中暂未使用, 预留用于以后校准)
 * @return 功率原始值 (需外部根据 SHUNT_CAL 公式转换为 W)
 * @note   POWER 寄存器值 = 功率(W) × 32 × SHUNT_CAL
 *         当前函数返回原始 24-bit 值, 由调用者自行换算
 */
float INA229_ReadPower(uint16_t shunt_cal_value);

/**
 * @brief  一次性读取所有主要测量数据
 * @param  data      数据输出结构体指针 (不可为 NULL)
 * @param  adc_range ADCRANGE 位值 (传给 INA229_ReadVShunt)
 * @param  shunt_cal SHUNT_CAL 寄存器值 (传给 INA229_ReadCurrent / INA229_ReadPower)
 * @note   内部依次读取 7 个寄存器, 读取期间数据可能略有时间差
 */
void INA229_ReadAll(INA229_Data_t *data, uint8_t adc_range, uint16_t shunt_cal);

/**
 * @brief  读取电能累积值 (ENERGY)
 * @return 40-bit 电能原始值 (LSB, 无符号)
 * @note   电能 LSB = 3.2 × SHUNT_CAL (μJ)
 */
uint64_t INA229_ReadEnergy(void);

/**
 * @brief  读取电荷累积值 (CHARGE)
 * @return 40-bit 电荷原始值 (LSB, 二进制补码)
 * @note   电荷 LSB = SHUNT_CAL (μC)
 */
int64_t INA229_ReadCharge(void);

/**
 * @brief  检查 ADC 转换是否完成
 * @retval true  转换完成, 数据可读
 * @retval false 转换进行中
 * @note   通过读取 DIAG_ALRT[CNVR] 位判断
 */
bool INA229_IsConversionReady(void);

/**
 * @brief  读取诊断/报警寄存器
 * @return DIAG_ALRT 寄存器原始值 (参考 @ref INA229_DIAG_ALRT_xxx 位定义)
 */
uint16_t INA229_ReadDiagAlrt(void);

/*---------- 配置 API ----------*/

/**
 * @brief  设置 ADC 量程
 * @param  range 0=±163.84mV, 1=±40.96mV (其他值视为 1)
 * @note   读-修改-写 CONFIG 寄存器
 */
void INA229_SetADCRange(uint8_t range);

/**
 * @brief  设置 ADC 工作模式
 * @param  mode 模式值 (使用 INA229_ADC_MODE_CONT_xxx 或 INA229_ADC_MODE_TRIG_xxx)
 * @note   读-修改-写 ADC_CONFIG 寄存器
 */
void INA229_SetMode(uint16_t mode);

/**
 * @brief  设置各通道 ADC 转换时间
 * @param  vbus_ct  总线电压转换时间
 * @param  vshunt_ct 分流电压转换时间
 * @param  temp_ct  温度转换时间
 * @note   读-修改-写 ADC_CONFIG 寄存器
 */
void INA229_SetConversionTime(INA229_ConvTime_t vbus_ct,
                               INA229_ConvTime_t vshunt_ct,
                               INA229_ConvTime_t temp_ct);

/**
 * @brief  设置平均值采样次数
 * @param  avg 采样次数 (1/4/16/64/128/256/512/1024)
 * @note   读-修改-写 ADC_CONFIG 寄存器
 */
void INA229_SetAveraging(INA229_AvgCount_t avg);

/**
 * @brief  设置分流校准值
 * @param  shunt_cal SHUNT_CAL 寄存器值 (0x0000~0x7FFF)
 * @note   该值由分流电阻和最大预期电流计算得出
 */
void INA229_SetShuntCal(uint16_t shunt_cal);

/**
 * @brief  启用/禁用分流温度补偿
 * @param  tempco_ppm 分流电阻温度系数 (ppm/°C), 0 表示禁用
 * @note   非零值时同时写入 SHUNT_TEMPCO 寄存器并置位 TEMPCOMP 位
 *         写入前先清除原有 TEMPCOMP 配置
 */
void INA229_EnableTempComp(uint16_t tempco_ppm);

/*---------- 报警阈值 ----------*/

/**
 * @brief  设置分流过压阈值  (SOVL)
 * @param  threshold 16-bit 阈值 (LSB = 5μV 或 1.25μV, 取决于 ADCRANGE)
 */
void INA229_SetShuntOverVoltage(uint16_t threshold);

/**
 * @brief  设置分流欠压阈值  (SUVL)
 * @param  threshold 16-bit 阈值
 */
void INA229_SetShuntUnderVoltage(uint16_t threshold);

/**
 * @brief  设置总线过压阈值  (BOVL)
 * @param  threshold 15-bit 阈值 (LSB = 3.125mV, 仅低 15-bit 有效)
 */
void INA229_SetBusOverVoltage(uint16_t threshold);

/**
 * @brief  设置总线欠压阈值  (BUVL)
 * @param  threshold 15-bit 阈值 (LSB = 3.125mV, 仅低 15-bit 有效)
 */
void INA229_SetBusUnderVoltage(uint16_t threshold);

/**
 * @brief  设置过温阈值  (TEMP_LIMIT)
 * @param  threshold 16-bit 阈值 (LSB = 7.8125 m°C, 二进制补码)
 */
void INA229_SetTempLimit(uint16_t threshold);

/**
 * @brief  设置过功率阈值 (PWR_LIMIT)
 * @param  threshold 16-bit 阈值
 */
void INA229_SetPowerLimit(uint16_t threshold);

/*---------- 器件识别 ----------*/

/**
 * @brief  读取制造商 ID
 * @return 制造商 ID (TI = 0x5449 = 'TI' ASCII)
 */
uint16_t INA229_ReadManufacturerID(void);

/**
 * @brief  读取器件 ID
 * @return 器件 ID (INA229 = 0x2291)
 */
uint16_t INA229_ReadDeviceID(void);

#ifdef __cplusplus
}
#endif

#endif /* __INA229_H__ */

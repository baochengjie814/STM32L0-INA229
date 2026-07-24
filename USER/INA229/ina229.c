/**
  ******************************************************************************
  * 文件名程: ina229.c
  * 作    者:
  * 版    本: V1.0
  * 编写日期:
  * 功    能: INA229 功率监控器 SPI 驱动实现
  *
  * 依赖: ina229.h, soft_spi.h, system.h, string.h
  *
  * 通信格式:
  *   每次传输 CS 拉低开始, CS 拉高结束.
  *   MOSI 首字节 = 命令: [ADDR5..ADDR0 | 0 | R/W]
  *   MISO 首字节 = 状态 (ALRT/CRDY/MATHOF/MEMSTAT)
  *
  * 寄存器数据宽度:
  *   16-bit: 3 字节 (1 命令 + 2 数据)
  *   24-bit: 4 字节 (1 命令 + 3 数据), 有效数据低 20-bit
  *   40-bit: 6 字节 (1 命令 + 5 数据), 有效数据低 40-bit
  ******************************************************************************
  */

/* 头文件 ---------------------------------------------------------------*/
#include "ina229.h"
#include "soft_spi.h"
#include <system.h>
#include <string.h>

/*===========================================================================
 * 内部常量
 *===========================================================================*/

#define INA229_CMD_READ                 0x01    /* 命令字节 R/W 位: 读=1 */
#define INA229_CMD_WRITE                0x00    /* 命令字节 R/W 位: 写=0 */

#define INA229_SHUNT_CAL_FACTOR         13107200000.0f  /* SHUNT_CAL 计算常数 */
#define INA229_SHUNT_CAL_DIVISOR        4.0f            /* 超限时缩放因子     */
#define INA229_SHUNT_CAL_MAX_FLOAT      32767.0f         /* 15-bit 最大浮点值   */

#define INA229_24BIT_DATA_SHIFT         4               /* 24-bit 中低 4-bit 始终为 0, 右移去掉 */

#define INA229_CHARGE_SIGN_BIT          0x8000000000ULL         /* 40-bit 符号位 (bit39) */
#define INA229_CHARGE_SIGN_EXTEND       0xFFFFFF0000000000ULL   /* 符号扩展到 int64 高 24-bit */



/*===========================================================================
 * 内部辅助函数
 *===========================================================================*/

/**
 * 构建 SPI 命令字节
 * cmd = (addr << 2) | rw
 * BIT[7:2] = ADDR[5:0],  BIT[1] = 0 (保留),  BIT[0] = R/W
 */
static inline uint8_t _BuildCmd(uint8_t addr, uint8_t rw)
{
    return (uint8_t)(((addr & 0x3F) << 2) | (rw & 0x01));
}

/**
 * 24-bit 符号扩展为 32-bit (VSHUNT/CURRENT 等有符号寄存器)
 * 例: raw24=0x800000 (bit23=1) → 0xFF800000 (int32_t = -8388608)
 */
static inline int32_t _SignExtend24(uint32_t raw24)
{
    if (raw24 & 0x800000UL)
        return (int32_t)(raw24 | 0xFF000000UL);
    return (int32_t)raw24;
}

/*===========================================================================
 * 初始化与系统控制
 *===========================================================================*/

/**
 * INA229 初始化流程:
 *   1. 初始化软件 SPI 引脚
 *   2. 发送软件复位
 *   3. 读取器件 ID (期望 0x2291)
 *   4. 默认 ADC 配置: 连续模式, 全部通道, 1052μs, 16 次平均
 *   5. 计算并写入 SHUNT_CAL
 *
 * 返回: true=成功, false=ID 不匹配或 SPI 通信失败
 */
bool INA229_Init(float rshunt_ohm, float i_max_a)
{
    uint16_t dev_id;
    float    scal;

    /* 1. 软件 SPI 引脚初始化 */
    Soft_SPI_Init();

    /* 2. 软件复位 */
    INA229_Reset();

    /* 3. 读器件 ID 验证通信 */
    dev_id = INA229_ReadDeviceID();
    if (dev_id != 0x2291)
        return false;

    /* 4. 默认 ADC 配置 */
    INA229_WriteReg16(INA229_REG_ADC_CONFIG,
        INA229_ADC_MODE_CONT_ALL |
        (INA229_CT_1052US << INA229_ADC_VBUSCT_Pos) |
        (INA229_CT_1052US << INA229_ADC_VSHCT_Pos) |
        (INA229_CT_1052US << INA229_ADC_VTCT_Pos) |
        INA229_AVG_16);

    /* 5. 计算并写入 SHUNT_CAL
     *    SHUNT_CAL = 13107200000 × Rshunt × Imax
     *    若 > 32767 → 除以 4 → 钳位到 32767
     */
    scal = INA229_SHUNT_CAL_FACTOR * rshunt_ohm * i_max_a;
    if (scal > INA229_SHUNT_CAL_MAX_FLOAT) scal /= INA229_SHUNT_CAL_DIVISOR;
    if (scal > INA229_SHUNT_CAL_MAX_FLOAT) scal = INA229_SHUNT_CAL_MAX_FLOAT;
    INA229_SetShuntCal((uint16_t)(scal + 0.5f));

    return true;
}

/* 软件复位: 写 CONFIG[RST]=1, 等待 1ms */
void INA229_Reset(void)
{
    INA229_WriteReg16(INA229_REG_CONFIG, INA229_CONFIG_RST);
    Soft_SPI_DelayUs(1000);
}

/* 清除 ENERGY/CHARGE 累加器 */
void INA229_ClearAccumulators(void)
{
    INA229_WriteReg16(INA229_REG_CONFIG, INA229_CONFIG_RSTACC);
}

/*===========================================================================
 * 底层寄存器读写
 *===========================================================================*/

/**
 * 读取 16-bit 寄存器
 * SPI 帧: | CMD(1B) | D[15:8](1B) | D[7:0](1B) |
 * 接收: rx[0]=状态(忽略), rx[1]=D15..8, rx[2]=D7..0
 */
uint16_t INA229_ReadReg16(uint8_t addr)
{
    uint8_t tx[3], rx[3];

    tx[0] = _BuildCmd(addr, INA229_CMD_READ);
    tx[1] = tx[2] = 0x00;

    Soft_SPI_CS_Low();
    Soft_SPI_TransferBuf(tx, rx, 3);
    Soft_SPI_CS_High();

    return ((uint16_t)rx[1] << 8) | rx[2];
}

/**
 * 读取 24-bit 寄存器 (VSHUNT, VBUS, CURRENT, POWER)
 * SPI 帧: | CMD(1B) | D[23:16](1B) | D[15:8](1B) | D[7:0](1B) |
 * 有效数据 20-bit, 左对齐, 低 4-bit=0
 */
uint32_t INA229_ReadReg24(uint8_t addr)
{
    uint8_t tx[4], rx[4];

    tx[0] = _BuildCmd(addr, INA229_CMD_READ);
    tx[1] = tx[2] = tx[3] = 0x00;

    Soft_SPI_CS_Low();
    Soft_SPI_TransferBuf(tx, rx, 4);
    Soft_SPI_CS_High();

    return ((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | rx[3];
}

/**
 * 读取 40-bit 寄存器 (ENERGY, CHARGE)
 * SPI 帧: | CMD(1B) | D[39:32](1B) | ... | D[7:0](1B) |
 * 有效数据 40-bit
 */
uint64_t INA229_ReadReg40(uint8_t addr)
{
    uint8_t tx[6], rx[6];

    tx[0] = _BuildCmd(addr, INA229_CMD_READ);
    tx[1] = tx[2] = tx[3] = tx[4] = tx[5] = 0x00;

    Soft_SPI_CS_Low();
    Soft_SPI_TransferBuf(tx, rx, 6);
    Soft_SPI_CS_High();

    return ((uint64_t)rx[1] << 32)
         | ((uint64_t)rx[2] << 24)
         | ((uint64_t)rx[3] << 16)
         | ((uint64_t)rx[4] << 8)
         |  (uint64_t)rx[5];
}

/**
 * 写入 16-bit 寄存器
 * SPI 帧: | CMD(1B) | D[15:8](1B) | D[7:0](1B) |
 */
void INA229_WriteReg16(uint8_t addr, uint16_t data)
{
    uint8_t tx[3];

    tx[0] = _BuildCmd(addr, INA229_CMD_WRITE);
    tx[1] = (uint8_t)(data >> 8);
    tx[2] = (uint8_t)(data & 0xFF);

    Soft_SPI_CS_Low();
    Soft_SPI_TransferBuf(tx, NULL, 3);
    Soft_SPI_CS_High();
}

/*===========================================================================
 * 测量值读取 (工程单位换算)
 *===========================================================================*/

/**
 * 读取分流电压 VSHUNT (V)
 * 流程: 读24bit → 符号扩展 → 右移4bit → ×LSB
 */
float INA229_ReadVShunt(uint8_t adc_range)
{
    int32_t val = _SignExtend24(INA229_ReadReg24(INA229_REG_VSHUNT)) >> INA229_24BIT_DATA_SHIFT;

    if (adc_range)
        return (float)val * INA229_VSHUNT_LSB_ADCRANGE1;
    else
        return (float)val * INA229_VSHUNT_LSB_ADCRANGE0;
}

/* 读取总线电压 VBUS (V), 无符号, 量程 0~85V */
float INA229_ReadVBus(void)
{
    uint32_t val = (INA229_ReadReg24(INA229_REG_VBUS) >> INA229_24BIT_DATA_SHIFT) & 0xFFFFFUL;
    return (float)val * INA229_VBUS_LSB;
}

/* 读取芯片温度 DIETEMP (°C), 16-bit 补码, LSB=7.8125m°C */
float INA229_ReadTemperature(void)
{
    return (float)((int16_t)INA229_ReadReg16(INA229_REG_DIETEMP)) * INA229_TEMP_LSB;
}

/**
 * 读取电流 CURRENT 原始值
 * TODO: 使用 shunt_cal_value 换算安培 → I(A) = val / shunt_cal_value
 * 当前返回原始 20-bit 值
 */
float INA229_ReadCurrent(uint16_t shunt_cal_value)
{
    int32_t val = _SignExtend24(INA229_ReadReg24(INA229_REG_CURRENT)) >> INA229_24BIT_DATA_SHIFT;
    (void)shunt_cal_value;
    return (float)val;
}

/**
 * 读取功率 POWER 原始值
 * TODO: 使用 shunt_cal_value 换算瓦特 → P(W) = val / (32 × shunt_cal_value)
 * 当前返回原始 24-bit 值
 */
float INA229_ReadPower(uint16_t shunt_cal_value)
{
    int32_t val = (int32_t)(INA229_ReadReg24(INA229_REG_POWER) & 0xFFFFFFUL);
    (void)shunt_cal_value;
    return (float)val;
}

/**
 * 一次性读取全部测量数据
 * 依次读 VSHUNT/VBUS/TEMP/CURRENT/POWER/ENERGY/CHARGE
 * 注意: 各通道采样时刻有微小时间差
 */
void INA229_ReadAll(INA229_Data_t *data, uint8_t adc_range, uint16_t shunt_cal)
{
    if (!data) return;

    memset(data, 0, sizeof(*data));

    data->vshunt      = INA229_ReadVShunt(adc_range);
    data->vbus        = INA229_ReadVBus();
    data->temperature = INA229_ReadTemperature();
    data->current     = INA229_ReadCurrent(shunt_cal);
    data->power       = INA229_ReadPower(shunt_cal);
    data->energy      = INA229_ReadEnergy();
    data->charge      = INA229_ReadCharge();
}

/* 读取电能 ENERGY (40-bit LSB, 无符号, LSB=3.2×SHUNT_CAL μJ) */
uint64_t INA229_ReadEnergy(void)
{
    return INA229_ReadReg40(INA229_REG_ENERGY);
}

/* 读取电荷 CHARGE (40-bit 补码, LSB=SHUNT_CAL μC) */
int64_t INA229_ReadCharge(void)
{
    uint64_t raw = INA229_ReadReg40(INA229_REG_CHARGE);

    /* 符号扩展: bit39=1 → 高 24-bit 填充 1 */
    if (raw & INA229_CHARGE_SIGN_BIT)
        return (int64_t)(raw | INA229_CHARGE_SIGN_EXTEND);
    return (int64_t)raw;
}

/* 检查 ADC 转换完成: 读 DIAG_ALRT[CNVR] (bit14) */
bool INA229_IsConversionReady(void)
{
    return (INA229_ReadReg16(INA229_REG_DIAG_ALRT) & INA229_DIAG_ALRT_CNVR) ? true : false;
}

/* 读取完整诊断/报警寄存器 */
uint16_t INA229_ReadDiagAlrt(void)
{
    return INA229_ReadReg16(INA229_REG_DIAG_ALRT);
}

/*===========================================================================
 * 配置 API (读-修改-写)
 *===========================================================================*/

void INA229_SetADCRange(uint8_t range)
{
    uint16_t cfg = INA229_ReadReg16(INA229_REG_CONFIG);
    cfg &= ~INA229_CONFIG_ADCRANGE;
    if (range) cfg |= INA229_CONFIG_ADCRANGE;
    INA229_WriteReg16(INA229_REG_CONFIG, cfg);
}

void INA229_SetMode(uint16_t mode)
{
    uint16_t cfg = INA229_ReadReg16(INA229_REG_ADC_CONFIG);
    cfg = (cfg & ~INA229_ADC_MODE_Msk) | (mode & INA229_ADC_MODE_Msk);
    INA229_WriteReg16(INA229_REG_ADC_CONFIG, cfg);
}

void INA229_SetConversionTime(INA229_ConvTime_t vct, INA229_ConvTime_t sct, INA229_ConvTime_t tct)
{
    uint16_t cfg = INA229_ReadReg16(INA229_REG_ADC_CONFIG);

    cfg &= ~(INA229_ADC_VBUSCT_Msk | INA229_ADC_VSHCT_Msk | INA229_ADC_VTCT_Msk);
    cfg |= ((uint16_t)vct << INA229_ADC_VBUSCT_Pos);
    cfg |= ((uint16_t)sct << INA229_ADC_VSHCT_Pos);
    cfg |= ((uint16_t)tct << INA229_ADC_VTCT_Pos);

    INA229_WriteReg16(INA229_REG_ADC_CONFIG, cfg);
}

void INA229_SetAveraging(INA229_AvgCount_t avg)
{
    uint16_t cfg = INA229_ReadReg16(INA229_REG_ADC_CONFIG);
    cfg = (cfg & ~INA229_ADC_AVG_Msk) | ((uint16_t)avg & 0x07);
    INA229_WriteReg16(INA229_REG_ADC_CONFIG, cfg);
}

void INA229_SetShuntCal(uint16_t v)
{
    INA229_WriteReg16(INA229_REG_SHUNT_CAL, v & INA229_SHUNT_CAL_MAX);
}

void INA229_EnableTempComp(uint16_t ppm)
{
    uint16_t cfg = INA229_ReadReg16(INA229_REG_CONFIG);

    if (ppm) {
        cfg |= INA229_CONFIG_TEMPCOMP;
        INA229_WriteReg16(INA229_REG_SHUNT_TEMPCO, ppm & 0x3FFF);
    } else {
        cfg &= ~INA229_CONFIG_TEMPCOMP;
    }
    INA229_WriteReg16(INA229_REG_CONFIG, cfg);
}

/*===========================================================================
 * 报警阈值
 *===========================================================================*/

void INA229_SetShuntOverVoltage(uint16_t t)  { INA229_WriteReg16(INA229_REG_SOVL, t); }
void INA229_SetShuntUnderVoltage(uint16_t t) { INA229_WriteReg16(INA229_REG_SUVL, t); }
void INA229_SetBusOverVoltage(uint16_t t)    { INA229_WriteReg16(INA229_REG_BOVL, t & 0x7FFF); }
void INA229_SetBusUnderVoltage(uint16_t t)   { INA229_WriteReg16(INA229_REG_BUVL, t & 0x7FFF); }
void INA229_SetTempLimit(uint16_t t)         { INA229_WriteReg16(INA229_REG_TEMP_LIMIT, t); }
void INA229_SetPowerLimit(uint16_t t)        { INA229_WriteReg16(INA229_REG_PWR_LIMIT, t); }

/*===========================================================================
 * 器件识别
 *===========================================================================*/

uint16_t INA229_ReadManufacturerID(void) { return INA229_ReadReg16(INA229_REG_MANUFACTURER_ID); }
uint16_t INA229_ReadDeviceID(void)       { return INA229_ReadReg16(INA229_REG_DEVICE_ID); }

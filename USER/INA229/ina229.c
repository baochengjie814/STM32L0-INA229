/**
 * @file    ina229.c
 * @brief   INA229 功率监控器 SPI 驱动实现
 *
 * @author  (你的名字)
 * @date    (日期)
 *
 * @note    仅依赖 "ina229.h"、"soft_spi.h" 和 <system.h>
 *          所有软 SPI 底层函数在 soft_spi.c 中实现
 *
 *   SPI 通信格式说明:
 *   每次 SPI 传输以 CS 拉低开始, CS 拉高结束。
 *   MOSI 首字节为命令字节: [ADDR5..ADDR0 | 0 | R/W]
 *   MISO 首字节为状态字节 (含 ALRT/CRDY/MATHOF/MEMSTAT 等标志)。
 *
 *   寄存器数据宽度:
 *     16-bit: 共需 3 字节 (1 命令 + 2 数据)
 *     24-bit: 共需 4 字节 (1 命令 + 3 数据), 有效数据在低 20-bit
 *     40-bit: 共需 6 字节 (1 命令 + 5 数据), 有效数据在低 40-bit
 */

#include "ina229.h"
#include "soft_spi.h"
#include <system.h>
#include <string.h>   /* memset() */

/*===========================================================================
 * 内部常量
 *===========================================================================*/

/** @brief SPI 命令字节中的读/写标志位 */
#define INA229_CMD_READ      0x01  /**< 读操作: 命令字节最低位 = 1 */
#define INA229_CMD_WRITE     0x00  /**< 写操作: 命令字节最低位 = 0 */

/**
 * @brief SHUNT_CAL 计算公式中的常数因子
 *
 *   数据手册公式: SHUNT_CAL = 13107200000 × Rshunt × Imax
 *   其中:
 *     - 13107200000 = 0x30D400000 ≈ 1.31072 × 10^10
 *     - Rshunt 单位: Ω
 *     - Imax   单位: A
 *
 *   若计算结果 > 32767 (0x7FFF), 则除以 4 重新缩放,
 *   上限 32767 (15-bit 无符号最大值)。
 */
#define INA229_SHUNT_CAL_FACTOR       13107200000.0f
#define INA229_SHUNT_CAL_DIVISOR      4.0f
#define INA229_SHUNT_CAL_MAX_FLOAT    32767.0f

/**
 * @brief 24-bit 寄存器中有效数据位数
 *
 *   对于 VSHUNT/CURRENT: 20-bit ADC 数据, 左对齐在 24-bit 帧中,
 *   需右移 4-bit 得到实际值。
 *   对于 VBUS/POWER: 20-bit 数据, 无符号。
 */
#define INA229_24BIT_DATA_SHIFT       4

/**
 * @brief 40-bit 寄存器符号位掩码 (CHARGE 寄存器)
 *
 *   CHARGE 为 40-bit 二进制补码, 符号位在 bit39。
 *   扩展到 int64_t 时, 若 bit39=1, 则高 24-bit 全部填充 1。
 */
#define INA229_CHARGE_SIGN_BIT        0x8000000000ULL
#define INA229_CHARGE_SIGN_EXTEND     0xFFFFFF0000000000ULL

/*===========================================================================
 * 内部辅助函数
 *===========================================================================*/

/**
 * @brief  构建 SPI 命令字节
 * @param  addr 寄存器地址 (6-bit, 0x00~0x3F)
 * @param  rw   读/写标志: INA229_CMD_READ 或 INA229_CMD_WRITE
 * @return 8-bit 命令字节
 *
 *   命令字节位布局 (MSB→LSB):
 *     BIT[7:2] = ADDR[5:0]  (6-bit 寄存器地址)
 *     BIT[1]   = 0          (保留, 始终为 0)
 *     BIT[0]   = R/W        (0=写, 1=读)
 *
 *   等效公式: cmd = (addr << 2) | rw
 */
static inline uint8_t _BuildCmd(uint8_t addr, uint8_t rw)
{
    return (uint8_t)(((addr & 0x3F) << 2) | (rw & 0x01));
}

/**
 * @brief  24-bit 符号扩展为 32-bit (用于 VSHUNT/CURRENT 等有符号寄存器)
 * @param  raw24 24-bit 原始值 (bit23 为符号位, 二进制补码)
 * @return 符号扩展后的 32-bit 有符号值
 *
 *   例: raw24=0x800000 → 符号位=1 → 返回 0xFF800000 (int32_t = -8388608)
 *       raw24=0x7FFFFF → 符号位=0 → 返回 0x007FFFFF (int32_t = +8388607)
 */
static inline int32_t _SignExtend24(uint32_t raw24)
{
    if (raw24 & 0x800000UL) {
        /* 符号位为 1, 将高 8-bit 填充为 1 */
        return (int32_t)(raw24 | 0xFF000000UL);
    }
    return (int32_t)raw24;
}

/*===========================================================================
 * 初始化与系统控制
 *===========================================================================*/

/**
 * @brief  初始化 INA229 驱动
 *
 *  初始化流程:
 *    1. 初始化软件 SPI 引脚 (Soft_SPI_Init)
 *    2. 发送软件复位命令
 *    3. 读取器件 ID 验证通信正常 (期望 0x2291)
 *    4. 配置默认 ADC 参数: 连续模式, 全部通道, 1052μs转换时间, 16次平均
 *    5. 根据分流电阻和最大预期电流计算并写入 SHUNT_CAL
 *
 * @param  rshunt_ohm 分流电阻值 (Ω)
 * @param  i_max_a    最大预期电流 (A)
 * @retval true  初始化成功
 * @retval false 器件 ID 不匹配, SPI 通信失败或器件未连接
 */
bool INA229_Init(float rshunt_ohm, float i_max_a)
{
    uint16_t dev_id;
    float    scal;

    /* 1. 初始化软件 SPI 引脚 */
    Soft_SPI_Init();

    /* 2. 软件复位器件 */
    INA229_Reset();

    /* 3. 读取器件 ID 验证通信 (INA229 的 DEVICE_ID = 0x2291) */
    dev_id = INA229_ReadDeviceID();
    if (dev_id != 0x2291) {
        return false;
    }

    /* 4. 写入默认 ADC 配置:
     *    - 连续模式, 全部通道 (VBUS + VSHUNT + TEMP)
     *    - 转换时间: 1052μs (噪声与速度的折中选择)
     *    - 16 次平均值 (进一步降低噪声)
     */
    INA229_WriteReg16(INA229_REG_ADC_CONFIG,
        INA229_ADC_MODE_CONT_ALL |
        (INA229_CT_1052US << INA229_ADC_VBUSCT_Pos) |
        (INA229_CT_1052US << INA229_ADC_VSHCT_Pos) |
        (INA229_CT_1052US << INA229_ADC_VTCT_Pos) |
        INA229_AVG_16);

    /* 5. 计算并写入 SHUNT_CAL 值
     *
     *    SHUNT_CAL = 13107200000 × Rshunt × Imax
     *
     *    若结果 > 32767 (15-bit 最大值), 则除以 4 并钳位到 32767。
     *    +0.5 用于四舍五入到最近整数。
     *
     *    示例: Rshunt=0.001Ω, Imax=10A
     *      scal = 13107200000 × 0.001 × 10 = 131072000
     *      scal > 32767 → scal = 32768000 → 仍 > 32767 → 钳位 32767
     */
    scal = INA229_SHUNT_CAL_FACTOR * rshunt_ohm * i_max_a;
    if (scal > INA229_SHUNT_CAL_MAX_FLOAT) scal /= INA229_SHUNT_CAL_DIVISOR;
    if (scal > INA229_SHUNT_CAL_MAX_FLOAT) scal = INA229_SHUNT_CAL_MAX_FLOAT;
    INA229_SetShuntCal((uint16_t)(scal + 0.5f));

    return true;
}

/**
 * @brief  软件复位 INA229
 * @note   写 CONFIG[RST]=1, 然后等待 1ms 让器件完成复位序列。
 *         复位后所有寄存器恢复默认值。
 */
void INA229_Reset(void)
{
    INA229_WriteReg16(INA229_REG_CONFIG, INA229_CONFIG_RST);
    Soft_SPI_DelayUs(1000);  /* 等待 1ms 以确保复位完成 */
}

/**
 * @brief  清除电能/电荷累加器
 * @note   写 CONFIG[RSTACC]=1, RSTACC 位会自动清除, 无需手动回写。
 */
void INA229_ClearAccumulators(void)
{
    INA229_WriteReg16(INA229_REG_CONFIG, INA229_CONFIG_RSTACC);
}

/*===========================================================================
 * 底层寄存器读写
 *===========================================================================*/

/**
 * @brief  读取 16-bit 寄存器
 *
 *   SPI 帧: | CMD(1B) | D[15:8](1B) | D[7:0](1B) |
 *   发送 3 字节 (命令 + 2 填充), 接收 3 字节 (状态 + 2 数据)。
 *   注意: 首个接收字节为状态字节, 此处丢弃; 数据在 rx[1] 和 rx[2]。
 *
 * @param  addr 寄存器地址 (0x00~0x3F)
 * @return 16-bit 寄存器值
 */
uint16_t INA229_ReadReg16(uint8_t addr)
{
    uint8_t tx[3], rx[3];

    tx[0] = _BuildCmd(addr, INA229_CMD_READ);
    tx[1] = tx[2] = 0x00;  /* 填充字节, MOSI 送出 0 */

    Soft_SPI_CS_Low();
    Soft_SPI_TransferBuf(tx, rx, 3);
    Soft_SPI_CS_High();

    /* rx[0] = 状态字节 (忽略)
     * rx[1] = D[15:8], rx[2] = D[7:0] */
    return ((uint16_t)rx[1] << 8) | rx[2];
}

/**
 * @brief  读取 24-bit 寄存器 (VSHUNT, VBUS, CURRENT, POWER)
 *
 *   SPI 帧: | CMD(1B) | D[23:16](1B) | D[15:8](1B) | D[7:0](1B) |
 *   发送 4 字节, 接收 4 字节。
 *   实际有效数据为 20-bit, 左对齐在 24-bit 帧的 bit[23:4],
 *   低 4-bit 始终为 0。
 *
 * @param  addr 寄存器地址
 * @return 32-bit 原始值 (高 8-bit 为 0, 有效数据在低 24-bit)
 */
uint32_t INA229_ReadReg24(uint8_t addr)
{
    uint8_t tx[4], rx[4];

    tx[0] = _BuildCmd(addr, INA229_CMD_READ);
    tx[1] = tx[2] = tx[3] = 0x00;

    Soft_SPI_CS_Low();
    Soft_SPI_TransferBuf(tx, rx, 4);
    Soft_SPI_CS_High();

    /* rx[0] = 状态字节, rx[1..3] = 24-bit 数据 */
    return ((uint32_t)rx[1] << 16) | ((uint32_t)rx[2] << 8) | rx[3];
}

/**
 * @brief  读取 40-bit 寄存器 (ENERGY, CHARGE)
 *
 *   SPI 帧: | CMD(1B) | D[39:32](1B) | ... | D[7:0](1B) |
 *   发送 6 字节, 接收 6 字节。
 *   实际有效数据为 40-bit, 存放在 uint64_t 低 40-bit。
 *
 * @param  addr 寄存器地址
 * @return 64-bit 原始值 (高 24-bit 为 0, 有效数据在低 40-bit)
 */
uint64_t INA229_ReadReg40(uint8_t addr)
{
    uint8_t tx[6], rx[6];

    tx[0] = _BuildCmd(addr, INA229_CMD_READ);
    tx[1] = tx[2] = tx[3] = tx[4] = tx[5] = 0x00;

    Soft_SPI_CS_Low();
    Soft_SPI_TransferBuf(tx, rx, 6);
    Soft_SPI_CS_High();

    /* rx[0] = 状态字节, rx[1..5] = 40-bit 数据 */
    return ((uint64_t)rx[1] << 32)
         | ((uint64_t)rx[2] << 24)
         | ((uint64_t)rx[3] << 16)
         | ((uint64_t)rx[4] << 8)
         |  (uint64_t)rx[5];
}

/**
 * @brief  写入 16-bit 寄存器
 *
 *   SPI 帧: | CMD(1B) | D[15:8](1B) | D[7:0](1B) |
 *   发送 3 字节, 接收内容忽略 (MISO 返回旧寄存器值, 此处丢弃)。
 *
 * @param  addr 寄存器地址 (0x00~0x3F)
 * @param  data 16-bit 数据
 */
void INA229_WriteReg16(uint8_t addr, uint16_t data)
{
    uint8_t tx[3];

    tx[0] = _BuildCmd(addr, INA229_CMD_WRITE);
    tx[1] = (uint8_t)(data >> 8);     /* 高字节 D[15:8] */
    tx[2] = (uint8_t)(data & 0xFF);   /* 低字节 D[7:0]  */

    Soft_SPI_CS_Low();
    Soft_SPI_TransferBuf(tx, NULL, 3);  /* rx=NULL, 丢弃 MISO 返回 */
    Soft_SPI_CS_High();
}

/*===========================================================================
 * 测量值读取 (工程单位转换)
 *===========================================================================*/

/**
 * @brief  读取分流电压 (VSHUNT), 转换为伏特
 *
 *   转换流程:
 *     1. 读取 24-bit 原始值
 *     2. 符号扩展到 32-bit (VSHUNT 为二进制补码)
 *     3. 右移 4-bit (低 4-bit 始终为 0)
 *     4. 乘以对应 ADCRANGE 的 LSB 值
 *
 * @param  adc_range ADCRANGE 位值 (0=±163.84mV/312.5nV, 1=±40.96mV/78.125nV)
 * @return 分流电压 (V)
 */
float INA229_ReadVShunt(uint8_t adc_range)
{
    /* 1. 读原始值, 符号扩展, 2. 右移 4-bit 得到 20-bit ADC 数据 */
    int32_t val = _SignExtend24(INA229_ReadReg24(INA229_REG_VSHUNT)) >> INA229_24BIT_DATA_SHIFT;

    /* 3. 乘以 LSB, 量程不同 LSB 不同 */
    if (adc_range) {
        return (float)val * INA229_VSHUNT_LSB_ADCRANGE1;  /* 78.125 nV/LSB */
    } else {
        return (float)val * INA229_VSHUNT_LSB_ADCRANGE0;  /* 312.5 nV/LSB  */
    }
}

/**
 * @brief  读取总线电压 (VBUS), 转换为伏特
 *
 *   VBUS 为无符号 20-bit 数据, 量程 0~85V。
 *
 * @return 总线电压 (V)
 */
float INA229_ReadVBus(void)
{
    /* 读 24-bit, 右移 4-bit, 取低 20-bit 与掩码 */
    uint32_t val = (INA229_ReadReg24(INA229_REG_VBUS) >> INA229_24BIT_DATA_SHIFT) & 0xFFFFFUL;
    return (float)val * INA229_VBUS_LSB;  /* 195.3125 μV/LSB */
}

/**
 * @brief  读取芯片温度 (DIETEMP), 转换为摄氏度
 *
 *   DIETEMP 为 16-bit 二进制补码, LSB = 7.8125 m°C。
 *   量程: -256°C ~ +256°C (实际芯片工作范围约 -40°C ~ +125°C)。
 *
 * @return 芯片温度 (°C)
 */
float INA229_ReadTemperature(void)
{
    /* 转换为有符号 16-bit, 再乘以 LSB */
    return (float)((int16_t)INA229_ReadReg16(INA229_REG_DIETEMP)) * INA229_TEMP_LSB;
}

/**
 * @brief  读取电流 (CURRENT) 原始值
 *
 *   电流通过 VSHUNT / Rshunt 计算, INA229 内部自动完成:
 *     CURRENT = VSHUNT × SHUNT_CAL / 4
 *
 *   CURRENT 为 20-bit 二进制补码。
 *
 * @param  shunt_cal_value SHUNT_CAL 寄存器值
 *         (TODO: 当前未实现工程单位换算, 保留参数用于未来扩展)
 * @return 电流原始值 (需除以 SHUNT_CAL 得到安培)
 */
float INA229_ReadCurrent(uint16_t shunt_cal_value)
{
    int32_t val = _SignExtend24(INA229_ReadReg24(INA229_REG_CURRENT)) >> INA229_24BIT_DATA_SHIFT;

    /* TODO: 使用 shunt_cal_value 将原始值转换为安培:
     *   I(A) = val / shunt_cal_value
     *   当前直接返回原始值, 由调用者自行换算。
     */
    (void)shunt_cal_value;  /* 显式标记参数未使用, 消除编译器警告 */
    return (float)val;
}

/**
 * @brief  读取功率 (POWER) 原始值
 *
 *   功率通过 VSHUNT × VBUS / Rshunt 计算, INA229 内部自动完成:
 *     POWER = CURRENT × VBUS / 32
 *
 *   POWER 为 24-bit 无符号值 (但 bit23 在某些条件下可为符号位)。
 *
 * @param  shunt_cal_value SHUNT_CAL 寄存器值
 *         (TODO: 当前未实现工程单位换算, 保留参数用于未来扩展)
 * @return 功率原始值 (需除以 32×SHUNT_CAL 得到瓦特)
 */
float INA229_ReadPower(uint16_t shunt_cal_value)
{
    int32_t val = (int32_t)(INA229_ReadReg24(INA229_REG_POWER) & 0xFFFFFFUL);

    /* TODO: 使用 shunt_cal_value 将原始值转换为瓦特:
     *   P(W) = val / (32 × shunt_cal_value)
     *   当前直接返回原始值, 由调用者自行换算。
     */
    (void)shunt_cal_value;
    return (float)val;
}

/**
 * @brief  一次性读取所有主要测量数据
 *
 *   依次读取 VSHUNT, VBUS, TEMP, CURRENT, POWER, ENERGY, CHARGE,
 *   填充到 INA229_Data_t 结构体。
 *
 *   @warning 读取期间数据采样时刻不同, 各通道之间可能有微小时间差。
 *
 * @param  data      输出结构体指针 (NULL 时直接返回)
 * @param  adc_range ADCRANGE 位值
 * @param  shunt_cal SHUNT_CAL 寄存器值
 */
void INA229_ReadAll(INA229_Data_t *data, uint8_t adc_range, uint16_t shunt_cal)
{
    if (!data) return;

    /* 清零结构体, 避免残留数据干扰 */
    memset(data, 0, sizeof(*data));

    data->vshunt      = INA229_ReadVShunt(adc_range);
    data->vbus        = INA229_ReadVBus();
    data->temperature = INA229_ReadTemperature();
    data->current     = INA229_ReadCurrent(shunt_cal);
    data->power       = INA229_ReadPower(shunt_cal);
    data->energy      = INA229_ReadEnergy();
    data->charge      = INA229_ReadCharge();
}

/**
 * @brief  读取电能累积值 (ENERGY)
 * @return 40-bit 电能原始值 (LSB, 无符号)
 * @note   LSB = 3.2 × SHUNT_CAL (μJ), 需外部换算为焦耳
 */
uint64_t INA229_ReadEnergy(void)
{
    return INA229_ReadReg40(INA229_REG_ENERGY);
}

/**
 * @brief  读取电荷累积值 (CHARGE), 含 40-bit 符号扩展
 *
 *   CHARGE 为 40-bit 二进制补码, 符号位在 bit39。
 *   先读取 40-bit 原始值, 再扩展到 int64_t。
 *
 * @return 40-bit 电荷原始值 (LSB, 二进制补码)
 * @note   LSB = SHUNT_CAL (μC), 需外部换算为库仑
 */
int64_t INA229_ReadCharge(void)
{
    uint64_t raw = INA229_ReadReg40(INA229_REG_CHARGE);

    /* 符号扩展: 若 bit39=1, 则高 24-bit 全部填充 1 */
    if (raw & INA229_CHARGE_SIGN_BIT) {
        return (int64_t)(raw | INA229_CHARGE_SIGN_EXTEND);
    }
    return (int64_t)raw;
}

/**
 * @brief  检查 ADC 转换是否完成
 *
 *   通过读取 DIAG_ALRT[CNVR] 位 (bit14) 判断。
 *   连续模式下, 该位在每个转换周期结束时翻转。
 *
 * @retval true  转换完成, 各通道数据已更新
 * @retval false 转换进行中
 */
bool INA229_IsConversionReady(void)
{
    /* DIAG_ALRT bit14 = CNVR (转换就绪) */
    return (INA229_ReadReg16(INA229_REG_DIAG_ALRT) & INA229_DIAG_ALRT_CNVR) ? true : false;
}

/**
 * @brief  读取诊断/报警寄存器完整值
 * @return DIAG_ALRT 寄存器 16-bit 原始值
 */
uint16_t INA229_ReadDiagAlrt(void)
{
    return INA229_ReadReg16(INA229_REG_DIAG_ALRT);
}

/*===========================================================================
 * 配置 API (读-修改-写模式)
 *===========================================================================*/

/**
 * @brief  设置 ADC 量程
 * @param  range 0=±163.84mV (默认), 非零=±40.96mV
 */
void INA229_SetADCRange(uint8_t range)
{
    uint16_t cfg = INA229_ReadReg16(INA229_REG_CONFIG);
    cfg &= ~INA229_CONFIG_ADCRANGE;       /* 清除原 ADCRANGE 位 */
    if (range) cfg |= INA229_CONFIG_ADCRANGE;  /* 设置新值 */
    INA229_WriteReg16(INA229_REG_CONFIG, cfg);
}

/**
 * @brief  设置 ADC 工作模式
 * @param  mode 模式值 (如 INA229_ADC_MODE_CONT_ALL)
 */
void INA229_SetMode(uint16_t mode)
{
    uint16_t cfg = INA229_ReadReg16(INA229_REG_ADC_CONFIG);
    /* 清除原 MODE 字段, 写入新值 (只保留低 4-bit 模式编码) */
    cfg = (cfg & ~INA229_ADC_MODE_Msk) | (mode & INA229_ADC_MODE_Msk);
    INA229_WriteReg16(INA229_REG_ADC_CONFIG, cfg);
}

/**
 * @brief  设置各通道 ADC 转换时间
 *
 *   三个通道可以独立设置不同转换时间。
 *   转换总时间 ≈ (VBUS_CT + VSHUNT_CT + TEMP_CT) × AVG 次数
 *
 * @param  vct 总线电压转换时间
 * @param  sct 分流电压转换时间
 * @param  tct 温度转换时间
 */
void INA229_SetConversionTime(INA229_ConvTime_t vct, INA229_ConvTime_t sct, INA229_ConvTime_t tct)
{
    uint16_t cfg = INA229_ReadReg16(INA229_REG_ADC_CONFIG);

    /* 清除三个转换时间字段 */
    cfg &= ~(INA229_ADC_VBUSCT_Msk | INA229_ADC_VSHCT_Msk | INA229_ADC_VTCT_Msk);

    /* 写入新值 */
    cfg |= ((uint16_t)vct << INA229_ADC_VBUSCT_Pos);
    cfg |= ((uint16_t)sct << INA229_ADC_VSHCT_Pos);
    cfg |= ((uint16_t)tct << INA229_ADC_VTCT_Pos);

    INA229_WriteReg16(INA229_REG_ADC_CONFIG, cfg);
}

/**
 * @brief  设置平均值采样次数
 * @param  avg 采样次数枚举值 (1/4/16/.../1024)
 */
void INA229_SetAveraging(INA229_AvgCount_t avg)
{
    uint16_t cfg = INA229_ReadReg16(INA229_REG_ADC_CONFIG);
    /* 清除原 AVG 字段 (低 3-bit), 写入新值 */
    cfg = (cfg & ~INA229_ADC_AVG_Msk) | ((uint16_t)avg & 0x07);
    INA229_WriteReg16(INA229_REG_ADC_CONFIG, cfg);
}

/**
 * @brief  设置分流校准值
 * @param  v SHUNT_CAL 值 (仅低 15-bit 有效, 最大 0x7FFF)
 */
void INA229_SetShuntCal(uint16_t v)
{
    INA229_WriteReg16(INA229_REG_SHUNT_CAL, v & INA229_SHUNT_CAL_MAX);
}

/**
 * @brief  启用/禁用分流温度补偿
 *
 *   当 ppm != 0 时:
 *     1. 置位 CONFIG[TEMPCOMP]
 *     2. 写入 SHUNT_TEMPCO (温度系数, ppm/°C, 低 14-bit)
 *   当 ppm == 0 时:
 *     1. 清除 CONFIG[TEMPCOMP], 关闭温度补偿
 *
 * @param  ppm 分流电阻温度系数 (ppm/°C), 0 禁用
 */
void INA229_EnableTempComp(uint16_t ppm)
{
    uint16_t cfg = INA229_ReadReg16(INA229_REG_CONFIG);

    if (ppm) {
        /* 启用温度补偿: 置位 TEMPCOMP, 写入温度系数 */
        cfg |= INA229_CONFIG_TEMPCOMP;
        INA229_WriteReg16(INA229_REG_SHUNT_TEMPCO, ppm & 0x3FFF);  /* 14-bit */
    } else {
        /* 禁用温度补偿: 清除 TEMPCOMP 位 */
        cfg &= ~INA229_CONFIG_TEMPCOMP;
    }
    INA229_WriteReg16(INA229_REG_CONFIG, cfg);
}

/*===========================================================================
 * 报警阈值设置
 *
 *   当测量值超过阈值时, DIAG_ALRT 寄存器对应标志位置位,
 *   同时 ALERT 引脚 (若有连接) 被拉低。
 *===========================================================================*/

void INA229_SetShuntOverVoltage(uint16_t t)  { INA229_WriteReg16(INA229_REG_SOVL, t); }
void INA229_SetShuntUnderVoltage(uint16_t t) { INA229_WriteReg16(INA229_REG_SUVL, t); }

/**
 * @brief  设置总线过压/欠压阈值
 * @note   BOVL/BUVL 寄存器只有低 15-bit 有效, 高位置零
 */
void INA229_SetBusOverVoltage(uint16_t t)    { INA229_WriteReg16(INA229_REG_BOVL, t & 0x7FFF); }
void INA229_SetBusUnderVoltage(uint16_t t)   { INA229_WriteReg16(INA229_REG_BUVL, t & 0x7FFF); }
void INA229_SetTempLimit(uint16_t t)         { INA229_WriteReg16(INA229_REG_TEMP_LIMIT, t); }
void INA229_SetPowerLimit(uint16_t t)        { INA229_WriteReg16(INA229_REG_PWR_LIMIT, t); }

/*===========================================================================
 * 器件 ID
 *===========================================================================*/

/**
 * @brief  读取制造商 ID
 * @return TI = 0x5449 (ASCII: 'T'=0x54, 'I'=0x49)
 */
uint16_t INA229_ReadManufacturerID(void) { return INA229_ReadReg16(INA229_REG_MANUFACTURER_ID); }

/**
 * @brief  读取器件 ID
 * @return INA229 = 0x2291
 */
uint16_t INA229_ReadDeviceID(void)       { return INA229_ReadReg16(INA229_REG_DEVICE_ID); }

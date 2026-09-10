#include "APP_PortMonitor.h"
// I2C通信端口选择：使用I2C通道1
#define APP_PORT_MONITOR_I2C_PORT_ID       APP_I2C_PORT_1_ID

#define APP_PORT_MONITOR_COMMAND_VOLTAGE   0x10U  // 读取电压命令
#define APP_PORT_MONITOR_COMMAND_CURRENT   0x11U  // 读取电流命令
#define APP_PORT_MONITOR_COMMAND_TEMP      0x20U  // 读取温度命令
#define APP_PORT_MONITOR_COMMAND_STATUS    0x30U  // 读取端口连接状态命令
#define APP_PORT_MONITOR_SHORT_FRAME_LEN   2U     // 短帧长度：1字节数据 + 1字节校验和（状态、温度使用）
#define APP_PORT_MONITOR_LONG_FRAME_LEN    7U     // 长帧长度：多通道数据 + 1字节校验和（电压、电流使用）
#define APP_PORT_MONITOR_CHANNEL_COUNT     2U     // 单颗监控芯片支持的端口通道数
#define APP_PORT_MONITOR_POWER_MAX_W       140U   // 单端口最大功率钳位值（单位：W）
#define APP_PORT_MONITOR_POWER_DIVISOR     1000000UL  // 功率计算除数：mV * mA = 微瓦，除以1e6转换为瓦
typedef struct
{
  uint8_t status[APP_PORT_MONITOR_SHORT_FRAME_LEN];    // 状态原始数据帧（短帧）
  uint8_t voltage[APP_PORT_MONITOR_LONG_FRAME_LEN];    // 电压原始数据帧（长帧）
  uint8_t current[APP_PORT_MONITOR_LONG_FRAME_LEN];    // 电流原始数据帧（长帧）
  uint8_t temperature[APP_PORT_MONITOR_SHORT_FRAME_LEN];// 温度原始数据帧（短帧）
} APP_PortMonitorRawDevice;

typedef struct
{
  uint8_t port_index[APP_PORT_MONITOR_CHANNEL_COUNT];  // 通道对应的逻辑端口编号数组
  uint8_t state_mask[APP_PORT_MONITOR_CHANNEL_COUNT];  // 每个通道在状态字节中的位掩码
  uint8_t data_offset[APP_PORT_MONITOR_CHANNEL_COUNT]; // 每个通道数据在长帧中的字节偏移
} APP_PortMonitorDeviceMap;

// 全局端口监控数据快照（对外提供的最终解析数据）
APP_PortMonitorData g_app_port_monitor_data;
// 全局运行诊断统计信息（错误计数、最后错误状态等）
APP_PortMonitorDiagnostics g_app_port_monitor_diagnostics;

static uint32_t s_app_port_monitor_last_poll_tick;  // 上次轮询的系统tick时间，用于控制轮询周期
static uint8_t  s_app_port_monitor_first_poll;      // 首次轮询标志：1表示首次运行，跳过周期判断

// I2C设备地址表：共4颗监控芯片，地址依次为0x58、0x5A、0x5C、0x5E
static const uint8_t s_app_port_monitor_address[APP_PORT_MONITOR_DEVICE_COUNT] =
{
  0x58U, 0x5AU, 0x5CU, 0x5EU
};

/**
 * @brief 设备-端口映射表
 * @details 4颗设备，每颗2个通道，分别映射到不同逻辑端口；
 *          每组包含：端口索引数组、状态掩码数组、数据偏移数组
 */
static const APP_PortMonitorDeviceMap s_app_port_monitor_map[APP_PORT_MONITOR_DEVICE_COUNT] =
{
  // 设备0：端口C1、C2，状态掩码bit0/bit1，数据偏移0/2
  {{APP_PORT_C1, APP_PORT_C2}, {0x01U, 0x02U}, {0U, 2U}},
  // 设备1：端口C3、C4，状态掩码bit0/bit1，数据偏移0/2
  {{APP_PORT_C3, APP_PORT_C4}, {0x01U, 0x02U}, {0U, 2U}},
  // 设备2：端口C5、A1，状态掩码bit0/bit2，数据偏移0/4
  {{APP_PORT_C5, APP_PORT_A1}, {0x01U, 0x04U}, {0U, 4U}},
  // 设备3：端口C6、A2，状态掩码bit0/bit2，数据偏移0/4
  {{APP_PORT_C6, APP_PORT_A2}, {0x01U, 0x04U}, {0U, 4U}}
};

static void    APP_PortMonitor_ClearData(APP_PortMonitorData *data);
static uint8_t APP_PortMonitor_ReadResponse(uint8_t deviceIndex,
                                            uint8_t command,
                                            uint8_t *response,
                                            uint16_t responseLength);
static uint8_t APP_PortMonitor_ChecksumValid(uint8_t command,
                                             const uint8_t *response,
                                             uint16_t responseLength);
static uint8_t APP_PortMonitor_Acquire(APP_PortMonitorRawDevice *raw);
static uint16_t APP_PortMonitor_LoadLe16(const uint8_t *data);
static void    APP_PortMonitor_BuildData(const APP_PortMonitorRawDevice *raw,
                                         APP_PortMonitorData *data);

/************************* 对外接口函数 *************************/
/**
 * @brief 端口监控模块初始化
 * @details 清空全局数据，初始化诊断统计计数器，重置轮询时间与首次运行标志
 */
void APP_PortMonitor_Init(void)
{
  // 清空全局监控数据缓冲区
  APP_PortMonitor_ClearData(&g_app_port_monitor_data);

  // 初始化诊断统计变量
  g_app_port_monitor_diagnostics.poll_attempt_count     = 0UL;    // 轮询尝试总次数
  g_app_port_monitor_diagnostics.successful_cycle_count = 0UL;    // 成功轮询周期数
  g_app_port_monitor_diagnostics.failed_cycle_count     = 0UL;    // 失败轮询周期数
  g_app_port_monitor_diagnostics.checksum_error_count   = 0UL;    // 校验和错误次数
  g_app_port_monitor_diagnostics.i2c_error_count        = 0UL;    // I2C通信错误次数
  g_app_port_monitor_diagnostics.last_failed_address    = 0U;     // 最后一次失败的设备地址
  g_app_port_monitor_diagnostics.last_failed_command    = 0U;     // 最后一次失败的命令字
  g_app_port_monitor_diagnostics.last_i2c_status        = HAL_OK; // 最后一次I2C操作状态
  g_app_port_monitor_diagnostics.last_result            = APP_PORT_MONITOR_RESULT_NOT_RUN; // 最后一次轮询结果

  // 重置轮询时间戳与首次轮询标志
  s_app_port_monitor_last_poll_tick = 0UL;
  s_app_port_monitor_first_poll     = 1U;
}

/**
 * @brief 端口监控轮询处理函数（需在主循环中周期调用）
 * @param now 当前系统tick时间（单位：ms）
 * @return uint8_t 1=本次执行了完整轮询并更新数据；0=未到轮询时间或轮询失败
 * @details 按配置周期轮询所有设备，采集并解析数据，更新全局数据快照与诊断信息
 */
uint8_t APP_PortMonitor_Poll(uint32_t now)
{
  APP_PortMonitorRawDevice raw[APP_PORT_MONITOR_DEVICE_COUNT]; // 所有设备的原始数据数组
  APP_PortMonitorData nextData;                                // 临时存储解析后的新数据

  // 非首次轮询时，检查是否到达轮询周期，未到则直接返回
  if ((s_app_port_monitor_first_poll == 0U) &&
      ((uint32_t)(now - s_app_port_monitor_last_poll_tick) < APP_PORT_MONITOR_POLL_MS))
  {
    return 0U;
  }

  // 清除首次轮询标志，更新本次轮询时间戳
  s_app_port_monitor_first_poll     = 0U;
  s_app_port_monitor_last_poll_tick = now;

  // 轮询尝试次数+1
  g_app_port_monitor_diagnostics.poll_attempt_count++;

  // 采集所有设备的原始数据，失败则计数并返回
  if (APP_PortMonitor_Acquire(raw) == 0U)
  {
    g_app_port_monitor_diagnostics.failed_cycle_count++;
    return 0U;
  }

  // 清空临时数据，将原始数据解析为应用层数据结构
  APP_PortMonitor_ClearData(&nextData);
  APP_PortMonitor_BuildData(raw, &nextData);

  // 标记数据有效，序号递增，记录更新时间
  nextData.valid            = 1U;
  nextData.sequence         = g_app_port_monitor_data.sequence + 1UL;
  nextData.last_update_tick = now;

  // 更新全局数据快照（结构体直接赋值，实际项目中可根据需要加锁保护）
  g_app_port_monitor_data = nextData;

  // 更新诊断信息：成功计数+1，清除错误记录
  g_app_port_monitor_diagnostics.successful_cycle_count++;
  g_app_port_monitor_diagnostics.last_failed_address = 0U;
  g_app_port_monitor_diagnostics.last_failed_command = 0U;
  g_app_port_monitor_diagnostics.last_i2c_status     = HAL_OK;
  g_app_port_monitor_diagnostics.last_result         = APP_PORT_MONITOR_RESULT_OK;

  return 1U;
}

/**
 * @brief 获取端口监控数据快照
 * @return const APP_PortMonitorData* 指向全局监控数据的只读指针
 * @details 上层模块通过该接口获取最新的端口状态、电参数、温度等数据
 */
const APP_PortMonitorData *APP_PortMonitor_GetSnapshot(void)
{
  return &g_app_port_monitor_data;
}

/************************* 内部静态函数 *************************/
/**
 * @brief 清空监控数据结构体
 * @param data 指向待清空的监控数据结构体指针
 * @details 将所有端口参数、设备温度、有效标志等重置为默认值
 */
static void APP_PortMonitor_ClearData(APP_PortMonitorData *data)
{
  uint8_t index;

  // 空指针保护
  if (data == 0)
  {
    return;
  }

  // 遍历所有端口，清空端口相关数据
  for (index = 0U; index < APP_PORT_MONITOR_PORT_COUNT; index++)
  {
    data->port_connected[index]  = 0U;  // 连接状态置为未连接
    data->port_voltage_mv[index] = 0U;  // 电压置0（单位：mV）
    data->port_current_ma[index] = 0U;  // 电流置0（单位：mA）
    data->port_power_w[index]    = 0U;  // 功率置0（单位：W）
  }

  // 遍历所有设备，清空设备温度数据
  for (index = 0U; index < APP_PORT_MONITOR_DEVICE_COUNT; index++)
  {
    data->device_temperature_c[index] = 0U; // 设备温度置0（单位：℃）
  }

  // 清空全局统计字段
  data->max_temperature_c = 0U;    // 最高温度置0
  data->valid             = 0U;    // 数据有效标志置为无效
  data->sequence          = 0UL;   // 数据序号置0
  data->last_update_tick  = 0UL;   // 最后更新时间置0
}

/**
 * @brief 读取指定设备的命令响应数据
 * @param deviceIndex 设备索引（0~APP_PORT_MONITOR_DEVICE_COUNT-1）
 * @param command     I2C命令字
 * @param response    输出参数：存储响应数据的缓冲区指针
 * @param responseLength 期望读取的响应数据长度
 * @return uint8_t 1=读取成功且校验和正确；0=失败（参数错误、I2C错误、校验和错误）
 * @details 执行I2C读命令，并自动校验和验证，同时更新诊断错误统计
 */
static uint8_t APP_PortMonitor_ReadResponse(uint8_t deviceIndex,
                                            uint8_t command,
                                            uint8_t *response,
                                            uint16_t responseLength)
{
  HAL_StatusTypeDef status;
  uint8_t address;

  // 参数合法性检查：设备索引越界、空指针、长度不足
  if ((deviceIndex >= APP_PORT_MONITOR_DEVICE_COUNT) ||
      (response == 0) || (responseLength < 2U))
  {
    return 0U;
  }

  // 获取设备I2C地址
  address = s_app_port_monitor_address[deviceIndex];

  // 执行I2C读命令
  status = APP_I2C_ReadCommand(APP_PORT_MONITOR_I2C_PORT_ID,
                               address,
                               command,
                               response,
                               responseLength);

  // 记录最后一次I2C状态
  g_app_port_monitor_diagnostics.last_i2c_status = status;

  // I2C通信失败处理：统计错误，记录失败信息
  if (status != HAL_OK)
  {
    g_app_port_monitor_diagnostics.i2c_error_count++;
    g_app_port_monitor_diagnostics.last_failed_address = address;
    g_app_port_monitor_diagnostics.last_failed_command = command;
    g_app_port_monitor_diagnostics.last_result         = APP_PORT_MONITOR_RESULT_I2C_ERROR;
    return 0U;
  }

  // 校验和验证失败处理：统计错误，记录失败信息
  if (APP_PortMonitor_ChecksumValid(command, response, responseLength) == 0U)
  {
    g_app_port_monitor_diagnostics.checksum_error_count++;
    g_app_port_monitor_diagnostics.last_failed_address = address;
    g_app_port_monitor_diagnostics.last_failed_command = command;
    g_app_port_monitor_diagnostics.last_result         = APP_PORT_MONITOR_RESULT_CHECKSUM_ERROR;
    return 0U;
  }

  return 1U;
}

/**
 * @brief 校验和验证函数
 * @param command        本次发送的命令字（参与校验和计算）
 * @param response       响应数据缓冲区指针
 * @param responseLength 响应数据总长度（含校验和字节）
 * @return uint8_t 1=校验和正确；0=错误
 * @details 校验和算法：命令字 + 所有数据字节（除最后一位校验和），结果与帧尾字节对比
 *          累加过程自动截断为8位
 */
static uint8_t APP_PortMonitor_ChecksumValid(uint8_t command,
                                             const uint8_t *response,
                                             uint16_t responseLength)
{
  uint16_t index;
  uint8_t  checksum = command; // 校验和初始值为命令字

  // 参数检查：空指针、长度不足2（至少1字节数据+1字节校验和）
  if ((response == 0) || (responseLength < 2U))
  {
    return 0U;
  }

  // 累加所有数据字节（除最后一个校验和字节）
  for (index = 0U; index < (responseLength - 1U); index++)
  {
    checksum = (uint8_t)(checksum + response[index]);
  }

  // 对比计算值与帧尾校验和
  return (checksum == response[responseLength - 1U]) ? 1U : 0U;
}

/**
 * @brief 采集所有设备的全部原始数据
 * @param raw 输出参数：存储所有设备原始数据的数组指针
 * @return uint8_t 1=全部采集成功；0=任意一项采集失败
 * @details 按顺序读取所有设备的状态、电压、电流、温度四类数据
 *          任意一次读取失败立即终止并返回失败，保证数据完整性
 */
static uint8_t APP_PortMonitor_Acquire(APP_PortMonitorRawDevice *raw)
{
  uint8_t device;

  // 空指针保护
  if (raw == 0)
  {
    return 0U;
  }

  // 第一步：读取所有设备的状态数据
  for (device = 0U; device < APP_PORT_MONITOR_DEVICE_COUNT; device++)
  {
    if (APP_PortMonitor_ReadResponse(device,
                                     APP_PORT_MONITOR_COMMAND_STATUS,
                                     raw[device].status,
                                     APP_PORT_MONITOR_SHORT_FRAME_LEN) == 0U)
    {
      return 0U;
    }
  }

  // 第二步：读取所有设备的电压数据
  for (device = 0U; device < APP_PORT_MONITOR_DEVICE_COUNT; device++)
  {
    if (APP_PortMonitor_ReadResponse(device,
                                     APP_PORT_MONITOR_COMMAND_VOLTAGE,
                                     raw[device].voltage,
                                     APP_PORT_MONITOR_LONG_FRAME_LEN) == 0U)
    {
      return 0U;
    }
  }

  // 第三步：读取所有设备的电流数据
  for (device = 0U; device < APP_PORT_MONITOR_DEVICE_COUNT; device++)
  {
    if (APP_PortMonitor_ReadResponse(device,
                                     APP_PORT_MONITOR_COMMAND_CURRENT,
                                     raw[device].current,
                                     APP_PORT_MONITOR_LONG_FRAME_LEN) == 0U)
    {
      return 0U;
    }
  }

  // 第四步：读取所有设备的温度数据
  for (device = 0U; device < APP_PORT_MONITOR_DEVICE_COUNT; device++)
  {
    if (APP_PortMonitor_ReadResponse(device,
                                     APP_PORT_MONITOR_COMMAND_TEMP,
                                     raw[device].temperature,
                                     APP_PORT_MONITOR_SHORT_FRAME_LEN) == 0U)
    {
      return 0U;
    }
  }

  return 1U;
}

/**
 * @brief 小端模式读取16位无符号整数
 * @param data 指向2字节数据的指针
 * @return uint16_t 解析后的16位整数
 * @details data[0]为低字节，data[1]为高字节，拼接为小端16位值
 */
static uint16_t APP_PortMonitor_LoadLe16(const uint8_t *data)
{
  return (uint16_t)((uint16_t)data[0] | ((uint16_t)data[1] << 8U));
}

/**
 * @brief 将原始数据解析构建为应用层监控数据
 * @param raw  输入参数：所有设备的原始数据数组
 * @param data 输出参数：解析后的应用层监控数据结构体
 * @details 遍历每个设备的每个通道，根据映射表解析连接状态、电压、电流、功率
 *          同时统计设备温度与全局最高温度，端口未连接时电参数保持0
 */
static void APP_PortMonitor_BuildData(const APP_PortMonitorRawDevice *raw,
                                      APP_PortMonitorData *data)
{
  uint8_t device;
  uint8_t channel;

  // 遍历每个监控设备
  for (device = 0U; device < APP_PORT_MONITOR_DEVICE_COUNT; device++)
  {
    // 读取设备温度（温度帧第0字节为温度值，单位：℃）
    uint8_t temperature = raw[device].temperature[0];
    data->device_temperature_c[device] = temperature;

    // 更新全局最高温度
    if (temperature > data->max_temperature_c)
    {
      data->max_temperature_c = temperature;
    }

    // 遍历设备的每个通道
    for (channel = 0U; channel < APP_PORT_MONITOR_CHANNEL_COUNT; channel++)
    {
      // 从映射表获取当前通道对应的逻辑端口、状态掩码、数据偏移
      uint8_t port   = s_app_port_monitor_map[device].port_index[channel];
      uint8_t mask   = s_app_port_monitor_map[device].state_mask[channel];
      uint8_t offset = s_app_port_monitor_map[device].data_offset[channel];

      // 按掩码读取端口连接状态
      uint8_t connected = ((raw[device].status[0] & mask) != 0U) ? 1U : 0U;
      data->port_connected[port] = connected;

      // 端口已连接时，解析电压、电流并计算功率
      if (connected != 0U)
      {
        // 按偏移读取小端16位电压值（单位：mV）
        uint16_t voltage = APP_PortMonitor_LoadLe16(&raw[device].voltage[offset]);
        // 按偏移读取小端16位电流值（单位：mA）
        uint16_t current = APP_PortMonitor_LoadLe16(&raw[device].current[offset]);

        // 计算功率：P(μW) = U(mV) * I(mA)，除以1e6转换为瓦
        uint32_t power = ((uint32_t)voltage * (uint32_t)current) /
                         APP_PORT_MONITOR_POWER_DIVISOR;

        // 功率钳位：超过最大值则限制为最大值
        if (power > APP_PORT_MONITOR_POWER_MAX_W)
        {
          power = APP_PORT_MONITOR_POWER_MAX_W;
        }

        // 保存解析后的电参数
        data->port_voltage_mv[port] = voltage;
        data->port_current_ma[port] = current;
        data->port_power_w[port]    = (uint8_t)power;
      }
    }
  }
}

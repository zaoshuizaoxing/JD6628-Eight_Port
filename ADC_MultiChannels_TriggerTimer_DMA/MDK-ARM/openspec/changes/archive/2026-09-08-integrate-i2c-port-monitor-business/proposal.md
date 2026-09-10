## Why

LCD 页面目前只能使用固定零值快照，无法反映四个 I²C 从设备所管理的八路端口状态、功率和温度。需要建立驱动与业务解耦的采集链路，以固定周期生成完整、可调试且可直接供 LCD 消费的数据快照。

## What Changes

- 扩展 PF0/PF1 软件 I²C 驱动，支持使用 7 位地址发送命令并连续读取定长响应，同时补充超时与总线恢复。
- 新增四从机端口监控业务模块，按状态、电压、电流、温度的固定顺序每 500 ms 查询 `0x58`、`0x5A`、`0x5C`、`0x5E`。
- 校验完整响应，解析小端电压与电流，计算整数瓦功率并提取四设备最高温度；只有整轮有效时才发布新快照。
- 将 LCD 数据顺序固定为 C1、C2、C3、C4、C5、C6、A1、A2，并以业务快照替代固定零数据。
- 暴露业务数据和诊断全局变量以及初始化、轮询、快照读取接口，方便调试与后续业务开发。
- 保持现有 Flash 烧录与 LCD 模式宏的位置和值不变，Flash 烧录模式不执行 I²C 业务轮询。

## Capabilities

### New Capabilities

- `i2c-port-monitor-business`: 定义单主四从查询协议、数据映射、功率与温度处理、500 ms调度、完整快照发布及故障诊断。

### Modified Capabilities

- `lcd-key-page-business`: 将固定零值快照改为消费 I²C 端口监控快照，并明确八路显示顺序。
- `minimal-flash-loader-firmware`: 保持 I²C 驱动与业务分层，同时允许 LCD 模式启动端口监控业务。

## Impact

- 修改 `APP_I2C.c/.h` 和 `APP_LCD_KeyPage.c`。
- 新增 `APP_PortMonitor.c/.h` 并加入 Keil 工程。
- 不改变 LCD、W25Q、SPI、UART Flash Loader 的驱动接口及现有业务宏值。

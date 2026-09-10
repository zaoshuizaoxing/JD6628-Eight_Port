# lcd-key-page-business Specification

## Purpose
TBD - created by archiving change port-lcd-key-page-business. Update Purpose after archive.
## Requirements
### Requirement: LCD 页面业务入口
固件 SHALL 将 LCD 按键页面逻辑封装在独立应用模块中，并 SHALL 仅通过 `APP_LCD_KeyPage_Run()` 启动该业务。

#### Scenario: 启用 LCD 页面模式
- **WHEN** `ENABLE_LCD_KEY_PAGE_MODE` 为启用且 Flash 烧录模式关闭
- **THEN** 主循环调用 `APP_LCD_KeyPage_Run()`，且 `main.c` 不包含页面状态机、资源表或渲染实现

### Requirement: 页面启动与按键切换
LCD 页面业务 SHALL 上电显示启动图 4 秒，随后进入笑脸页面，并 SHALL 使用 PB2 高电平有效按键依次切换笑脸、八路数据、温度、累计时间、黑屏页面。

#### Scenario: 启动页面
- **WHEN** LCD 页面业务首次运行
- **THEN** 系统从 W25Q 显示启动图 4 秒后进入笑脸动画页面

#### Scenario: 有效按键切页
- **WHEN** PB2 高电平稳定至少 50 ms 且距离上次触发至少 150 ms
- **THEN** 页面只前进一次，并按笑脸、八路数据、温度、累计时间、黑屏、笑脸的顺序循环

#### Scenario: 黑屏恢复
- **WHEN** 当前为黑屏页面且收到下一次有效按键
- **THEN** 系统恢复背光并进入笑脸页面

### Requirement: 零值业务快照
LCD 页面业务 SHALL 在端口监控首次发布有效 I²C 快照前使用八路端口关闭、所有业务数值为零的快照；首次有效快照发布后 SHALL 使用实时端口状态、功率和最高温度进行显示。

#### Scenario: 首次有效数据前
- **WHEN** 用户在端口监控尚未成功完成整轮查询前查看各页面
- **THEN** 系统显示未充电笑脸、八路零值、低温资源和0分钟

#### Scenario: 有效数据发布后
- **WHEN** 端口监控发布新的完整有效快照
- **THEN** LCD业务按快照序号同步八路连接状态、功率和最高温度

### Requirement: W25Q 图像与数字资源显示
LCD 页面业务 SHALL 使用示例工程定义的 W25Q 固定资源地址、有效显示长度和数字坐标渲染完整图像、动画和局部数字覆盖层。

#### Scenario: 完整图像显示
- **WHEN** 页面需要显示完整 240×240 RGB565 图像
- **THEN** 系统仅从对应 Flash 起始地址读取并发送 `ST7789_FRAME_BYTES` 有效数据，不发送槽位填充数据

#### Scenario: 数字资源显示
- **WHEN** 八路数据、合计值或累计时间需要显示数字
- **THEN** 系统跳过数字资源的 8 字节头并在示例坐标处显示限制范围内的数字

### Requirement: 共享 DMA 串行所有权
LCD 页面业务 MUST 保证 W25Q SPI1 数据读取与 LCD SPI2 DMA 发送不并发使用 DMA1 Channel3。

#### Scenario: 分块图像传输
- **WHEN** 从 W25Q 向 LCD 流式传输任意资源
- **THEN** 每一块数据必须先完整执行阻塞式 W25Q 读取，再启动并完成 LCD 数据发送，然后才能读取下一块

### Requirement: 板级配置保持
移植 MUST 保持当前工程的 LCD、W25Q、SPI 和 I²C 引脚及初始化实现，并 MUST 使用当前 `ST7789_ROTATION=90U` 配置。

#### Scenario: 检查板级映射
- **WHEN** 对移植后的 MSP、驱动和业务模块执行静态检查
- **THEN** PB3 仍为 W25Q SPI1 SCK、PB2 仅为页面按键，且示例工程的 USI/SPI 和时钟初始化未被引入

### Requirement: 充电页面显示八路端口合计值
系统 SHALL 按 `C1,C2,C3,C4,C5,C6,A1,A2` 的顺序将八路整数功率写入LCD业务快照，并 SHALL 将八路功率全部相加作为充电页面显示值，MUST NOT 使用任一单路最大值代替合计值；合计结果超过590时 SHALL 限幅为590。

#### Scenario: 八路端口功率正常求和
- **WHEN** 八路功率依次为10、20、30、40、0、0、0、0
- **THEN** 充电页面显示值为100

#### Scenario: 合计值超过显示上限
- **WHEN** 八路功率的合计大于590
- **THEN** 充电页面显示值限幅为590

#### Scenario: C6与A1顺序
- **WHEN** `0x5C`发布A1数据且 `0x5E`发布C6数据
- **THEN** LCD索引5显示C6，索引6显示A1


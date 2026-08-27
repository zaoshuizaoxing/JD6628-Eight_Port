## ADDED Requirements

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
本阶段 LCD 页面业务 MUST 不执行 I²C 读取，并 SHALL 使用八路端口关闭、所有业务数值为零的固定快照进行显示。

#### Scenario: 零数据页面显示
- **WHEN** 用户依次查看笑脸、八路数据、温度和累计时间页面
- **THEN** 系统显示未充电笑脸动画、八路零值、低温资源和 0 分钟

#### Scenario: 不产生 I²C 事务
- **WHEN** LCD 页面业务持续运行
- **THEN** 业务模块不得调用软件 I²C 寄存器读取接口，但现有 I²C 驱动和初始化能力继续保留

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

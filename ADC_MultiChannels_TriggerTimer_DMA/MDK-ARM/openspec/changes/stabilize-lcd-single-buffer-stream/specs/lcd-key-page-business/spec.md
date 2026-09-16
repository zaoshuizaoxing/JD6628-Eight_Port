## ADDED Requirements

### Requirement: 固定 DMA 通道与单缓冲传输
LCD 图像传输 MUST 将 DMA1 Channel1 固定用于 SPI2 TX，将 DMA1 Channel2/3 固定用于 SPI1 TX/RX，并 SHALL 由 LCD 传输驱动独占一个 4096 字节静态缓冲区；应用业务层 MUST NOT 执行 DMA 通道重映射或管理图像传输缓冲区。

#### Scenario: 传输一个完整 Flash 块
- **WHEN** 页面业务请求从 W25Q 向 LCD 发送不超过 4096 字节的有效图像数据
- **THEN** 驱动发送快速读取命令后启动一次完整 SPI1 RX DMA，并仅在半传输完成后发送缓冲区前半段、在全传输完成后发送后半段

#### Scenario: DMA 通道保持固定
- **WHEN** Flash RX DMA 与 LCD TX DMA 在同一图像块内先后工作
- **THEN** SPI1 RX 始终使用 DMA1 Channel3，LCD SPI2 TX 始终使用 DMA1 Channel1，传输过程中不得重映射任一活动通道

#### Scenario: 应用层请求图像传输
- **WHEN** `APP_LCD_KeyPage` 需要显示完整图像、动画帧或局部资源
- **THEN** 应用层仅向 `ST7789_WriteFlashData()` 提供 Flash 地址和有效长度，不持有 4096 字节图像缓冲区或 DMA 状态

### Requirement: Flash-to-LCD 有界恢复
Flash-to-LCD 驱动 MUST 对 DMA 等待设置有限超时，并 MUST 在成功、超时、终止或错误后释放 W25Q 片选并恢复 SPI1 双线方向；DMA 未完成的数据 SHALL 使用阻塞式 Flash 读取补齐后再发送到 LCD。

#### Scenario: Flash RX DMA 启动失败
- **WHEN** SPI1 RX DMA 无法启动
- **THEN** 驱动恢复 Flash/SPI 状态，使用阻塞式读取获得当前块，并继续发送正确的有效数据

#### Scenario: 后半块超时或错误
- **WHEN** 前半块已经发送但 SPI1 RX DMA 在全传输完成前超时或报错
- **THEN** 驱动只重新读取尚未提交的后半块，发送后半块并恢复 SPI1 双线模式

### Requirement: 板级稳定显示与按键配置
系统 SHALL 将 SPI2 波特率预分频保持为 `/4`，并 MUST 保持 PB2 输入下拉、高电平有效、稳定至少 50 ms 且距离上次触发至少 150 ms 的页面按键行为。

#### Scenario: 稳定显示时钟
- **WHEN** 初始化 LCD SPI2
- **THEN** SPI2 使用 `SPI_BAUDRATEPRESCALER_4`，而 W25Q SPI1 保持其既有 `/2` 配置

#### Scenario: PB2 页面切换
- **WHEN** PB2 从释放状态进入高电平并稳定至少 50 ms，且满足 150 ms 防重复间隔
- **THEN** 页面状态机只前进一次，并在 PB2 返回低电平后重新允许下一次触发

## REMOVED Requirements

### Requirement: 共享 DMA 串行所有权
**Reason**: DMA1 Channel1 is available after ADC removal, so SPI2 TX no longer needs to share Channel3 with SPI1 RX. The old strictly serialized blocking-read requirement does not describe the final board-tested transport.

**Migration**: Map LCD SPI2 TX permanently to DMA1 Channel1, retain W25Q SPI1 TX/RX on Channels2/3, and use the driver-owned 4096-byte half/full-completion transfer described above.

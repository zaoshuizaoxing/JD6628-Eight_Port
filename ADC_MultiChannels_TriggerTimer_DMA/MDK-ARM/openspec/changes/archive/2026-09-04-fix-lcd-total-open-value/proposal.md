## Why

当前充电页面字段命名为 `max_open_value`，且没有从八路端口值计算量产业务要求的合计值，容易把“八路总和”误实现成“单路最大值”。需要在接入 I2C 数据前先统一数据语义，确保后续收到端口数据后显示结果正确。

## What Changes

- 将充电页面业务快照字段由“最大开启值”更名为“八路合计值”。
- 每次业务循环对八路端口值求和，并按现有显示上限 590 进行限幅后供充电页面显示。
- 保持当前零数据模式，不新增 I2C 读取或解析逻辑。
- 不改变 Flash 烧录宏、驱动层、页面顺序、资源地址和 DMA 数据路径。

## Capabilities

### New Capabilities

无。

### Modified Capabilities

- `lcd-key-page-business`: 明确充电页面显示八路端口值的合计，而不是单路最大值，并规定显示上限。

## Impact

- 修改 `Src/APP_LCD_KeyPage.c` 内部业务快照和派生值计算。
- 不改变 `APP_LCD_KeyPage_Run()` 公共接口，不影响 LCD、W25Q、SPI、I2C 驱动及 Flash 烧录业务。

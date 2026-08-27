## MODIFIED Requirements

### Requirement: 单一 Flash 烧录业务
固件 SHALL 保留 UART Flash Loader 及其 `ENABLE_UART_FLASH_LOADER` 宏不变，并 SHALL 允许新增的 `ENABLE_LCD_KEY_PAGE_MODE` 选择 LCD 页面业务；两个业务模式 MUST 编译期互斥且一次构建只能运行其中一个。

#### Scenario: 启用 Flash 烧录模式
- **WHEN** `ENABLE_UART_FLASH_LOADER` 被配置为启用且 LCD 页面模式关闭
- **THEN** 系统初始化 W25Q 与 UART 后持续执行 UART Flash Loader，且不启动 LCD 页面业务、ADC 或 I²C 轮询业务

#### Scenario: 启用 LCD 页面模式
- **WHEN** `ENABLE_LCD_KEY_PAGE_MODE` 被配置为启用且 Flash 烧录模式关闭
- **THEN** 系统执行独立 LCD 页面业务，且不进入 UART Flash Loader

#### Scenario: 同时启用两个业务模式
- **WHEN** `ENABLE_UART_FLASH_LOADER` 与 `ENABLE_LCD_KEY_PAGE_MODE` 同时被配置为启用
- **THEN** 编译必须报错并拒绝生成固件

#### Scenario: 关闭全部业务模式
- **WHEN** 两个业务宏均被配置为关闭
- **THEN** 系统可初始化保留的外设驱动，但主循环不得执行产品业务

### Requirement: 删除 LCD 产品业务
固件 MUST 保持旧 `APP_LCD_UI`、测试图片流程及其业务接口处于删除状态，但 SHALL 允许由 `APP_LCD_KeyPage` 独立模块重新引入经规格约束的 W25Q 图片地址、数字资源、覆盖层和页面策略，同时继续保留纯 ST7789 驱动与基础绘图接口。

#### Scenario: 检查旧 LCD 业务残留
- **WHEN** 搜索当前应用源码、头文件和 Keil 工程清单
- **THEN** 不得存在 `APP_LCD_UI` 文件或调用，且新的组合业务只能位于 `APP_LCD_KeyPage` 模块

#### Scenario: LCD 驱动继续存在
- **WHEN** 新业务代码包含 `lcd_spi.h`
- **THEN** ST7789 初始化、SPI 数据传输、窗口设置和基础绘图接口仍可用且公开接口不变

### Requirement: 清理功能宏与工程依赖
固件 MUST 仅保留 `ENABLE_UART_FLASH_LOADER` 与 `ENABLE_LCD_KEY_PAGE_MODE` 作为产品业务功能宏，并 MUST 使公共头文件、HAL 模块配置、MSP、IRQ 和 Keil 编译清单与当前文件集合一致。

#### Scenario: 静态残留检查
- **WHEN** 对工程执行功能宏、删除文件名和删除符号搜索
- **THEN** 只允许两个互斥业务宏，且不得存在指向已删除 ADC 或旧 LCD UI 模块的引用

#### Scenario: 工程验证
- **WHEN** 完成本次移植
- **THEN** Keil 全量构建与 `openspec validate --all --strict` 均应通过

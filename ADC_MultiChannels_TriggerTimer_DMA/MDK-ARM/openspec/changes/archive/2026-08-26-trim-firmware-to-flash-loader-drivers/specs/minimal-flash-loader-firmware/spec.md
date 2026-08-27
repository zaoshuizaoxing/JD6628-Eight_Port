## ADDED Requirements

### Requirement: 单一 Flash 烧录业务
固件 SHALL 仅保留 UART Flash Loader 作为产品业务逻辑，并 SHALL 使用 `ENABLE_UART_FLASH_LOADER` 作为唯一应用功能宏开关。

#### Scenario: 启用 Flash 烧录模式
- **WHEN** `ENABLE_UART_FLASH_LOADER` 被配置为启用
- **THEN** 系统初始化 W25Q 与 UART 后持续执行 UART Flash Loader，且不启动 LCD UI、ADC 或 I²C 轮询业务

#### Scenario: 关闭 Flash 烧录模式
- **WHEN** `ENABLE_UART_FLASH_LOADER` 被配置为关闭
- **THEN** 系统可初始化保留的外设驱动，但主循环不得执行已删除的产品业务

### Requirement: 保留外设驱动层
固件 SHALL 保留 W25Q/SPI1、ST7789/SPI2、软件 I²C、UART、GPIO 与 SPI所需DMA驱动能力及现有板级资源映射。

#### Scenario: 驱动接口可编译
- **WHEN** Keil 工程执行全量构建
- **THEN** W25Q、ST7789、软件 I²C、UART、SPI、GPIO 与 DMA 驱动源文件和公开接口应成功编译链接

#### Scenario: SPI DMA资源保持
- **WHEN** ADC 链路被移除
- **THEN** DMA1 Channel2/3 及其 SPI1/SPI2 映射和中断处理仍应保留

### Requirement: 删除 ADC 全链路
固件 MUST 不再包含或编译 ADC 应用模块，并 MUST 不再初始化、调用或处理中断形式的 ADC、TIM1触发及 DMA1 Channel1 采集链路。

#### Scenario: 检查 ADC 残留
- **WHEN** 搜索当前应用源码、头文件、MSP、中断文件和 Keil 工程清单
- **THEN** 不得存在 `APP_ADC`、ADC句柄、ADC回调、TIM1采样句柄、DMA1 Channel1 ADC处理或 ADC HAL编译项

### Requirement: 删除 LCD 产品业务
固件 MUST 删除 LCD UI、测试图片和依赖 Flash图片地址、数字资源或覆盖层策略的 LCD/W25Q 组合业务，同时 SHALL 保留纯 ST7789 驱动与基础绘图接口。

#### Scenario: 检查 LCD 业务残留
- **WHEN** 搜索当前应用源码、头文件和 Keil 工程清单
- **THEN** 不得存在 `APP_LCD_UI` 文件或调用、LCD测试流程、Flash图片描述符及覆盖层业务接口

#### Scenario: LCD 驱动继续存在
- **WHEN** 后续代码包含 `lcd_spi.h`
- **THEN** ST7789 初始化、SPI数据传输、窗口设置和基础绘图接口仍可用

### Requirement: I²C 驱动与业务解耦
软件 I²C 模块 SHALL 保留初始化和寄存器读写能力，但 MUST 删除周期轮询、电量状态、产品寄存器策略和 ADC 功率补偿逻辑。

#### Scenario: I²C底层访问
- **WHEN** 调用软件 I²C 初始化后对有效端口执行寄存器读写
- **THEN** 驱动应使用现有开漏GPIO端口与起止、字节收发时序完成事务

#### Scenario: 检查 I²C业务残留
- **WHEN** 搜索 I²C 模块的公开和私有符号
- **THEN** 不得存在 I²C轮询处理、端口业务处理、电量/状态全局变量或 ADC 符号依赖

### Requirement: 清理功能宏与工程依赖
固件 MUST 删除 Flash 烧录开关以外的产品功能宏开关，并 MUST 使公共头文件、HAL模块配置、MSP、IRQ和 Keil 编译清单与裁剪后的文件集合一致。

#### Scenario: 静态残留检查
- **WHEN** 对工程执行功能宏、删除文件名和删除符号搜索
- **THEN** 只允许 `ENABLE_UART_FLASH_LOADER` 作为应用功能开关，且不得存在指向已删除模块的引用

#### Scenario: 工程验证
- **WHEN** 完成裁剪
- **THEN** Keil 全量构建与 `openspec validate --all --strict` 均应通过

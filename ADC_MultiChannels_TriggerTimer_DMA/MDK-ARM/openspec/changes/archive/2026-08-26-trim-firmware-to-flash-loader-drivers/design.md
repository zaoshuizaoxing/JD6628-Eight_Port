## Context

当前 PY32F031 Keil 工程将产品业务和驱动直接聚合在 `main.c`、`APP_I2C.c` 与 `lcd_spi.c` 中。ADC 使用 TIM1 触发与 DMA1 Channel1；W25Q 的 SPI1 使用 DMA1 Channel2/3；LCD 的 SPI2 复用 DMA1 Channel3。Flash 烧录通过 USART1 接收数据并写入 W25Q。裁剪必须保留后续开发所需驱动接口与现有引脚/DMA映射，同时移除已废弃业务及其编译依赖。

约束如下：

- 源码继续采用 GB2312，现有中文注释乱码不作为本次修复范围。
- 不改动 LCD、W25Q、UART Flash Loader 的对外协议与底层硬件映射。
- 整个工程当前未纳入 Git 跟踪，无法依靠历史版本回滚，实施前后以文件清单、OpenSpec 与构建产物作为证据。

## Goals / Non-Goals

**Goals:**

- 运行时只保留 UART Flash 烧录这一项产品业务。
- 保留 W25Q/SPI1、ST7789/SPI2、软件 I²C、UART、GPIO、DMA 等驱动能力。
- 完整移除 ADC、ADC专用TIM1/DMA1 Channel1 链路和 LCD UI/Flash图片组合业务。
- 仅保留 `ENABLE_UART_FLASH_LOADER` 作为应用功能宏开关；其它功能开关按当前有效配置固化或连同已删除功能一起移除。
- 使 Keil 工程清单、HAL模块配置、头文件与中断/MSP实现保持一致。

**Non-Goals:**

- 不修改 UART Flash 帧格式、校验、超时、擦写策略或 PC 端烧录工具。
- 不重新设计 LCD/W25Q/I²C 驱动 API，不改变现有板级引脚。
- 不删除共享 vendor HAL 源码文件，只从当前 Keil 工程和 HAL 模块配置中移除不再使用的 ADC/TIM 驱动。
- 不处理注释编码乱码，不进行无关格式化或重构。

## Decisions

1. **主循环按唯一业务宏隔离。** 系统时钟、W25Q 和 UART 始终初始化；启用 `ENABLE_UART_FLASH_LOADER` 时只进入烧录业务。关闭时初始化保留的 I²C、SPI2 与 ST7789 驱动后空闲，不恢复任何旧产品业务。相比删除宏并永久进入烧录模式，此方案满足保留烧录宏开关的要求，也保留驱动联调入口。

2. **按“驱动是否依赖产品数据”划分 LCD 边界。** 保留 ST7789 初始化、SPI传输、窗口、清屏、像素、矩形与 RAM 位图等纯驱动/基础绘图接口；删除依赖 W25Q 图片地址、数字资源、覆盖层和页面策略的组合逻辑。这样 LCD 与 W25Q 驱动彼此解耦，后续业务可重新组合。

3. **在原文件内提纯软件 I²C 驱动。** 保留多端口引脚表、开漏初始化、START/STOP、字节收发及寄存器读写 API；删除定时轮询、电量值/状态全局变量、寄存器业务常量、端口业务处理和 ADC 功率补偿。保留文件名与核心 API 可降低后续迁移成本。

4. **ADC 链路整体退出当前工程。** 删除 `APP_ADC.c/.h`，移除 ADC/TIM HAL 编译项和模块定义，删除 ADC MSP、DMA1 Channel1 IRQ、TIM1 MSP及所有句柄/回调引用。DMA HAL 仍保留，因为 W25Q/LCD SPI 驱动继续使用 DMA1 Channel2/3。

5. **功能开关按固定配置展开。** 删除 `APP_ADC_RESULT_ENABLE`、`APP_I2C_ENABLE`、`APP_LCD_UI_ENABLE`、LCD测试开关、LCD DMA/复位/背光有效电平开关、Flash图片DMA开关及 W25Q checked-write 开关。保留功能所需分支按当前硬件配置固化；关闭状态对应的未使用变量和函数直接删除。HAL vendor 模块选择、头文件保护、断言和硬件参数宏不属于产品业务功能开关，继续保留。

## Risks / Trade-offs

- [LCD/W25Q 组合接口被历史代码调用] → 对全工程执行符号搜索，确保调用方仅为待删除 UI/测试业务，并在构建时检查未解析符号。
- [删除 ADC 后误删 SPI DMA] → 只移除 DMA1 Channel1 和 ADC/TIM对象，明确保留 DMA1 Channel2/3、SPI1/SPI2映射与IRQ。
- [关闭烧录宏后主循环空闲] → 这是有意设计，作为后续驱动开发基线，不暗中执行旧业务。
- [GB2312 文件被工具改写编码] → 仅对必要源文件做局部修改，并在实施后检查其编码可按代码页936解码。
- [无 Git 历史导致回滚困难] → OpenSpec归档记录精确变更；删除目标在实施前解析绝对路径并逐项核对。

## Migration Plan

1. 创建并校验 OpenSpec 提案、规格、设计与任务。
2. 先清理应用入口与混合业务，再删除 ADC/LCD UI 文件。
3. 同步清理 MSP、IRQ、HAL配置和 Keil 工程清单。
4. 搜索已删除宏、符号、文件引用并执行 Keil 全量构建。
5. 严格校验 OpenSpec、同步主规格后归档。

回滚时应从量产工程备份恢复本次删除的业务文件和对应工程清单；由于当前工程未受 Git 跟踪，不能依赖 `git checkout` 回滚。

## Open Questions

无。当前需求足以确定裁剪边界；真实硬件上的 UART 烧录、LCD、W25Q 与 I²C 信号验证留作板级验收。

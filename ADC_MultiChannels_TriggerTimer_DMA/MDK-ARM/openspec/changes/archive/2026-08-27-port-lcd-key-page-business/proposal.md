## Why

裁剪后的固件已经形成可复用的外设驱动基线，现在需要恢复量产工程中由按键切换的 LCD 页面业务，以验证 W25Q 图像资源和 ST7789 显示链路。示例工程来自另一 MCU 平台，因此必须只迁移业务策略，并继续使用当前 PY32F031 的引脚、SPI 和驱动实现。

## What Changes

- 新增独立的 LCD 按键页面业务模块，承载启动图、动画、数据页、温度页、累计时间页、黑屏页及数字叠加逻辑。
- 新增 `ENABLE_LCD_KEY_PAGE_MODE` 业务宏，并与原有 `ENABLE_UART_FLASH_LOADER` 建立编译期互斥；原烧录宏的定义、位置和值保持不变。
- 页面按键从示例工程的 PB3 改为当前空闲的 PB2，高电平有效并使用下拉输入。
- 本阶段不执行 I2C 读取，页面使用固定零值业务快照；I2C 驱动和初始化继续保留。
- W25Q 到 LCD 的图像传输改为分块阻塞读取后再调用现有 LCD DMA 发送，避免 SPI1 和 SPI2 同时争用 DMA1 Channel3。
- 不迁移示例工程的时钟、UART、USI/SPI、I2C 初始化及板级引脚配置。

## Capabilities

### New Capabilities

- `lcd-key-page-business`: 定义 LCD 页面状态机、PB2 按键切换、零数据快照、W25Q 资源显示及共享 DMA 串行访问要求。

### Modified Capabilities

- `minimal-flash-loader-firmware`: 将“仅允许 Flash 烧录业务”调整为 Flash 烧录与 LCD 页面业务可编译选择但不得同时运行，并保留原烧录宏不变。

## Impact

- 新增应用层源文件 `APP_LCD_KeyPage.c/.h`，并加入 Keil Application 编译组。
- `main.c` 和 `main.h` 仅增加业务头文件、互斥检查及宏控制的单一入口调用。
- 继续复用 `w25qxx`、`lcd_spi`、软件 I2C、SPI1/SPI2 和 DMA 驱动，不改变其公开接口及板级映射。
- 固件新增 4096 字节静态图像缓冲区和 Flash 常量资源表，需通过链接 MAP 校验 ROM/RAM 边界。

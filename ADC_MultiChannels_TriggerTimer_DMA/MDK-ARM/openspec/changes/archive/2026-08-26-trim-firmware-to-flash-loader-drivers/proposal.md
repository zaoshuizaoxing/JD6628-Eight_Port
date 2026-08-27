## Why

现有量产固件同时包含 ADC 采集、LCD 界面、电量轮询等已不再需要的产品业务，继续在此基础上开发会增加耦合、资源占用和误触发风险。现需将工程收敛为 Flash 烧录单一业务入口与可复用外设驱动基线，供后续需求重新开发。

## What Changes

- **BREAKING** 删除 ADC 应用、ADC HAL 编译项、ADC/DMA1 Channel1 初始化与中断入口。
- **BREAKING** 删除 LCD UI 文件及其初始化、刷新、测试图片显示等业务调用，但保留 ST7789/LCD SPI 驱动、SPI2 初始化和公开驱动接口。
- **BREAKING** 删除 I²C 电量轮询、端口状态和 ADC 功率补偿业务，仅保留软件 I²C 初始化与寄存器读写驱动接口。
- 保留 W25Q 驱动、SPI1/DMA 驱动资源、UART Flash 烧录业务以及其唯一功能宏 `ENABLE_UART_FLASH_LOADER`。
- 删除 Flash 烧录宏以外的业务功能宏开关及其专属变量、函数和条件分支。
- 更新 Keil 工程编译清单、公共头文件、MSP 与中断文件，消除已删除模块的引用。

## Capabilities

### New Capabilities

- `minimal-flash-loader-firmware`: 规定裁剪后固件只运行 UART-to-W25Q Flash 烧录业务，同时保留 LCD、W25Q、SPI、I²C、UART 等外设驱动层。

### Modified Capabilities

无。

## Impact

- 应用层：`main.c`、UART Flash Loader 入口及功能宏组织。
- 删除文件：`APP_ADC.c/.h`、`APP_LCD_UI.c/.h`。
- 驱动层：裁剪 `APP_I2C.c/.h` 中的业务耦合，保留软件 I²C 驱动；保留 `lcd_spi.c/.h`、`w25qxx.c/.h`。
- 平台层：`py32f031_hal_msp.c`、`py32f031_it.c/.h` 和 `Project.uvprojx`。
- 兼容性：ADC 与 LCD UI 的应用 API 不再可用；硬件驱动 API 原则上保持不变。

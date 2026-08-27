## 1. 应用入口裁剪

- [x] 1.1 将 `main.c` 裁剪为唯一 Flash Loader 业务入口，并删除调试变量、ADC/LCD UI/测试/I²C轮询流程及其宏
- [x] 1.2 清理 `main.h` 的已删除模块依赖，同时保留 W25Q、UART、LCD 和 I²C 驱动接口

## 2. 业务与驱动分层

- [x] 2.1 删除 `APP_ADC.c/.h` 与 `APP_LCD_UI.c/.h`
- [x] 2.2 提纯 `APP_I2C.c/.h`，仅保留软件 I²C 初始化、端口配置及寄存器读写驱动
- [x] 2.3 提纯 `lcd_spi.c/.h`，删除 W25Q图片、数字资源和覆盖层组合业务，保留 ST7789 驱动
- [x] 2.4 删除 W25Q checked-write 功能开关及其禁用分支、缓冲区和函数
- [x] 2.5 固化保留驱动的当前硬件配置，删除 Flash Loader 以外的应用/驱动功能开关

## 3. 平台与工程清理

- [x] 3.1 从 MSP 与 IRQ 文件删除 ADC、TIM1 和 DMA1 Channel1 资源，保留 SPI DMA1 Channel2/3
- [x] 3.2 从 HAL 配置和 Keil 工程清单移除 ADC/TIM模块及已删除源码引用，保留 I²C/SPI/UART/GPIO/DMA驱动

## 4. 验证与交付

- [x] 4.1 搜索已删除文件、宏、变量、函数和外设资源引用，确认没有陈旧依赖
- [x] 4.2 检查修改后的 C/H 文件可按 GB2312 解码
- [x] 4.3 执行 Keil 全量构建并检查 Flash/RAM 结果
- [x] 4.4 执行 `openspec validate --all --strict` 并记录板级验证缺口

板级验证缺口：本次仅完成静态检查与 Keil 构建，仍需在实板验证 UART Flash 烧录协议、W25Q 擦写、LCD SPI/DMA显示、软件 I²C 三端口波形与异常恢复。

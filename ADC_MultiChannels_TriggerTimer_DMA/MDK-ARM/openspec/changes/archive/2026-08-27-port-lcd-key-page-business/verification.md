## 自动验证

- 2026-08-27 使用 Keil ARMCC V5.06 update 7 对 `Project.uvprojx` 执行全量重建。
- 构建结果：0 Error(s)，0 Warning(s)。
- 镜像占用：Code 15940 B，RO-data 1780 B，RW-data 104 B，ZI-data 5744 B。
- MAP 汇总：Total ROM 17824 B，Total RW 5848 B；均未超过工程配置的 32 KB Flash / 8 KB RAM。
- 示例与新模块的 108 个 Flash 资源起始地址逐项一致，最大资源槽结束地址为 `0x0071CFFF`，低于 W25Q64 的 `0x00800000` 容量边界。
- 新业务模块未引用 I2C、USI、示例时钟初始化、W25Q DMA 或 SPI DMA 回调。
- `ENABLE_UART_FLASH_LOADER` 保持为 0；PB3 继续用于 W25Q SPI1 SCK，PB2 用于页面按键。
- `openspec validate --all --strict`：2 项通过，0 项失败。

## 实板验收待办

- [ ] 启动图显示 4 秒后进入笑脸动画，方向和颜色正确。
- [ ] PB2 每次按下只切换一页，长按不连续翻页。
- [ ] 八路数据页显示 8 路零值，温度页显示低温资源，累计时间显示 0。
- [ ] 黑屏页关闭背光，下一次按键恢复笑脸页面。
- [ ] 连续切页和动画过程中无花屏、错位、死机或 Flash 读取异常。

实板项目未连接到当前执行环境，因此以上板级项目不作为本次源码归档的自动验证项，需烧录后人工确认。

## 1. DMA Resource Ownership

- [x] 1.1 Assign LCD SPI2 TX permanently to DMA1 Channel1 in normal mode and add its IRQ handler.
- [x] 1.2 Keep W25Q SPI1 TX/RX assigned to DMA1 Channels2/3 without runtime channel remapping.

## 2. Flash-to-LCD Transport

- [x] 2.1 Move the single 4096-byte image buffer and transport state into `lcd_spi.c` behind `ST7789_WriteFlashData()`.
- [x] 2.2 Implement the SPI1 fast-read header, RX-only full-block DMA, and half/full completion publication boundaries.
- [x] 2.3 Add finite timeout/error cleanup, SPI1 direction and CS restoration, and blocking-read fallback for uncommitted data.
- [x] 2.4 Refactor `APP_LCD_KeyPage` to request transport by Flash address and valid length without changing page business behavior.

## 3. Board-Stable Configuration

- [x] 3.1 Set LCD SPI2 to the `/4` baud prescaler while retaining W25Q SPI1 at `/2`.
- [x] 3.2 Preserve the original PB2 pull-down, active-high, 50 ms stable and 150 ms repeat-guard key implementation.

## 4. Verification

- [x] 4.1 Complete a Keil full rebuild with 0 errors and 0 warnings; confirm RAM usage remains within the PY32F031 8 KiB limit.
- [x] 4.2 Verify on the target board that images display without white-screen or corruption faults and refresh performance is acceptable.
- [x] 4.3 Verify on the target board that PB2 advances every page once per valid press, including recovery from the black page.

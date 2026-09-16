## Why

The optimized W25Q-to-ST7789 path improved throughput but produced corrupted or white frames when DMA ownership, buffer publication, and SPI direction changes did not match the proven product transfer boundaries. The final implementation also needs to preserve the existing PB2 page-key behavior while delivering stable image refresh on the target board.

## What Changes

- Move Flash-to-LCD streaming ownership into the LCD transport driver behind `ST7789_WriteFlashData()`.
- Use one static 4096-byte buffer, start a complete SPI1 RX DMA transaction, and expose its two halves to LCD transmission only at the DMA half/full completion boundaries.
- Assign DMA1 Channel1 permanently to SPI2 TX and retain Channels2/3 for SPI1 TX/RX so the LCD path does not remap an active Flash channel.
- Restore SPI1 direction and Flash chip-select state after success, timeout, abort, or error, with blocking Flash-read fallback for incomplete data.
- Run SPI2 at the board-proven `/4` baud prescaler while retaining SPI1 at `/2`.
- Preserve the existing PB2 pull-down, active-high, 50 ms stable and 150 ms repeat-guard page-key behavior.

## Capabilities

### New Capabilities

None.

### Modified Capabilities

- `lcd-key-page-business`: Replace the obsolete shared-Channel3 serialized-transfer requirement with the final fixed-channel, single-buffer Flash-to-LCD transport contract and explicitly preserve the verified PB2 key contract.

## Impact

- Affected firmware: `lcd_spi.c/.h`, SPI/DMA MSP and IRQ setup, SPI2 initialization, and `APP_LCD_KeyPage.c` transport integration.
- Public application behavior and page order remain unchanged.
- Static RAM includes one 4096-byte LCD transport buffer; no second image buffer is added.
- Verification requires a clean Keil rebuild plus target-board checks for correct images, acceptable refresh, and page switching.

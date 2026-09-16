## Context

The PY32F031 target has three programmable DMA channels and 8 KiB SRAM. Earlier display optimizations either remapped a channel while Flash DMA was active or exposed partially owned ping-pong buffers, causing white screens or corrupted RGB565 data. The final board-tested path must coexist with the migrated page business logic and its original active-high PB2 key.

## Goals / Non-Goals

**Goals:**

- Keep DMA request ownership fixed for the lifetime of each SPI peripheral.
- Limit image staging to one 4096-byte static buffer.
- Match the known-good 4 KiB Flash transaction and half/full callback boundaries.
- Restore all SPI1 state on every exit path and retain a blocking fallback.
- Use the board-tested SPI2 clock and preserve the original page/key behavior.

**Non-Goals:**

- Changing page order, image addresses, overlay coordinates, I2C business behavior, or PB2 key semantics.
- Adding RTOS scheduling, dynamic allocation, LCD receive DMA, or circular LCD DMA.
- Maximizing benchmark throughput at the expense of stable board output.

## Decisions

1. **Fixed DMA ownership.** DMA1 Channel1 is permanently mapped to SPI2 TX; Channels2 and 3 remain mapped to SPI1 TX and RX. This avoids request remapping during an active transaction. The alternative—sharing Channel3 between Flash RX and LCD TX—was rejected because it created fragile ownership boundaries and image corruption.

2. **One driver-owned 4 KiB buffer.** `lcd_spi.c` owns a single sector-sized buffer and the application passes only Flash address and visible length. This keeps DMA flags, SPI direction, chip-select, timeout, and fallback in one layer. Application-owned double buffers were rejected because they duplicated transport state and exceeded the proven completion model.

3. **One complete Flash RX DMA per chunk.** SPI1 sends the fast-read header in normal two-line mode, changes to two-line RX-only for the payload, and receives up to 4096 bytes. The half-complete flag permits transmission of the first 2048 bytes; the full-complete flag publishes the second half. No half is sent before its DMA completion boundary.

4. **Bounded recovery.** A 1000 ms timeout or SPI DMA error aborts the active transfer, releases Flash CS, restores SPI1 to normal two-line mode, and obtains any uncommitted bytes through blocking `W25QXX_Read()` before display transmission.

5. **Board-stable LCD clock.** SPI2 uses the `/4` prescaler (approximately 18 MHz from the 72 MHz peripheral clock); SPI1 remains `/2`. The faster LCD setting was rejected after target testing showed unstable image output.

6. **Key behavior is preserved.** PB2 remains input pull-down and active-high, with 50 ms stable time and 150 ms repeat guard. The active-low key implementation from another finished product was explicitly rejected after board comparison.

## Risks / Trade-offs

- **Lower SPI2 clock reduces peak bandwidth** → The single-buffer half/full schedule still improves useful frame pacing while prioritizing correct output.
- **Polling callback flags occupies the main context during a transfer** → Transfers are bounded by timeout and remain compatible with the existing bare-metal page loop.
- **DMA failure falls back to slower blocking reads** → The fallback preserves correct frames and restores peripheral state instead of leaving the display corrupted.
- **A 4096-byte static buffer consumes half of nominal SRAM** → The link map remains within the 8 KiB SRAM budget (`RW + ZI = 6016 bytes`).

## Migration Plan

1. Build the final firmware with fixed DMA mappings and the single driver-owned buffer.
2. Confirm the Keil full rebuild completes with zero errors and warnings.
3. Program the target and verify image integrity, refresh behavior, and all PB2 page transitions.
4. Roll back only the transport implementation if a board regression occurs; page business state and resource tables are independent.

## Open Questions

None. Display and key behavior were confirmed on the target board on 2026-09-16.

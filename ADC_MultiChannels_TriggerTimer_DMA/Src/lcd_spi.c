#include "lcd_spi.h"

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern DMA_HandleTypeDef HdmaCh2;
extern DMA_HandleTypeDef HdmaCh3;

#define ST7789_LINE_BUF_SIZE   128U
#define ST7789_DMA_TIMEOUT_MS  1000U

static uint8_t st7789_line_buf[ST7789_LINE_BUF_SIZE];
static volatile uint8_t st7789_dma_done = 1U;
static volatile uint8_t st7789_dma_error = 0U;

static void ST7789_WriteCommand(uint8_t cmd);
static void ST7789_WriteData(uint8_t data);
static HAL_StatusTypeDef LCD_MapDmaCh3ToSpi2Tx(void);
static void LCD_RestoreW25QDmaMap(void);

static void ST7789_WriteCommand(uint8_t cmd)
{
  ST7789_SendByte(cmd, ST7789_CMD);
}

static void ST7789_WriteData(uint8_t data)
{
  ST7789_SendByte(data, ST7789_DATA);
}

void ST7789_SendByte(uint8_t dat, ST7789_DCType dc)
{
  ST7789_CS_LOW();
  if (dc == ST7789_DATA)
  {
    ST7789_DC_HIGH();
  }
  else
  {
    ST7789_DC_LOW();
  }
  (void)HAL_SPI_Transmit(&hspi2, &dat, 1U, 100U);
  ST7789_CS_HIGH();
}

void ST7789_SendHalfWord(uint16_t dat)
{
  uint8_t bytes[2];

  bytes[0] = (uint8_t)(dat >> 8);
  bytes[1] = (uint8_t)dat;
  ST7789_SendMultiByte(bytes, 2U);
}

void ST7789_SendMultiByte(const uint8_t *dat, uint16_t len)
{
  if ((dat == NULL) || (len == 0U))
  {
    return;
  }

  ST7789_CS_LOW();
  ST7789_DC_HIGH();
  (void)HAL_SPI_Transmit(&hspi2, (uint8_t *)dat, len, 1000U);
  ST7789_CS_HIGH();
}

void ST7789_BeginWrite(void)
{
  ST7789_CS_LOW();
  ST7789_DC_HIGH();
}

void ST7789_EndWrite(void)
{
  ST7789_CS_HIGH();
}

void ST7789_WriteDataBlock(const uint8_t *dat, uint16_t len)
{
  HAL_StatusTypeDef map_status;

  if ((dat == NULL) || (len == 0U))
  {
    return;
  }

  st7789_dma_done = 0U;
  st7789_dma_error = 0U;
  map_status = LCD_MapDmaCh3ToSpi2Tx();

  if ((map_status == HAL_OK) &&
      (HAL_SPI_Transmit_DMA(&hspi2, (uint8_t *)dat, len) == HAL_OK))
  {
    uint32_t start_tick = HAL_GetTick();

    while (st7789_dma_done == 0U)
    {
      if ((HAL_GetTick() - start_tick) > ST7789_DMA_TIMEOUT_MS)
      {
        (void)HAL_SPI_Abort(&hspi2);
        st7789_dma_done = 1U;
        st7789_dma_error = 1U;
      }
    }

    LCD_RestoreW25QDmaMap();
    if (st7789_dma_error == 0U)
    {
      return;
    }
  }
  else if (map_status == HAL_OK)
  {
    LCD_RestoreW25QDmaMap();
  }

  st7789_dma_done = 1U;
  (void)HAL_SPI_Transmit(&hspi2, (uint8_t *)dat, len, 1000U);
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi == &hspi2)
  {
    st7789_dma_done = 1U;
  }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi == &hspi2)
  {
    st7789_dma_error = 1U;
    st7789_dma_done = 1U;
  }
}

void ST7789_Init(void)
{
  ST7789_BL_ON();
  ST7789_CS_HIGH();
  ST7789_DC_LOW();

  ST7789_RST_HIGH();
  HAL_Delay(5U);
  ST7789_RST_LOW();
  HAL_Delay(20U);
  ST7789_RST_HIGH();
  HAL_Delay(120U);

  ST7789_WriteCommand(0x01U);
  HAL_Delay(120U);
  ST7789_WriteCommand(0x11U);
  HAL_Delay(120U);

  ST7789_WriteCommand(0x3AU);
  ST7789_WriteData(0x55U);

  ST7789_WriteCommand(0xB2U);
  ST7789_WriteData(0x0CU);
  ST7789_WriteData(0x0CU);
  ST7789_WriteData(0x00U);
  ST7789_WriteData(0x33U);
  ST7789_WriteData(0x33U);

  ST7789_WriteCommand(0xB7U);
  ST7789_WriteData(0x35U);
  ST7789_WriteCommand(0xBBU);
  ST7789_WriteData(0x32U);
  ST7789_WriteCommand(0xC2U);
  ST7789_WriteData(0x01U);
  ST7789_WriteCommand(0xC3U);
  ST7789_WriteData(0x19U);
  ST7789_WriteCommand(0xC4U);
  ST7789_WriteData(0x20U);
  ST7789_WriteCommand(0xC6U);
  ST7789_WriteData(0x0FU);

  ST7789_WriteCommand(0xD0U);
  ST7789_WriteData(0xA4U);
  ST7789_WriteData(0xA1U);

  ST7789_WriteCommand(0xE0U);
  ST7789_WriteData(0xD0U);
  ST7789_WriteData(0x08U);
  ST7789_WriteData(0x0EU);
  ST7789_WriteData(0x09U);
  ST7789_WriteData(0x09U);
  ST7789_WriteData(0x05U);
  ST7789_WriteData(0x31U);
  ST7789_WriteData(0x33U);
  ST7789_WriteData(0x48U);
  ST7789_WriteData(0x17U);
  ST7789_WriteData(0x14U);
  ST7789_WriteData(0x15U);
  ST7789_WriteData(0x31U);
  ST7789_WriteData(0x34U);

  ST7789_WriteCommand(0xE1U);
  ST7789_WriteData(0xD0U);
  ST7789_WriteData(0x08U);
  ST7789_WriteData(0x0EU);
  ST7789_WriteData(0x09U);
  ST7789_WriteData(0x09U);
  ST7789_WriteData(0x15U);
  ST7789_WriteData(0x31U);
  ST7789_WriteData(0x33U);
  ST7789_WriteData(0x48U);
  ST7789_WriteData(0x17U);
  ST7789_WriteData(0x14U);
  ST7789_WriteData(0x15U);
  ST7789_WriteData(0x31U);
  ST7789_WriteData(0x34U);

  ST7789_WriteCommand(0x21U);
  ST7789_WriteCommand(0x36U);
#if (ST7789_ROTATION == 0U)
  ST7789_WriteData(0x00U);
#elif (ST7789_ROTATION == 90U)
  ST7789_WriteData(0x60U);
#elif (ST7789_ROTATION == 180U)
  ST7789_WriteData(0xC0U);
#elif (ST7789_ROTATION == 270U)
  ST7789_WriteData(0xA0U);
#else
  ST7789_WriteData(0x00U);
#endif

  ST7789_SetFullWindow();
  ST7789_WriteCommand(0x29U);
  HAL_Delay(20U);
}

void ST7789_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
  ST7789_WriteCommand(0x2AU);
  ST7789_SendHalfWord(x1);
  ST7789_SendHalfWord(x2);
  ST7789_WriteCommand(0x2BU);
  ST7789_SendHalfWord(y1);
  ST7789_SendHalfWord(y2);
  ST7789_WriteCommand(0x2CU);
}

void ST7789_SetFullWindow(void)
{
  ST7789_Address_Set(0U,
                     0U,
                     (uint16_t)(ST7789_WIDTH - 1U),
                     (uint16_t)(ST7789_HEIGHT - 1U));
}

void ST7789_Clear(uint16_t color)
{
  uint16_t i;
  uint32_t bytes;
  uint32_t full_blocks;
  uint16_t tail_bytes;

  for (i = 0U; i < ST7789_LINE_BUF_SIZE; i += 2U)
  {
    st7789_line_buf[i] = (uint8_t)(color >> 8);
    st7789_line_buf[i + 1U] = (uint8_t)color;
  }

  bytes = (uint32_t)ST7789_WIDTH * (uint32_t)ST7789_HEIGHT * ST7789_PIXEL_BYTES;
  full_blocks = bytes / ST7789_LINE_BUF_SIZE;
  tail_bytes = (uint16_t)(bytes % ST7789_LINE_BUF_SIZE);

  ST7789_SetFullWindow();
  ST7789_BeginWrite();
  while (full_blocks > 0U)
  {
    ST7789_WriteDataBlock(st7789_line_buf, ST7789_LINE_BUF_SIZE);
    full_blocks--;
  }
  if (tail_bytes > 0U)
  {
    ST7789_WriteDataBlock(st7789_line_buf, tail_bytes);
  }
  ST7789_EndWrite();
}

void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
  if ((x >= ST7789_WIDTH) || (y >= ST7789_HEIGHT))
  {
    return;
  }

  ST7789_Address_Set(x, y, x, y);
  ST7789_SendHalfWord(color);
}

void ST7789_FillRect(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color)
{
  uint16_t i;
  uint32_t pixels;
  uint32_t full_blocks;
  uint16_t tail_bytes;

  if ((xs >= ST7789_WIDTH) || (ys >= ST7789_HEIGHT) || (xe < xs) || (ye < ys))
  {
    return;
  }
  if (xe >= ST7789_WIDTH)
  {
    xe = (uint16_t)(ST7789_WIDTH - 1U);
  }
  if (ye >= ST7789_HEIGHT)
  {
    ye = (uint16_t)(ST7789_HEIGHT - 1U);
  }

  for (i = 0U; i < ST7789_LINE_BUF_SIZE; i += 2U)
  {
    st7789_line_buf[i] = (uint8_t)(color >> 8);
    st7789_line_buf[i + 1U] = (uint8_t)color;
  }

  pixels = (uint32_t)(xe - xs + 1U) * (uint32_t)(ye - ys + 1U);
  full_blocks = (pixels * ST7789_PIXEL_BYTES) / ST7789_LINE_BUF_SIZE;
  tail_bytes = (uint16_t)((pixels * ST7789_PIXEL_BYTES) % ST7789_LINE_BUF_SIZE);

  ST7789_Address_Set(xs, ys, xe, ye);
  ST7789_BeginWrite();
  while (full_blocks > 0U)
  {
    ST7789_WriteDataBlock(st7789_line_buf, ST7789_LINE_BUF_SIZE);
    full_blocks--;
  }
  if (tail_bytes > 0U)
  {
    ST7789_WriteDataBlock(st7789_line_buf, tail_bytes);
  }
  ST7789_EndWrite();
}

void ST7789_DrawBitmap(uint16_t xs,
                       uint16_t ys,
                       uint16_t xsize,
                       uint16_t ysize,
                       const uint8_t *p)
{
  uint32_t len;

  if ((p == NULL) || (xsize == 0U) || (ysize == 0U))
  {
    return;
  }
  if ((((uint32_t)xs + xsize) > ST7789_WIDTH) ||
      (((uint32_t)ys + ysize) > ST7789_HEIGHT))
  {
    return;
  }

  len = (uint32_t)xsize * (uint32_t)ysize * ST7789_PIXEL_BYTES;
  ST7789_Address_Set(xs,
                     ys,
                     (uint16_t)(xs + xsize - 1U),
                     (uint16_t)(ys + ysize - 1U));
  ST7789_BeginWrite();
  while (len > 0U)
  {
    uint16_t chunk = (len > 65535U) ? 65535U : (uint16_t)len;

    ST7789_WriteDataBlock(p, chunk);
    p += chunk;
    len -= chunk;
  }
  ST7789_EndWrite();
}

static HAL_StatusTypeDef LCD_MapDmaCh3ToSpi2Tx(void)
{
  if (hspi2.State != HAL_SPI_STATE_READY)
  {
    return HAL_BUSY;
  }

  HdmaCh3.Instance = DMA1_Channel3;
  HdmaCh3.Init.Direction = DMA_MEMORY_TO_PERIPH;
  HdmaCh3.Init.PeriphInc = DMA_PINC_DISABLE;
  HdmaCh3.Init.MemInc = DMA_MINC_ENABLE;
  HdmaCh3.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  HdmaCh3.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  HdmaCh3.Init.Mode = DMA_NORMAL;
  HdmaCh3.Init.Priority = DMA_PRIORITY_HIGH;
  (void)HAL_DMA_DeInit(&HdmaCh3);
  if (HAL_DMA_Init(&HdmaCh3) != HAL_OK)
  {
    return HAL_ERROR;
  }
  (void)HAL_DMA_ChannelMap(&HdmaCh3, DMA_CHANNEL_MAP_SPI2_TX);
  __HAL_LINKDMA(&hspi2, hdmatx, HdmaCh3);
  return HAL_OK;
}

static void LCD_RestoreW25QDmaMap(void)
{
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  CLEAR_BIT(hspi1.Instance->CR1, SPI_CR1_RXONLY | SPI_CR1_BIDIMODE);

  HdmaCh2.Instance = DMA1_Channel2;
  HdmaCh2.Init.Direction = DMA_MEMORY_TO_PERIPH;
  HdmaCh2.Init.PeriphInc = DMA_PINC_DISABLE;
  HdmaCh2.Init.MemInc = DMA_MINC_ENABLE;
  HdmaCh2.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  HdmaCh2.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  HdmaCh2.Init.Mode = DMA_NORMAL;
  HdmaCh2.Init.Priority = DMA_PRIORITY_HIGH;
  (void)HAL_DMA_DeInit(&HdmaCh2);
  (void)HAL_DMA_Init(&HdmaCh2);
  (void)HAL_DMA_ChannelMap(&HdmaCh2, DMA_CHANNEL_MAP_SPI1_TX);
  __HAL_LINKDMA(&hspi1, hdmatx, HdmaCh2);

  HdmaCh3.Instance = DMA1_Channel3;
  HdmaCh3.Init.Direction = DMA_PERIPH_TO_MEMORY;
  HdmaCh3.Init.PeriphInc = DMA_PINC_DISABLE;
  HdmaCh3.Init.MemInc = DMA_MINC_ENABLE;
  HdmaCh3.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  HdmaCh3.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  HdmaCh3.Init.Mode = DMA_NORMAL;
  HdmaCh3.Init.Priority = DMA_PRIORITY_HIGH;
  (void)HAL_DMA_DeInit(&HdmaCh3);
  (void)HAL_DMA_Init(&HdmaCh3);
  (void)HAL_DMA_ChannelMap(&HdmaCh3, DMA_CHANNEL_MAP_SPI1_RX);
  __HAL_LINKDMA(&hspi1, hdmarx, HdmaCh3);
}

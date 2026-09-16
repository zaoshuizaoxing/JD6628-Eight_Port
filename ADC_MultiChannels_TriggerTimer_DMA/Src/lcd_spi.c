#include "lcd_spi.h"
#include "w25qxx.h"

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;

DMA_HandleTypeDef HdmaCh1;

#define ST7789_LINE_BUF_SIZE   128U
#define ST7789_DMA_TIMEOUT_MS  1000U
#define LCD_FLASH_BUFFER_LEN   W25QXX_SECTOR_SIZE

//优化旋转方向
#if (ST7789_ROTATION == 180U)
#define ST7789_X_OFFSET         0U
#define ST7789_Y_OFFSET         80U
#elif (ST7789_ROTATION == 270U)
#define ST7789_X_OFFSET         80U
#define ST7789_Y_OFFSET         0U
#else
#define ST7789_X_OFFSET         0U
#define ST7789_Y_OFFSET         0U
#endif

static uint8_t st7789_line_buf[ST7789_LINE_BUF_SIZE];
static uint8_t lcd_flash_buffer[LCD_FLASH_BUFFER_LEN];
static volatile uint8_t st7789_dma_done = 1U;
static volatile uint8_t st7789_dma_error = 0U;
static volatile uint8_t lcd_flash_dma_active = 0U;
static volatile uint8_t lcd_flash_half_ready = 0U;
static volatile uint8_t lcd_flash_full_ready = 0U;
static volatile uint8_t lcd_flash_dma_error = 0U;

static void ST7789_WriteCommand(uint8_t cmd);
static void ST7789_WriteData(uint8_t data);
static uint16_t LCD_MinU16(uint32_t value, uint16_t limit);
static HAL_StatusTypeDef LCD_FlashReadDmaStart(uint8_t *buffer,
                                               uint32_t read_addr,
                                               uint16_t length);
static void LCD_FlashReadDmaEnd(void);

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
  if ((dat == NULL) || (len == 0U))
  {
    return;
  }

  if (ST7789_WriteDataBlockStart(dat, len) == HAL_OK)
  {
    if (ST7789_WriteDataBlockWait() == HAL_OK)
    {
      return;
    }
  }

  st7789_dma_done = 1U;
  (void)HAL_SPI_Transmit(&hspi2, (uint8_t *)dat, len, 1000U);
}

HAL_StatusTypeDef ST7789_WriteDataBlockStart(const uint8_t *dat, uint16_t len)
{
  if ((dat == NULL) || (len == 0U) || (st7789_dma_done == 0U))
  {
    return HAL_ERROR;
  }

  st7789_dma_done = 0U;
  st7789_dma_error = 0U;
  if (HAL_SPI_Transmit_DMA(&hspi2, (uint8_t *)dat, len) != HAL_OK)
  {
    st7789_dma_done = 1U;
    return HAL_ERROR;
  }

  return HAL_OK;
}

HAL_StatusTypeDef ST7789_WriteDataBlockWait(void)
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

  return (st7789_dma_error == 0U) ? HAL_OK : HAL_ERROR;
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi == &hspi2)
  {
    st7789_dma_done = 1U;
  }
}

void HAL_SPI_RxHalfCpltCallback(SPI_HandleTypeDef *hspi)
{
  if ((hspi == &hspi1) && (lcd_flash_dma_active != 0U))
  {
    lcd_flash_half_ready = 1U;
  }
}

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if ((hspi == &hspi1) && (lcd_flash_dma_active != 0U))
  {
    lcd_flash_full_ready = 1U;
  }
}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi == &hspi2)
  {
    st7789_dma_error = 1U;
    st7789_dma_done = 1U;
  }
  else if ((hspi == &hspi1) && (lcd_flash_dma_active != 0U))
  {
    lcd_flash_dma_error = 1U;
    lcd_flash_full_ready = 1U;
  }
}

static uint16_t LCD_MinU16(uint32_t value, uint16_t limit)
{
  return (value > limit) ? limit : (uint16_t)value;
}

static HAL_StatusTypeDef LCD_FlashReadDmaStart(uint8_t *buffer,
                                               uint32_t read_addr,
                                               uint16_t length)
{
  uint8_t command[5];
  HAL_StatusTypeDef status;

  if ((buffer == NULL) || (length == 0U) ||
      (hspi1.State != HAL_SPI_STATE_READY) ||
      (hspi2.State != HAL_SPI_STATE_READY) ||
      (read_addr >= W25QXX_FlashSizeBytes) ||
      ((uint32_t)length > (W25QXX_FlashSizeBytes - read_addr)))
  {
    return HAL_ERROR;
  }

  command[0] = W25X_FastReadData;
  command[1] = (uint8_t)(read_addr >> 16);
  command[2] = (uint8_t)(read_addr >> 8);
  command[3] = (uint8_t)read_addr;
  command[4] = 0xFFU;

  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  CLEAR_BIT(hspi1.Instance->CR1, SPI_CR1_RXONLY | SPI_CR1_BIDIMODE);

  W25QXX_CS_LOW();
  status = HAL_SPI_Transmit(&hspi1, command, sizeof(command), 100U);
  if (status != HAL_OK)
  {
    W25QXX_CS_HIGH();
    return status;
  }

  __HAL_SPI_DISABLE(&hspi1);
  hspi1.Init.Direction = SPI_DIRECTION_2LINES_RXONLY;
  MODIFY_REG(hspi1.Instance->CR1,
             SPI_CR1_RXONLY | SPI_CR1_BIDIMODE,
             SPI_DIRECTION_2LINES_RXONLY);

  status = HAL_SPI_Receive_DMA(&hspi1, buffer, length);
  if (status != HAL_OK)
  {
    LCD_FlashReadDmaEnd();
  }
  return status;
}

static void LCD_FlashReadDmaEnd(void)
{
  if (hspi1.State != HAL_SPI_STATE_READY)
  {
    (void)HAL_SPI_Abort(&hspi1);
  }

  W25QXX_CS_HIGH();
  __HAL_SPI_DISABLE(&hspi1);
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  CLEAR_BIT(hspi1.Instance->CR1, SPI_CR1_RXONLY | SPI_CR1_BIDIMODE);
}

HAL_StatusTypeDef ST7789_WriteFlashData(uint32_t read_addr, uint32_t length)
{
  uint32_t remaining = length;

  if ((length == 0UL) || (read_addr >= W25QXX_FlashSizeBytes) ||
      (length > (W25QXX_FlashSizeBytes - read_addr)))
  {
    return (length == 0UL) ? HAL_OK : HAL_ERROR;
  }

  while (remaining > 0UL)
  {
    uint16_t chunk_length = LCD_MinU16(remaining, LCD_FLASH_BUFFER_LEN);
    uint16_t first_length = (uint16_t)(chunk_length / 2U);
    uint16_t second_length = (uint16_t)(chunk_length - first_length);
    uint8_t first_sent = 0U;
    uint8_t dma_failed = 0U;
    uint32_t start_tick;

    lcd_flash_half_ready = 0U;
    lcd_flash_full_ready = 0U;
    lcd_flash_dma_error = 0U;
    lcd_flash_dma_active = 1U;

    if (LCD_FlashReadDmaStart(lcd_flash_buffer,
                              read_addr,
                              chunk_length) != HAL_OK)
    {
      lcd_flash_dma_active = 0U;
      W25QXX_Read(lcd_flash_buffer, read_addr, chunk_length);
      ST7789_WriteDataBlock(&lcd_flash_buffer[0], first_length);
      if (second_length > 0U)
      {
        ST7789_WriteDataBlock(&lcd_flash_buffer[first_length], second_length);
      }
    }
    else
    {
      start_tick = HAL_GetTick();
      while (lcd_flash_full_ready == 0U)
      {
        if ((lcd_flash_half_ready != 0U) && (first_sent == 0U))
        {
          ST7789_WriteDataBlock(&lcd_flash_buffer[0], first_length);
          first_sent = 1U;
        }
        if ((HAL_GetTick() - start_tick) > ST7789_DMA_TIMEOUT_MS)
        {
          dma_failed = 1U;
          break;
        }
      }

      if (lcd_flash_dma_error != 0U)
      {
        dma_failed = 1U;
      }

      LCD_FlashReadDmaEnd();
      lcd_flash_dma_active = 0U;

      if (dma_failed != 0U)
      {
        if (first_sent == 0U)
        {
          W25QXX_Read(lcd_flash_buffer, read_addr, chunk_length);
          ST7789_WriteDataBlock(&lcd_flash_buffer[0], first_length);
          if (second_length > 0U)
          {
            ST7789_WriteDataBlock(&lcd_flash_buffer[first_length], second_length);
          }
        }
        else if (second_length > 0U)
        {
          W25QXX_Read(&lcd_flash_buffer[first_length],
                      read_addr + first_length,
                      second_length);
          ST7789_WriteDataBlock(&lcd_flash_buffer[first_length], second_length);
        }
      }
      else
      {
        if (first_sent == 0U)
        {
          ST7789_WriteDataBlock(&lcd_flash_buffer[0], first_length);
        }
        if (second_length > 0U)
        {
          ST7789_WriteDataBlock(&lcd_flash_buffer[first_length], second_length);
        }
      }
    }

    read_addr += chunk_length;
    remaining -= chunk_length;
  }

  return HAL_OK;
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
  ST7789_SendHalfWord((uint16_t)(x1+ST7789_X_OFFSET));
  ST7789_SendHalfWord((uint16_t)(x2+ST7789_X_OFFSET));
  ST7789_WriteCommand(0x2BU);
  ST7789_SendHalfWord((uint16_t)(y1+ST7789_Y_OFFSET));
  ST7789_SendHalfWord((uint16_t)(y2+ST7789_Y_OFFSET));
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

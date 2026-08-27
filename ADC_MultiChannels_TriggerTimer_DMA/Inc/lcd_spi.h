#ifndef __LCD_SPI_H
#define __LCD_SPI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "py32f0xx_hal.h"

#define ST7789_WIDTH        240U
#define ST7789_HEIGHT       240U
#define ST7789_PIXEL_BYTES  2U
#define ST7789_FRAME_BYTES  (ST7789_WIDTH * ST7789_HEIGHT * ST7789_PIXEL_BYTES)
#define ST7789_ROTATION     0U

#define LCD_RST_GPIO_PORT  GPIOA
#define LCD_RST_PIN        GPIO_PIN_12
#define LCD_DC_GPIO_PORT   GPIOA
#define LCD_DC_PIN         GPIO_PIN_11
#define LCD_CS_GPIO_PORT   GPIOA
#define LCD_CS_PIN         GPIO_PIN_15
#define LCD_BL_GPIO_PORT   GPIOA
#define LCD_BL_PIN         GPIO_PIN_9

#define ST7789_RST_LOW()   HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_PIN, GPIO_PIN_RESET)
#define ST7789_RST_HIGH()  HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_PIN, GPIO_PIN_SET)
#define ST7789_CS_LOW()    HAL_GPIO_WritePin(LCD_CS_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_RESET)
#define ST7789_CS_HIGH()   HAL_GPIO_WritePin(LCD_CS_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_SET)
#define ST7789_DC_LOW()    HAL_GPIO_WritePin(LCD_DC_GPIO_PORT, LCD_DC_PIN, GPIO_PIN_RESET)
#define ST7789_DC_HIGH()   HAL_GPIO_WritePin(LCD_DC_GPIO_PORT, LCD_DC_PIN, GPIO_PIN_SET)
#define ST7789_BL_ON()     HAL_GPIO_WritePin(LCD_BL_GPIO_PORT, LCD_BL_PIN, GPIO_PIN_SET)
#define ST7789_BL_OFF()    HAL_GPIO_WritePin(LCD_BL_GPIO_PORT, LCD_BL_PIN, GPIO_PIN_RESET)

#define WHITE    0xFFFFU
#define BLACK    0x0000U
#define BLUE     0x001FU
#define RED      0xF800U
#define GREEN    0x07E0U
#define CYAN     0x7FFFU
#define MAGENTA  0xF81FU
#define YELLOW   0xFFE0U
#define GRAY     0x8430U

typedef enum
{
  ST7789_CMD = 0,
  ST7789_DATA = 1
} ST7789_DCType;

void ST7789_Init(void);
void ST7789_SendByte(uint8_t dat, ST7789_DCType dc);
void ST7789_SendHalfWord(uint16_t dat);
void ST7789_SendMultiByte(const uint8_t *dat, uint16_t len);
void ST7789_Address_Set(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void ST7789_BeginWrite(void);
void ST7789_EndWrite(void);
void ST7789_WriteDataBlock(const uint8_t *dat, uint16_t len);
void ST7789_SetFullWindow(void);
void ST7789_Clear(uint16_t color);
void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7789_FillRect(uint16_t xs, uint16_t ys, uint16_t xe, uint16_t ye, uint16_t color);
void ST7789_DrawBitmap(uint16_t xs,
                       uint16_t ys,
                       uint16_t xsize,
                       uint16_t ysize,
                       const uint8_t *p);

#ifdef __cplusplus
}
#endif

#endif /* __LCD_SPI_H */

#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "py32f0xx_hal.h"
#include "py32f031xx_Start_Kit.h"
#include "APP_I2C.h"

#define SPI_CS_GPIO_PORT GPIOB
#define SPI_CS_PIN       GPIO_PIN_6

#include "w25qxx.h"
#include "uart_flash_loader.h"
#include "lcd_spi.h"

extern UART_HandleTypeDef huart1;
extern SPI_HandleTypeDef hspi2;

void APP_ErrorHandler(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */

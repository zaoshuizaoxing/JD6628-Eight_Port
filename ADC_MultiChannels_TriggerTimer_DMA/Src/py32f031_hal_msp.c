/**
  ******************************************************************************
  * @file    py32f031_hal_msp.c
  * @author  MCU Application Team
  * @brief   This file provides code for the MSP Initialization
  *          and de-Initialization codes.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2023 Puya Semiconductor Co.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by Puya under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2016 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* External functions --------------------------------------------------------*/

/**
  * @brief Initialize global MSP.
  */
void HAL_MspInit(void)
{
  __HAL_RCC_SYSCFG_CLK_ENABLE();
  __HAL_RCC_PWR_CLK_ENABLE();
}

/**
  * @brief Initialize SPI MSP.
  */
void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (hspi->Instance == SPI1)
  {
    __HAL_RCC_SPI1_CLK_ENABLE();
    __HAL_RCC_DMA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(SPI_CS_GPIO_PORT, SPI_CS_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Pin       = GPIO_PIN_3 | GPIO_PIN_4 | GPIO_PIN_5;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF0_SPI1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = SPI_CS_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(SPI_CS_GPIO_PORT, &GPIO_InitStruct);

    HdmaCh2.Instance                 = DMA1_Channel2;
    HdmaCh2.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    HdmaCh2.Init.PeriphInc           = DMA_PINC_DISABLE;
    HdmaCh2.Init.MemInc              = DMA_MINC_ENABLE;
    HdmaCh2.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    HdmaCh2.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    HdmaCh2.Init.Mode                = DMA_NORMAL;
    HdmaCh2.Init.Priority            = DMA_PRIORITY_HIGH;
    HAL_DMA_DeInit(&HdmaCh2);
    HAL_DMA_Init(&HdmaCh2);
    HAL_DMA_ChannelMap(&HdmaCh2, DMA_CHANNEL_MAP_SPI1_TX);
    __HAL_LINKDMA(hspi, hdmatx, HdmaCh2);
    HdmaCh3.Instance                 = DMA1_Channel3;
    HdmaCh3.Init.Direction           = DMA_PERIPH_TO_MEMORY;
    HdmaCh3.Init.PeriphInc           = DMA_PINC_DISABLE;
    HdmaCh3.Init.MemInc              = DMA_MINC_ENABLE;
    HdmaCh3.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    HdmaCh3.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    HdmaCh3.Init.Mode                = DMA_NORMAL;
    HdmaCh3.Init.Priority            = DMA_PRIORITY_HIGH;
    HAL_DMA_DeInit(&HdmaCh3);
    HAL_DMA_Init(&HdmaCh3);
    HAL_DMA_ChannelMap(&HdmaCh3, DMA_CHANNEL_MAP_SPI1_RX);
    __HAL_LINKDMA(hspi, hdmarx, HdmaCh3);
    HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
  }
  else if (hspi->Instance == SPI2)
  {
    __HAL_RCC_SPI2_CLK_ENABLE();
    __HAL_RCC_DMA_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    HAL_GPIO_WritePin(LCD_CS_GPIO_PORT, LCD_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_DC_GPIO_PORT, LCD_DC_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_BL_GPIO_PORT, LCD_BL_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_RST_GPIO_PORT, LCD_RST_PIN, GPIO_PIN_SET);

    GPIO_InitStruct.Pin       = GPIO_PIN_0;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_NOPULL;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF0_SPI2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin       = GPIO_PIN_10;
    GPIO_InitStruct.Alternate = GPIO_AF0_SPI2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    GPIO_InitStruct.Pin   = LCD_CS_PIN | LCD_DC_PIN | LCD_BL_PIN;
    GPIO_InitStruct.Pin  |= LCD_RST_PIN;
    GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull  = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    HdmaCh3.Instance                 = DMA1_Channel3;
    HdmaCh3.Init.Direction           = DMA_MEMORY_TO_PERIPH;
    HdmaCh3.Init.PeriphInc           = DMA_PINC_DISABLE;
    HdmaCh3.Init.MemInc              = DMA_MINC_ENABLE;
    HdmaCh3.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
    HdmaCh3.Init.MemDataAlignment    = DMA_MDATAALIGN_BYTE;
    HdmaCh3.Init.Mode                = DMA_NORMAL;
    HdmaCh3.Init.Priority            = DMA_PRIORITY_HIGH;
    HAL_DMA_DeInit(&HdmaCh3);
    HAL_DMA_Init(&HdmaCh3);
    HAL_DMA_ChannelMap(&HdmaCh3, DMA_CHANNEL_MAP_SPI2_TX);
    __HAL_LINKDMA(hspi, hdmatx, HdmaCh3);
    HAL_NVIC_SetPriority(DMA1_Channel2_3_IRQn, 1U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Channel2_3_IRQn);
  }
}

/**
  * @brief Initialize UART MSP.
  */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  if (huart->Instance == USART1)
  {
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin       = GPIO_PIN_7;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF0_USART1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    GPIO_InitStruct.Pin       = GPIO_PIN_8;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    HAL_NVIC_SetPriority(USART1_IRQn, 1U, 1U);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
  }
}

/************************ (C) COPYRIGHT Puya *****END OF FILE******************/

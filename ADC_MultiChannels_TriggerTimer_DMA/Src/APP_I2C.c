#include "APP_I2C.h"

typedef struct
{
  GPIO_TypeDef *SdaPort;
  uint16_t SdaPin;
  GPIO_TypeDef *SclPort;
  uint16_t SclPin;
} APP_I2C_SoftPortTypeDef;

static const APP_I2C_SoftPortTypeDef sAPP_I2CSoftPorts[APP_I2C_PORT_COUNT] =
{
  {GPIOF, GPIO_PIN_0, GPIOF, GPIO_PIN_1},
  {GPIOF, GPIO_PIN_3, GPIOA, GPIO_PIN_1},
  {GPIOA, GPIO_PIN_2, GPIOB, GPIO_PIN_1}
};

static HAL_StatusTypeDef APP_I2C_SoftwareWriteRegister(
  const APP_I2C_SoftPortTypeDef *port,
  uint8_t slaveWriteAddr,
  uint8_t regAddr,
  uint8_t regValue);
static HAL_StatusTypeDef APP_I2C_SoftwareReadRegister(
  const APP_I2C_SoftPortTypeDef *port,
  uint8_t slaveWriteAddr,
  uint8_t slaveReadAddr,
  uint8_t regAddr,
  uint8_t *value);
static void APP_I2C_SoftDelay(void);
static void APP_I2C_SoftSDAHigh(const APP_I2C_SoftPortTypeDef *port);
static void APP_I2C_SoftSDALow(const APP_I2C_SoftPortTypeDef *port);
static void APP_I2C_SoftSCLHigh(const APP_I2C_SoftPortTypeDef *port);
static void APP_I2C_SoftSCLLow(const APP_I2C_SoftPortTypeDef *port);
static uint8_t APP_I2C_SoftReadSDA(const APP_I2C_SoftPortTypeDef *port);
static void APP_I2C_SoftStart(const APP_I2C_SoftPortTypeDef *port);
static void APP_I2C_SoftStop(const APP_I2C_SoftPortTypeDef *port);
static uint8_t APP_I2C_SoftWriteByte(const APP_I2C_SoftPortTypeDef *port, uint8_t data);
static uint8_t APP_I2C_SoftReadByte(const APP_I2C_SoftPortTypeDef *port, uint8_t ack);

void APP_I2C_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  uint8_t i;

  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();

  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;

  for (i = 0U; i < APP_I2C_PORT_COUNT; i++)
  {
    GPIO_InitStruct.Pin = sAPP_I2CSoftPorts[i].SdaPin;
    HAL_GPIO_Init(sAPP_I2CSoftPorts[i].SdaPort, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = sAPP_I2CSoftPorts[i].SclPin;
    HAL_GPIO_Init(sAPP_I2CSoftPorts[i].SclPort, &GPIO_InitStruct);

    APP_I2C_SoftSDAHigh(&sAPP_I2CSoftPorts[i]);
    APP_I2C_SoftSCLHigh(&sAPP_I2CSoftPorts[i]);
  }
}

HAL_StatusTypeDef APP_I2C_WriteRegister(uint8_t portId,
                                        uint8_t slaveWriteAddr,
                                        uint8_t regAddr,
                                        uint8_t regValue)
{
  if (portId >= APP_I2C_PORT_COUNT)
  {
    return HAL_ERROR;
  }

  return APP_I2C_SoftwareWriteRegister(
    &sAPP_I2CSoftPorts[portId],
    slaveWriteAddr,
    regAddr,
    regValue);
}

HAL_StatusTypeDef APP_I2C_ReadRegister(uint8_t portId,
                                       uint8_t slaveWriteAddr,
                                       uint8_t slaveReadAddr,
                                       uint8_t regAddr,
                                       uint8_t *value)
{
  if ((portId >= APP_I2C_PORT_COUNT) || (value == NULL))
  {
    return HAL_ERROR;
  }

  return APP_I2C_SoftwareReadRegister(
    &sAPP_I2CSoftPorts[portId],
    slaveWriteAddr,
    slaveReadAddr,
    regAddr,
    value);
}

static HAL_StatusTypeDef APP_I2C_SoftwareWriteRegister(
  const APP_I2C_SoftPortTypeDef *port,
  uint8_t slaveWriteAddr,
  uint8_t regAddr,
  uint8_t regValue)
{
  APP_I2C_SoftStart(port);
  if (APP_I2C_SoftWriteByte(port, slaveWriteAddr) == 0U)
  {
    APP_I2C_SoftStop(port);
    return HAL_ERROR;
  }
  if (APP_I2C_SoftWriteByte(port, regAddr) == 0U)
  {
    APP_I2C_SoftStop(port);
    return HAL_ERROR;
  }
  if (APP_I2C_SoftWriteByte(port, regValue) == 0U)
  {
    APP_I2C_SoftStop(port);
    return HAL_ERROR;
  }

  APP_I2C_SoftStop(port);
  return HAL_OK;
}

static HAL_StatusTypeDef APP_I2C_SoftwareReadRegister(
  const APP_I2C_SoftPortTypeDef *port,
  uint8_t slaveWriteAddr,
  uint8_t slaveReadAddr,
  uint8_t regAddr,
  uint8_t *value)
{
  APP_I2C_SoftStart(port);
  if (APP_I2C_SoftWriteByte(port, slaveWriteAddr) == 0U)
  {
    APP_I2C_SoftStop(port);
    return HAL_ERROR;
  }
  if (APP_I2C_SoftWriteByte(port, regAddr) == 0U)
  {
    APP_I2C_SoftStop(port);
    return HAL_ERROR;
  }

  APP_I2C_SoftStart(port);
  if (APP_I2C_SoftWriteByte(port, slaveReadAddr) == 0U)
  {
    APP_I2C_SoftStop(port);
    return HAL_ERROR;
  }

  *value = APP_I2C_SoftReadByte(port, 0U);
  APP_I2C_SoftStop(port);
  return HAL_OK;
}

static void APP_I2C_SoftDelay(void)
{
  volatile uint32_t delay = 120U;

  while (delay-- > 0U)
  {
    __NOP();
  }
}

static void APP_I2C_SoftSDAHigh(const APP_I2C_SoftPortTypeDef *port)
{
  HAL_GPIO_WritePin(port->SdaPort, port->SdaPin, GPIO_PIN_SET);
}

static void APP_I2C_SoftSDALow(const APP_I2C_SoftPortTypeDef *port)
{
  HAL_GPIO_WritePin(port->SdaPort, port->SdaPin, GPIO_PIN_RESET);
}

static void APP_I2C_SoftSCLHigh(const APP_I2C_SoftPortTypeDef *port)
{
  HAL_GPIO_WritePin(port->SclPort, port->SclPin, GPIO_PIN_SET);
}

static void APP_I2C_SoftSCLLow(const APP_I2C_SoftPortTypeDef *port)
{
  HAL_GPIO_WritePin(port->SclPort, port->SclPin, GPIO_PIN_RESET);
}

static uint8_t APP_I2C_SoftReadSDA(const APP_I2C_SoftPortTypeDef *port)
{
  return (HAL_GPIO_ReadPin(port->SdaPort, port->SdaPin) == GPIO_PIN_SET) ? 1U : 0U;
}

static void APP_I2C_SoftStart(const APP_I2C_SoftPortTypeDef *port)
{
  APP_I2C_SoftSDAHigh(port);
  APP_I2C_SoftSCLHigh(port);
  APP_I2C_SoftDelay();
  APP_I2C_SoftSDALow(port);
  APP_I2C_SoftDelay();
  APP_I2C_SoftSCLLow(port);
}

static void APP_I2C_SoftStop(const APP_I2C_SoftPortTypeDef *port)
{
  APP_I2C_SoftSDALow(port);
  APP_I2C_SoftDelay();
  APP_I2C_SoftSCLHigh(port);
  APP_I2C_SoftDelay();
  APP_I2C_SoftSDAHigh(port);
  APP_I2C_SoftDelay();
}

static uint8_t APP_I2C_SoftWriteByte(const APP_I2C_SoftPortTypeDef *port, uint8_t data)
{
  uint8_t i;
  uint8_t ack;

  for (i = 0U; i < 8U; i++)
  {
    if ((data & 0x80U) != 0U)
    {
      APP_I2C_SoftSDAHigh(port);
    }
    else
    {
      APP_I2C_SoftSDALow(port);
    }

    APP_I2C_SoftDelay();
    APP_I2C_SoftSCLHigh(port);
    APP_I2C_SoftDelay();
    APP_I2C_SoftSCLLow(port);
    data <<= 1U;
  }

  APP_I2C_SoftSDAHigh(port);
  APP_I2C_SoftDelay();
  APP_I2C_SoftSCLHigh(port);
  APP_I2C_SoftDelay();
  ack = (APP_I2C_SoftReadSDA(port) == 0U) ? 1U : 0U;
  APP_I2C_SoftSCLLow(port);
  return ack;
}

static uint8_t APP_I2C_SoftReadByte(const APP_I2C_SoftPortTypeDef *port, uint8_t ack)
{
  uint8_t i;
  uint8_t data = 0U;

  APP_I2C_SoftSDAHigh(port);
  for (i = 0U; i < 8U; i++)
  {
    data <<= 1U;
    APP_I2C_SoftDelay();
    APP_I2C_SoftSCLHigh(port);
    APP_I2C_SoftDelay();
    if (APP_I2C_SoftReadSDA(port) != 0U)
    {
      data |= 1U;
    }
    APP_I2C_SoftSCLLow(port);
  }

  if (ack != 0U)
  {
    APP_I2C_SoftSDALow(port);
  }
  else
  {
    APP_I2C_SoftSDAHigh(port);
  }

  APP_I2C_SoftDelay();
  APP_I2C_SoftSCLHigh(port);
  APP_I2C_SoftDelay();
  APP_I2C_SoftSCLLow(port);
  APP_I2C_SoftSDAHigh(port);
  return data;
}

#include "APP_I2C.h"

#define APP_I2C_ADDRESS_7BIT_MAX  0x7FU
#define APP_I2C_RECOVERY_CLOCKS   9U
#define APP_I2C_SCL_WAIT_LIMIT    1000U

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
static HAL_StatusTypeDef APP_I2C_SoftwareReadCommand(
  const APP_I2C_SoftPortTypeDef *port,
  uint8_t slaveAddress7Bit,
  uint8_t command,
  uint8_t *response,
  uint16_t responseLength);
static void APP_I2C_SoftDelay(void);
static void APP_I2C_SoftSDAHigh(const APP_I2C_SoftPortTypeDef *port);
static void APP_I2C_SoftSDALow(const APP_I2C_SoftPortTypeDef *port);
static HAL_StatusTypeDef APP_I2C_SoftSCLHigh(const APP_I2C_SoftPortTypeDef *port);
static void APP_I2C_SoftSCLLow(const APP_I2C_SoftPortTypeDef *port);
static uint8_t APP_I2C_SoftReadSDA(const APP_I2C_SoftPortTypeDef *port);
static uint8_t APP_I2C_SoftReadSCL(const APP_I2C_SoftPortTypeDef *port);
static HAL_StatusTypeDef APP_I2C_SoftEnsureBus(const APP_I2C_SoftPortTypeDef *port);
static HAL_StatusTypeDef APP_I2C_SoftStart(const APP_I2C_SoftPortTypeDef *port);
static HAL_StatusTypeDef APP_I2C_SoftStop(const APP_I2C_SoftPortTypeDef *port);
static HAL_StatusTypeDef APP_I2C_SoftWriteByte(const APP_I2C_SoftPortTypeDef *port,
                                               uint8_t data);
static HAL_StatusTypeDef APP_I2C_SoftReadByte(const APP_I2C_SoftPortTypeDef *port,
                                              uint8_t ack,
                                              uint8_t *data);

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

HAL_StatusTypeDef APP_I2C_ReadCommand(uint8_t portId,
                                      uint8_t slaveAddress7Bit,
                                      uint8_t command,
                                      uint8_t *response,
                                      uint16_t responseLength)
{
  if ((portId >= APP_I2C_PORT_COUNT) ||
      (slaveAddress7Bit > APP_I2C_ADDRESS_7BIT_MAX) ||
      (response == NULL) || (responseLength == 0U))
  {
    return HAL_ERROR;
  }

  return APP_I2C_SoftwareReadCommand(&sAPP_I2CSoftPorts[portId],
                                     slaveAddress7Bit,
                                     command,
                                     response,
                                     responseLength);
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
  HAL_StatusTypeDef status = APP_I2C_SoftEnsureBus(port);

  if (status != HAL_OK)
  {
    return status;
  }
  status = APP_I2C_SoftStart(port);
  if (status != HAL_OK)
  {
    return status;
  }
  if (APP_I2C_SoftWriteByte(port, slaveWriteAddr) != HAL_OK)
  {
    (void)APP_I2C_SoftStop(port);
    return HAL_ERROR;
  }
  if (APP_I2C_SoftWriteByte(port, regAddr) != HAL_OK)
  {
    (void)APP_I2C_SoftStop(port);
    return HAL_ERROR;
  }
  if (APP_I2C_SoftWriteByte(port, regValue) != HAL_OK)
  {
    (void)APP_I2C_SoftStop(port);
    return HAL_ERROR;
  }

  return APP_I2C_SoftStop(port);
}

static HAL_StatusTypeDef APP_I2C_SoftwareReadRegister(
  const APP_I2C_SoftPortTypeDef *port,
  uint8_t slaveWriteAddr,
  uint8_t slaveReadAddr,
  uint8_t regAddr,
  uint8_t *value)
{
  HAL_StatusTypeDef status = APP_I2C_SoftEnsureBus(port);

  if (status != HAL_OK)
  {
    return status;
  }
  status = APP_I2C_SoftStart(port);
  if (status != HAL_OK)
  {
    return status;
  }
  if (APP_I2C_SoftWriteByte(port, slaveWriteAddr) != HAL_OK)
  {
    (void)APP_I2C_SoftStop(port);
    return HAL_ERROR;
  }
  if (APP_I2C_SoftWriteByte(port, regAddr) != HAL_OK)
  {
    (void)APP_I2C_SoftStop(port);
    return HAL_ERROR;
  }

  status = APP_I2C_SoftStart(port);
  if (status != HAL_OK)
  {
    (void)APP_I2C_SoftStop(port);
    return status;
  }
  if (APP_I2C_SoftWriteByte(port, slaveReadAddr) != HAL_OK)
  {
    (void)APP_I2C_SoftStop(port);
    return HAL_ERROR;
  }

  status = APP_I2C_SoftReadByte(port, 0U, value);
  if (status != HAL_OK)
  {
    (void)APP_I2C_SoftStop(port);
    return status;
  }
  return APP_I2C_SoftStop(port);
}

static HAL_StatusTypeDef APP_I2C_SoftwareReadCommand(
  const APP_I2C_SoftPortTypeDef *port,
  uint8_t slaveAddress7Bit,
  uint8_t command,
  uint8_t *response,
  uint16_t responseLength)
{
  HAL_StatusTypeDef status;
  uint16_t index;
  uint8_t writeAddress = (uint8_t)(slaveAddress7Bit << 1U);
  uint8_t readAddress = (uint8_t)(writeAddress | 0x01U);

  status = APP_I2C_SoftEnsureBus(port);
  if (status != HAL_OK)
  {
    return status;
  }
  status = APP_I2C_SoftStart(port);
  if (status != HAL_OK)
  {
    return status;
  }
  status = APP_I2C_SoftWriteByte(port, writeAddress);
  if (status == HAL_OK)
  {
    status = APP_I2C_SoftWriteByte(port, command);
  }
  if (status == HAL_OK)
  {
    status = APP_I2C_SoftStart(port);
  }
  if (status == HAL_OK)
  {
    status = APP_I2C_SoftWriteByte(port, readAddress);
  }
  if (status != HAL_OK)
  {
    (void)APP_I2C_SoftStop(port);
    return status;
  }

  for (index = 0U; index < responseLength; index++)
  {
    uint8_t sendAck = ((index + 1U) < responseLength) ? 1U : 0U;
    status = APP_I2C_SoftReadByte(port, sendAck, &response[index]);
    if (status != HAL_OK)
    {
      (void)APP_I2C_SoftStop(port);
      return status;
    }
  }

  return APP_I2C_SoftStop(port);
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

static HAL_StatusTypeDef APP_I2C_SoftSCLHigh(const APP_I2C_SoftPortTypeDef *port)
{
  uint32_t wait = APP_I2C_SCL_WAIT_LIMIT;

  HAL_GPIO_WritePin(port->SclPort, port->SclPin, GPIO_PIN_SET);
  while (wait-- > 0U)
  {
    if (APP_I2C_SoftReadSCL(port) != 0U)
    {
      return HAL_OK;
    }
  }
  return HAL_TIMEOUT;
}

static void APP_I2C_SoftSCLLow(const APP_I2C_SoftPortTypeDef *port)
{
  HAL_GPIO_WritePin(port->SclPort, port->SclPin, GPIO_PIN_RESET);
}

static uint8_t APP_I2C_SoftReadSDA(const APP_I2C_SoftPortTypeDef *port)
{
  return (HAL_GPIO_ReadPin(port->SdaPort, port->SdaPin) == GPIO_PIN_SET) ? 1U : 0U;
}

static uint8_t APP_I2C_SoftReadSCL(const APP_I2C_SoftPortTypeDef *port)
{
  return (HAL_GPIO_ReadPin(port->SclPort, port->SclPin) == GPIO_PIN_SET) ? 1U : 0U;
}

static HAL_StatusTypeDef APP_I2C_SoftEnsureBus(const APP_I2C_SoftPortTypeDef *port)
{
  uint8_t clock;

  APP_I2C_SoftSDAHigh(port);
  if (APP_I2C_SoftSCLHigh(port) != HAL_OK)
  {
    return HAL_TIMEOUT;
  }
  APP_I2C_SoftDelay();
  if (APP_I2C_SoftReadSDA(port) != 0U)
  {
    return HAL_OK;
  }

  for (clock = 0U; clock < APP_I2C_RECOVERY_CLOCKS; clock++)
  {
    APP_I2C_SoftSCLLow(port);
    APP_I2C_SoftDelay();
    if (APP_I2C_SoftSCLHigh(port) != HAL_OK)
    {
      return HAL_TIMEOUT;
    }
    APP_I2C_SoftDelay();
    if (APP_I2C_SoftReadSDA(port) != 0U)
    {
      break;
    }
  }

  if (APP_I2C_SoftStop(port) != HAL_OK)
  {
    return HAL_TIMEOUT;
  }
  if ((APP_I2C_SoftReadSDA(port) == 0U) ||
      (APP_I2C_SoftReadSCL(port) == 0U))
  {
    return HAL_TIMEOUT;
  }
  return HAL_OK;
}

static HAL_StatusTypeDef APP_I2C_SoftStart(const APP_I2C_SoftPortTypeDef *port)
{
  APP_I2C_SoftSDAHigh(port);
  if (APP_I2C_SoftSCLHigh(port) != HAL_OK)
  {
    return HAL_TIMEOUT;
  }
  APP_I2C_SoftDelay();
  APP_I2C_SoftSDALow(port);
  APP_I2C_SoftDelay();
  APP_I2C_SoftSCLLow(port);
  return HAL_OK;
}

static HAL_StatusTypeDef APP_I2C_SoftStop(const APP_I2C_SoftPortTypeDef *port)
{
  APP_I2C_SoftSDALow(port);
  APP_I2C_SoftDelay();
  if (APP_I2C_SoftSCLHigh(port) != HAL_OK)
  {
    APP_I2C_SoftSDAHigh(port);
    return HAL_TIMEOUT;
  }
  APP_I2C_SoftDelay();
  APP_I2C_SoftSDAHigh(port);
  APP_I2C_SoftDelay();
  return HAL_OK;
}

static HAL_StatusTypeDef APP_I2C_SoftWriteByte(const APP_I2C_SoftPortTypeDef *port,
                                               uint8_t data)
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
    if (APP_I2C_SoftSCLHigh(port) != HAL_OK)
    {
      return HAL_TIMEOUT;
    }
    APP_I2C_SoftDelay();
    APP_I2C_SoftSCLLow(port);
    data <<= 1U;
  }

  APP_I2C_SoftSDAHigh(port);
  APP_I2C_SoftDelay();
  if (APP_I2C_SoftSCLHigh(port) != HAL_OK)
  {
    return HAL_TIMEOUT;
  }
  APP_I2C_SoftDelay();
  ack = (APP_I2C_SoftReadSDA(port) == 0U) ? 1U : 0U;
  APP_I2C_SoftSCLLow(port);
  return (ack != 0U) ? HAL_OK : HAL_ERROR;
}

static HAL_StatusTypeDef APP_I2C_SoftReadByte(const APP_I2C_SoftPortTypeDef *port,
                                              uint8_t ack,
                                              uint8_t *data)
{
  uint8_t i;

  if (data == NULL)
  {
    return HAL_ERROR;
  }

  *data = 0U;
  APP_I2C_SoftSDAHigh(port);
  for (i = 0U; i < 8U; i++)
  {
    *data <<= 1U;
    APP_I2C_SoftDelay();
    if (APP_I2C_SoftSCLHigh(port) != HAL_OK)
    {
      return HAL_TIMEOUT;
    }
    APP_I2C_SoftDelay();
    if (APP_I2C_SoftReadSDA(port) != 0U)
    {
      *data |= 1U;
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
  if (APP_I2C_SoftSCLHigh(port) != HAL_OK)
  {
    APP_I2C_SoftSDAHigh(port);
    return HAL_TIMEOUT;
  }
  APP_I2C_SoftDelay();
  APP_I2C_SoftSCLLow(port);
  APP_I2C_SoftSDAHigh(port);
  return HAL_OK;
}

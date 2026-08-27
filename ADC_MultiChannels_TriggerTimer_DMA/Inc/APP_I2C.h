#ifndef __APP_I2C_H
#define __APP_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "py32f0xx_hal.h"

#define APP_I2C_PORT_COUNT  3U
#define APP_I2C_PORT_1_ID   0U
#define APP_I2C_PORT_2_ID   1U
#define APP_I2C_PORT_3_ID   2U

void APP_I2C_Init(void);
HAL_StatusTypeDef APP_I2C_WriteRegister(uint8_t portId,
                                        uint8_t slaveWriteAddr,
                                        uint8_t regAddr,
                                        uint8_t regValue);
HAL_StatusTypeDef APP_I2C_ReadRegister(uint8_t portId,
                                       uint8_t slaveWriteAddr,
                                       uint8_t slaveReadAddr,
                                       uint8_t regAddr,
                                       uint8_t *value);

#ifdef __cplusplus
}
#endif

#endif /* __APP_I2C_H */

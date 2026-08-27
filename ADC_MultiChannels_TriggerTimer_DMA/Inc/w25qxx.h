#ifndef __W25QXX_H
#define __W25QXX_H

#include "main.h"

/* W25QXX Chip ID (Read via 0x90 command, Manufacturer+Device) */
// #define W25Q80    0x4013
// #define W25Q16    0x4014
// #define W25Q32    0x4015
#define W25Q64    0x8516
// #define W25Q128   0x4017
// #define W25Q256   0x4018

/* CS Pin Control - PB6, defined in main.h */
#define W25QXX_CS_LOW()    HAL_GPIO_WritePin(SPI_CS_GPIO_PORT, SPI_CS_PIN, GPIO_PIN_RESET)
#define W25QXX_CS_HIGH()   HAL_GPIO_WritePin(SPI_CS_GPIO_PORT, SPI_CS_PIN, GPIO_PIN_SET)

/* W25QXX Sector/Page Size */
#define W25QXX_SECTOR_SIZE     4096    /* 4KB per sector */
#define W25QXX_PAGE_SIZE       256     /* 256 bytes per page */
#define W25QXX_FLASH_SIZE_BYTES 0x800000UL /* W25Q64 total size: 8MB */
#define W25QXX_MAX_24BIT_ADDR_BYTES 0x1000000UL /* 24-bit address command limit: 16MB */

/* W25QXX Instruction Table */
#define W25X_WriteEnable       0x06
#define W25X_WriteDisable      0x04
#define W25X_ReadStatusReg1    0x05
#define W25X_ReadStatusReg2    0x35
#define W25X_ReadStatusReg3    0x15
#define W25X_WriteStatusReg1   0x01
#define W25X_WriteStatusReg2   0x31
#define W25X_WriteStatusReg3   0x11
#define W25X_ReadData          0x03
#define W25X_FastReadData      0x0B
#define W25X_FastReadDual      0x3B
#define W25X_PageProgram       0x02
#define W25X_BlockErase        0xD8
#define W25X_SectorErase       0x20
#define W25X_ChipErase         0xC7
#define W25X_PowerDown         0xB9
#define W25X_ReleasePowerDown  0xAB
#define W25X_DeviceID          0xAB
#define W25X_ManufactDeviceID  0x90
#define W25X_JedecDeviceID     0x9F
#define W25X_EnableReset       0x66
#define W25X_ResetDevice       0x99

extern uint16_t W25QXX_TYPE;
extern uint32_t W25QXX_FlashSizeBytes;
extern SPI_HandleTypeDef hspi1;
extern DMA_HandleTypeDef HdmaCh2;
extern DMA_HandleTypeDef HdmaCh3;

void     W25QXX_Init(void);
uint16_t W25QXX_ReadID(void);
uint32_t W25QXX_ReadJEDECID(void);
uint8_t  W25QXX_ReadSR(void);
void     W25QXX_Write_SR(uint8_t sr);
void     W25QXX_Write_Enable(void);
void     W25QXX_Write_Disable(void);
void     W25QXX_Write_NoCheck(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite);
void     W25QXX_Read(uint8_t *pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead);
HAL_StatusTypeDef W25QXX_Read_DMA_Start(uint8_t *pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead);
void     W25QXX_Read_DMA_End(void);
void     W25QXX_Erase_Chip(void);
void     W25QXX_Erase_Sector(uint32_t Dst_Addr);
void     W25QXX_Wait_Busy(void);
void     W25QXX_PowerDown(void);
void     W25QXX_WAKEUP(void);

#endif /* __W25QXX_H */

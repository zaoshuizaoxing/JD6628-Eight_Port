/**
 * @file    w25qxx.c
 * @brief   W25QXX SPI Flash driver for PY32F031 (SPI1 Master)
 *
 * ?? HAL SPI ????????? W25Q64
 * CS ??: PB6 (GPIO ????)
 * SPI1: PB3=SCK, PB5=MOSI, PB4=MISO
 */

#include "w25qxx.h"

SPI_HandleTypeDef hspi1;
DMA_HandleTypeDef HdmaCh2;
DMA_HandleTypeDef HdmaCh3;

uint16_t W25QXX_TYPE = 0;  /* W25QXX_TYPE ? main.c ??????? */
uint32_t W25QXX_FlashSizeBytes = W25QXX_FLASH_SIZE_BYTES;
static const uint8_t W25QXX_DMA_DUMMY_BYTE = 0xFFU;


/* ========================================================================== */
/*  SPI ??????                                                           */
/* ========================================================================== */

/**
 * @brief  SPI ????????
 * @param  txData  ?????
 * @retval ??????
 */
static uint8_t W25QXX_SPI_ReadWriteByte(uint8_t txData)
{
    uint8_t rxData;
    HAL_SPI_TransmitReceive(&hspi1, &txData, &rxData, 1, 100);
    return rxData;
}

/**
 * @brief  ????? (?????? 64MHz HRC)
 * @param  us  ?????
 */
static void W25QXX_DelayUs(uint32_t us)
{
    /* 64MHz ??? us ? 64 ????????? 4 ??? => ? 16 iterations/us */
    volatile uint32_t count = us * 16;
    while (count--) {
        __NOP();
    }
}
static uint32_t W25QXX_CalcFlashSizeFromJEDEC(uint32_t jedec_id)
{
    uint8_t capacity = (uint8_t)jedec_id;
    uint32_t flash_size = W25QXX_FLASH_SIZE_BYTES;

    if ((capacity >= 20U) && (capacity <= 31U)) {
        flash_size = 1UL << capacity;
    }

    if (flash_size > W25QXX_MAX_24BIT_ADDR_BYTES) {
        flash_size = W25QXX_MAX_24BIT_ADDR_BYTES;
    }

    return flash_size;
}

/* ========================================================================== */
/*  W25QXX ????                                                            */
/* ========================================================================== */

/**
 * @brief  ??? W25QXX????? ID ??? W25QXX_TYPE
 */
void W25QXX_Init(void)
{
    hspi1.Instance               = SPI1;
    hspi1.Init.Mode              = SPI_MODE_MASTER;
    hspi1.Init.Direction         = SPI_DIRECTION_2LINES;
    hspi1.Init.DataSize          = SPI_DATASIZE_8BIT;
    hspi1.Init.CLKPolarity       = SPI_POLARITY_LOW;
    hspi1.Init.CLKPhase          = SPI_PHASE_1EDGE;
    hspi1.Init.NSS               = SPI_NSS_SOFT;
    hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
    hspi1.Init.FirstBit          = SPI_FIRSTBIT_MSB;
    hspi1.Init.CRCCalculation    = SPI_CRCCALCULATION_DISABLE;
    hspi1.Init.CRCPolynomial     = 7U;

    if (HAL_SPI_Init(&hspi1) != HAL_OK) {
        APP_ErrorHandler();
    }

    W25QXX_CS_HIGH();
    W25QXX_TYPE = W25QXX_ReadID();
    W25QXX_FlashSizeBytes = W25QXX_CalcFlashSizeFromJEDEC(W25QXX_ReadJEDECID());
}

/**
 * @brief  ???????1
 * @retval ?????? (BIT0=BUSY)
 */
uint8_t W25QXX_ReadSR(void)
{
    uint8_t byte = 0;
    W25QXX_CS_LOW();
    W25QXX_SPI_ReadWriteByte(W25X_ReadStatusReg1);
    byte = W25QXX_SPI_ReadWriteByte(0xFF);
    W25QXX_CS_HIGH();
    return byte;
}

/**
 * @brief  ??????1
 * @param  sr  ????? (? bit7,5,4,3,2 ??)
 */
void W25QXX_Write_SR(uint8_t sr)
{
    W25QXX_CS_LOW();
    W25QXX_SPI_ReadWriteByte(W25X_WriteStatusReg1);
    W25QXX_SPI_ReadWriteByte(sr);
    W25QXX_CS_HIGH();
}

/**
 * @brief  ??? (? WEL ??)
 */
void W25QXX_Write_Enable(void)
{
    W25QXX_CS_LOW();
    W25QXX_SPI_ReadWriteByte(W25X_WriteEnable);
    W25QXX_CS_HIGH();
}

/**
 * @brief  ??? (? WEL ??)
 */
void W25QXX_Write_Disable(void)
{
    W25QXX_CS_LOW();
    W25QXX_SPI_ReadWriteByte(W25X_WriteDisable);
    W25QXX_CS_HIGH();
}

/**
 * @brief  ?? Manufacturer + Device ID (?? 0x90)
 * @retval 16-bit ID:
 *         0xEF13 = W25Q80, 0xEF14 = W25Q16, 0xEF15 = W25Q32,
 *         0xEF16 = W25Q64, 0xEF17 = W25Q128
 */
uint16_t W25QXX_ReadID(void)
{
    uint16_t id = 0;
    W25QXX_CS_LOW();
    W25QXX_SPI_ReadWriteByte(W25X_ManufactDeviceID);  /* 0x90 */
    W25QXX_SPI_ReadWriteByte(0x00);
    W25QXX_SPI_ReadWriteByte(0x00);
    W25QXX_SPI_ReadWriteByte(0x00);
    id  = W25QXX_SPI_ReadWriteByte(0xFF) << 8;  /* Manufacturer ID */
    id |= W25QXX_SPI_ReadWriteByte(0xFF);        /* Device ID */
    W25QXX_CS_HIGH();
    return id;
}

/**
 * @brief  ?? JEDEC ID (?? 0x9F)
 * @retval 24-bit JEDEC ID: [Manufacturer, MemoryType, Capacity]
 *         W25Q64 = 0x856017
 */
uint32_t W25QXX_ReadJEDECID(void)
{
    uint32_t id = 0;
    uint8_t txBuf[4] = {W25X_JedecDeviceID, 0xFF, 0xFF, 0xFF};
    uint8_t rxBuf[4];

    W25QXX_CS_LOW();
    HAL_SPI_TransmitReceive(&hspi1, txBuf, rxBuf, 4, 100);
    W25QXX_CS_HIGH();

    id  = rxBuf[1] << 16;  /* Manufacturer ID */
    id |= rxBuf[2] << 8;   /* Memory Type */
    id |= rxBuf[3];         /* Capacity */
    return id;
}

/**
 * @brief  ?? SPI Flash ??
 * @param  pBuffer       ?????
 * @param  ReadAddr      ???? (24-bit)
 * @param  NumByteToRead ????? (?? 65535)
 */
void W25QXX_Read(uint8_t *pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead)
{
    uint8_t cmd[5];
    uint8_t dummy[32];
    uint16_t i;

    cmd[0] = W25X_FastReadData;
    cmd[1] = (uint8_t)(ReadAddr >> 16);
    cmd[2] = (uint8_t)(ReadAddr >> 8);
    cmd[3] = (uint8_t)(ReadAddr);
    cmd[4] = 0xFFU;

    for (i = 0; i < sizeof(dummy); i++) {
        dummy[i] = 0xFF;
    }

    W25QXX_CS_LOW();
    HAL_SPI_Transmit(&hspi1, cmd, 5, 100);
    while (NumByteToRead > 0U) {
        uint16_t chunk = NumByteToRead;

        if (chunk > sizeof(dummy)) {
            chunk = sizeof(dummy);
        }
        HAL_SPI_TransmitReceive(&hspi1, dummy, pBuffer, chunk, 100);
        pBuffer += chunk;
        NumByteToRead -= chunk;
    }
    W25QXX_CS_HIGH();
}

HAL_StatusTypeDef W25QXX_Read_DMA_Start(uint8_t *pBuffer, uint32_t ReadAddr, uint16_t NumByteToRead)
{
    uint8_t cmd[5];
    HAL_StatusTypeDef status;

    if ((pBuffer == 0) || (NumByteToRead == 0U)) {
        return HAL_ERROR;
    }

    cmd[0] = W25X_FastReadData;
    cmd[1] = (uint8_t)(ReadAddr >> 16);
    cmd[2] = (uint8_t)(ReadAddr >> 8);
    cmd[3] = (uint8_t)ReadAddr;
    cmd[4] = 0xFFU;

    W25QXX_CS_LOW();
    status = HAL_SPI_Transmit(&hspi1, cmd, 5U, 100U);
    if (status != HAL_OK) {
        W25QXX_CS_HIGH();
        return status;
    }

    hspi1.hdmatx->Init.MemInc = DMA_MINC_DISABLE;
    CLEAR_BIT(hspi1.hdmatx->Instance->CCR, DMA_CCR_MINC);

    status = HAL_SPI_TransmitReceive_DMA(&hspi1, (uint8_t *)&W25QXX_DMA_DUMMY_BYTE, pBuffer, NumByteToRead);
    if (status != HAL_OK) {
        W25QXX_Read_DMA_End();
    }

    return status;
}

void W25QXX_Read_DMA_End(void)
{
    if (hspi1.hdmatx != 0) {
        hspi1.hdmatx->Init.MemInc = DMA_MINC_ENABLE;
        SET_BIT(hspi1.hdmatx->Instance->CCR, DMA_CCR_MINC);
    }
    W25QXX_CS_HIGH();
}

/**
 * @brief  SPI ???????? 256 ?????
 * @param  pBuffer        ?????
 * @param  WriteAddr      ???? (24-bit)
 * @param  NumByteToWrite ????? (?? 256?????)
 */
void W25QXX_Write_Page(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    uint8_t cmd[4];
    cmd[0] = W25X_PageProgram;
    cmd[1] = (uint8_t)(WriteAddr >> 16);
    cmd[2] = (uint8_t)(WriteAddr >> 8);
    cmd[3] = (uint8_t)(WriteAddr);

    W25QXX_Write_Enable();
    W25QXX_CS_LOW();
    HAL_SPI_Transmit(&hspi1, cmd, 4, 100);
    HAL_SPI_Transmit(&hspi1, pBuffer, NumByteToWrite, 100);
    W25QXX_CS_HIGH();
    W25QXX_Wait_Busy();
}

/**
 * @brief  ???? SPI Flash (????)
 *         ???????????????? 0xFF???? 0xFF ??????
 * @param  pBuffer        ?????
 * @param  WriteAddr      ???? (24-bit)
 * @param  NumByteToWrite ????? (?? 65535)
 */
void W25QXX_Write_NoCheck(uint8_t *pBuffer, uint32_t WriteAddr, uint16_t NumByteToWrite)
{
    uint16_t pageremain;

    if (NumByteToWrite == 0)
        return;
    pageremain = W25QXX_PAGE_SIZE - WriteAddr % W25QXX_PAGE_SIZE;

    if (NumByteToWrite <= pageremain)
        pageremain = NumByteToWrite;

    while (1) {
        W25QXX_Write_Page(pBuffer, WriteAddr, pageremain);
        if (NumByteToWrite == pageremain)
            break;
        pBuffer += pageremain;
        WriteAddr += pageremain;
        NumByteToWrite -= pageremain;
        if (NumByteToWrite > W25QXX_PAGE_SIZE)
            pageremain = W25QXX_PAGE_SIZE;
        else
            pageremain = NumByteToWrite;
    }
}

/**
 * @brief  ? SPI Flash (?????)
 *         ?????????????????????????
 * @param  pBuffer        ?????
 * @param  WriteAddr      ???? (24-bit)
 * @param  NumByteToWrite ????? (?? 65535)
 */
void W25QXX_Erase_Chip(void)
{
    W25QXX_Write_Enable();
    W25QXX_Wait_Busy();
    W25QXX_CS_LOW();
    W25QXX_SPI_ReadWriteByte(W25X_ChipErase);
    W25QXX_CS_HIGH();
    W25QXX_Wait_Busy();
}

/**
 * @brief  ?????? (4KB)
 * @param  Dst_Addr  ???? (0, 1, 2, ...)?????? 4096
 */
void W25QXX_Erase_Sector(uint32_t Dst_Addr)
{
    uint32_t addr = Dst_Addr * W25QXX_SECTOR_SIZE;
    uint8_t cmd[4];
    cmd[0] = W25X_SectorErase;
    cmd[1] = (uint8_t)(addr >> 16);
    cmd[2] = (uint8_t)(addr >> 8);
    cmd[3] = (uint8_t)(addr);

    W25QXX_Write_Enable();
    W25QXX_Wait_Busy();
    W25QXX_CS_LOW();
    HAL_SPI_Transmit(&hspi1, cmd, 4, 100);
    W25QXX_CS_HIGH();
    W25QXX_Wait_Busy();
}

/**
 * @brief  ?? Flash ?? (?? BUSY ?)
 */
void W25QXX_Wait_Busy(void)
{
    while ((W25QXX_ReadSR() & 0x01) == 0x01)
        ;
}

/**
 * @brief  ??????
 */
void W25QXX_PowerDown(void)
{
    W25QXX_CS_LOW();
    W25QXX_SPI_ReadWriteByte(W25X_PowerDown);
    W25QXX_CS_HIGH();
    W25QXX_DelayUs(3);  /* tDP >= 3us */
}

/**
 * @brief  ?? (???????)
 */
void W25QXX_WAKEUP(void)
{
    W25QXX_CS_LOW();
    W25QXX_SPI_ReadWriteByte(W25X_ReleasePowerDown);
    W25QXX_CS_HIGH();
    W25QXX_DelayUs(3);  /* tRES1 >= 3us */
}

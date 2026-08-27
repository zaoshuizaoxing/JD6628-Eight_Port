#ifndef __UART_FLASH_LOADER_H
#define __UART_FLASH_LOADER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "py32f0xx_hal.h"

#define ENABLE_UART_FLASH_LOADER 0
// ============================== UART Flash ==============================
#define UART_FLASH_FRAME_START    0xAAU     
#define UART_FLASH_FRAME_END      0x55U      
#define UART_FLASH_CMD_DATA       0x01U      
#define UART_FLASH_CMD_END        0x02U      
#define UART_FLASH_CMD_INIT       0x03U      
#define UART_FLASH_ACK_SUCCESS    0x10U    
#define UART_FLASH_ACK_FAIL       0x11U      
#define UART_FLASH_RX_TIMEOUT_MS  5000U     
#define UART_FLASH_BYTE_TIMEOUT_MS 3000U     

// UART Flash
//test
#define UART_FLASH_FAIL_CMD_TIMEOUT 0xE1U    
#define UART_FLASH_FAIL_LEN_TIMEOUT 0xE2U     
#define UART_FLASH_FAIL_PAYLOAD_TIMEOUT 0xE3U 
#define UART_FLASH_FAIL_CHECKSUM_TIMEOUT 0xE4U 
#define UART_FLASH_FAIL_END_TIMEOUT 0xE5U    
#define UART_FLASH_FAIL_LEN_OVERFLOW 0xE6U  
#define UART_FLASH_FAIL_CHECKSUM 0xE7U       
#define UART_FLASH_FAIL_END_BYTE 0xE8U       
#define UART_FLASH_FAIL_INIT_PARAM 0xE9U      
#define UART_FLASH_FAIL_DATA_PARAM 0xEAU      
#define UART_FLASH_FAIL_END_PARAM 0xEBU       
#define UART_FLASH_FAIL_UNKNOWN_CMD 0xECU    

void UartFlashLoader_Run(void);
void UartFlashLoader_DisableRxIrqAndFlush(void);

#ifdef __cplusplus
}
#endif

#endif /* __UART_FLASH_LOADER_H */

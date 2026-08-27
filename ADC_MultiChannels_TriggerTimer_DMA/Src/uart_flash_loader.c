#include "uart_flash_loader.h"
#include "w25qxx.h"

extern UART_HandleTypeDef huart1;

static uint8_t uart_flash_fail_code = 0U;
static uint16_t uart_flash_fail_detail = 0U;

static uint8_t uart_flash_rx_buf[W25QXX_SECTOR_SIZE];

static uint8_t UartFlashChecksumAdd(uint8_t checksum, uint8_t data)
{
    return (uint8_t)(checksum + data);
}

static void UartFlashSendByte(uint8_t data)
{
    (void)HAL_UART_Transmit(&huart1, &data, 1U, 1000U);
}
/**
 * @brief  ���UART������ص��жϱ�־
 */
static void UartFlashClearRxFlags(void)
{
    __HAL_UART_CLEAR_PEFLAG(&huart1);
}
/**
 * @brief  ����ʱ�ش�UART��ȡһ���ֽ�
 * @param  data: ָ��洢��ȡ���ݵ�ָ��
 * @param  timeout_ms: ��ʱʱ�䣨���룩
 * @retval 1=�ɹ���ȡ��0=��ʱ
 */
static uint8_t UartFlashReadByte(uint8_t *data, uint32_t timeout_ms)
{
    if (HAL_UART_Receive(&huart1, data, 1U, timeout_ms) == HAL_OK) {
        return 1U;
    }

    UartFlashClearRxFlags();
    return 0U;
}

/**
 * @brief  ����UART�����жϲ���ս��ջ�����
 */
void UartFlashLoader_DisableRxIrqAndFlush(void)
{
    HAL_NVIC_DisableIRQ(USART1_IRQn);
    HAL_NVIC_ClearPendingIRQ(USART1_IRQn);
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_RXNE);
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_ERR);
    __HAL_UART_DISABLE_IT(&huart1, UART_IT_PE);

    while (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE)) {
        (void)huart1.Instance->DR;
    }

    UartFlashClearRxFlags();
}

/**
 * @brief  ͨ��UART����Ӧ��֡
 * @param  ack_cmd: Ӧ��������
 * @param  data: Ӧ������ָ��
 * @param  data_len: Ӧ�����ݳ���
 */
static void UartFlashSendAck(uint8_t ack_cmd, const uint8_t *data, uint16_t data_len)
{
    uint16_t i;
    uint8_t checksum = 0U;
    uint8_t len_hi = (uint8_t)(data_len >> 8);
    uint8_t len_lo = (uint8_t)data_len;

    // ����У���
    checksum = UartFlashChecksumAdd(checksum, UART_FLASH_FRAME_START);
    checksum = UartFlashChecksumAdd(checksum, ack_cmd);
    checksum = UartFlashChecksumAdd(checksum, len_hi);
    checksum = UartFlashChecksumAdd(checksum, len_lo);

    // ����֡ͷ
    UartFlashSendByte(UART_FLASH_FRAME_START);
    UartFlashSendByte(ack_cmd);
    UartFlashSendByte(len_hi);
    UartFlashSendByte(len_lo);

    // ��������
    for (i = 0U; i < data_len; i++) {
        checksum = UartFlashChecksumAdd(checksum, data[i]);
        UartFlashSendByte(data[i]);
    }

    // ����֡β��У���+�����ֽڣ�
    UartFlashSendByte(checksum);
    UartFlashSendByte(UART_FLASH_FRAME_END);
}

/**
 * @brief  ���ͳɹ�Ӧ��
 */
static void UartFlashSendAckSuccess(void)
{
    UartFlashSendAck(UART_FLASH_ACK_SUCCESS, 0, 0U);
}

/**
 * @brief  ����ʧ��Ӧ�𣨰�������������飩
 */
static void UartFlashSendAckFail(void)
{
    uint8_t fail_data[3];

    fail_data[0] = uart_flash_fail_code;
    fail_data[1] = (uint8_t)(uart_flash_fail_detail >> 8);
    fail_data[2] = (uint8_t)uart_flash_fail_detail;
    UartFlashSendAck(UART_FLASH_ACK_FAIL, fail_data, 3U);
    uart_flash_fail_code = 0U;
    uart_flash_fail_detail = 0U;
}

/**
 * @brief  ��������д���ַ���ɹ�Ӧ��
 * @param  final_addr: ����д���Flash��ַ
 */
static void UartFlashSendFinalAddr(uint32_t final_addr)
{
    uint8_t data[4];

    data[0] = (uint8_t)(final_addr >> 24);
    data[1] = (uint8_t)(final_addr >> 16);
    data[2] = (uint8_t)(final_addr >> 8);
    data[3] = (uint8_t)final_addr;
    UartFlashSendAck(UART_FLASH_ACK_SUCCESS, data, 4U);
}

/**
 * @brief  ����ʱ�ض�ȡһ֡UART����
 * @param  cmd: ָ��洢�����ֵ�ָ��
 * @param  payload: ָ��洢�����غɵĻ�����
 * @param  payload_size: �غɻ�������С
 * @param  payload_len: ָ��洢ʵ���غɳ��ȵ�ָ��
 * @retval 0=�ȴ���ʼ�ֽڳ�ʱ��1=�ɹ���2=���չ����г���
 */
static uint8_t UartFlashReadFrame(uint8_t *cmd, uint8_t *payload, uint16_t payload_size, uint16_t *payload_len)
{
    uint8_t byte;
    uint8_t checksum;
    uint8_t recv_checksum;
    uint16_t data_len;
    uint16_t i;

    // 1. �ȴ�֡��ʼ�ֽ�
    do {
        if (!UartFlashReadByte(&byte, UART_FLASH_RX_TIMEOUT_MS)) {
            return 0U; // ��ʱ
        }
    } while (byte != UART_FLASH_FRAME_START);

    checksum = UartFlashChecksumAdd(0U, byte);

    // 2. ��ȡ������
    if (!UartFlashReadByte(cmd, UART_FLASH_BYTE_TIMEOUT_MS)) {
        uart_flash_fail_code = UART_FLASH_FAIL_CMD_TIMEOUT;
        return 2U;
    }
    checksum = UartFlashChecksumAdd(checksum, *cmd);

    // 3. ��ȡ���ݳ��ȣ���ˣ�
    if (!UartFlashReadByte(&byte, UART_FLASH_BYTE_TIMEOUT_MS)) {
        uart_flash_fail_code = UART_FLASH_FAIL_LEN_TIMEOUT;
        return 2U;
    }
    checksum = UartFlashChecksumAdd(checksum, byte);
    data_len = ((uint16_t)byte) << 8;

    if (!UartFlashReadByte(&byte, UART_FLASH_BYTE_TIMEOUT_MS)) {
        uart_flash_fail_code = UART_FLASH_FAIL_LEN_TIMEOUT;
        return 2U;
    }
    checksum = UartFlashChecksumAdd(checksum, byte);
    data_len |= byte;

    // 4. ��鳤���Ƿ����
    if (data_len > payload_size) {
        uart_flash_fail_code = UART_FLASH_FAIL_LEN_OVERFLOW;
        uart_flash_fail_detail = data_len;
        // ����ʣ������
        for (i = 0U; i < (uint16_t)(data_len + 2U); i++) {
            if (!UartFlashReadByte(&byte, UART_FLASH_BYTE_TIMEOUT_MS)) {
                return 2U;
            }
        }
        return 2U;
    }

    // 5. ��ȡ�����غ�
    for (i = 0U; i < data_len; i++) {
        if (!UartFlashReadByte(&payload[i], UART_FLASH_BYTE_TIMEOUT_MS)) {
            uart_flash_fail_code = UART_FLASH_FAIL_PAYLOAD_TIMEOUT;
            uart_flash_fail_detail = i;
            return 2U;
        }
        checksum = UartFlashChecksumAdd(checksum, payload[i]);
    }

    // 6. ��ȡУ���
    if (!UartFlashReadByte(&recv_checksum, UART_FLASH_BYTE_TIMEOUT_MS)) {
        uart_flash_fail_code = UART_FLASH_FAIL_CHECKSUM_TIMEOUT;
        uart_flash_fail_detail = data_len;
        return 2U;
    }

    // 7. ��ȡ֡�����ֽ�
    if (!UartFlashReadByte(&byte, UART_FLASH_BYTE_TIMEOUT_MS)) {
        uart_flash_fail_code = UART_FLASH_FAIL_END_TIMEOUT;
        uart_flash_fail_detail = data_len;
        return 2U;
    }

    // 8. У��ͼ��
    if (recv_checksum != checksum) {
        uart_flash_fail_code = UART_FLASH_FAIL_CHECKSUM;
        uart_flash_fail_detail = data_len;
        return 2U;
    }
    // 9. ֡�����ֽڼ��
    if (byte != UART_FLASH_FRAME_END) {
        uart_flash_fail_code = UART_FLASH_FAIL_END_BYTE;
        uart_flash_fail_detail = byte;
        return 2U;
    }

    *payload_len = data_len;
    return 1U; // �ɹ�
}

/**
 * @brief  �Ӵ���ֽ������32λ����
 * @param  data: ָ��4�ֽ����ݵ�ָ��
 * @retval 32λ����
 */
static uint32_t UartFlashLoadBe32(const uint8_t *data)
{
    uint32_t value = 0U;

    value |= ((uint32_t)data[0]) << 24;
    value |= ((uint32_t)data[1]) << 16;
    value |= ((uint32_t)data[2]) << 8;
    value |= data[3];
    return value;
}

/**
 * @brief  UART Flash��¼����ѭ��
 */
void UartFlashLoader_Run(void)
{
    uint8_t cmd = 0U;
    uint16_t payload_len = 0U;
    uint32_t current_addr = 0U;
    uint8_t inited = 0U;
    uint8_t frame_state;
    uint8_t *rx_buf = uart_flash_rx_buf;
    uint16_t rx_buf_len = (uint16_t)W25QXX_SECTOR_SIZE;

    UartFlashLoader_DisableRxIrqAndFlush();

    while (1) {
        // ��ȡһ֡����
        frame_state = UartFlashReadFrame(&cmd, rx_buf, rx_buf_len, &payload_len);
        if (frame_state == 0U) {
            continue; // �ȴ���ʼ�ֽڳ�ʱ������
        }
        if (frame_state != 1U) {
            UartFlashSendAckFail(); // ���ճ���������ʧ��Ӧ��
            continue;
        }

        // ��������
        if (cmd == UART_FLASH_CMD_INIT) {
            // ��ʼ�����������ʼ��ַ
            if (payload_len == 4U) {
                current_addr = UartFlashLoadBe32(rx_buf);
            }
            // ����ַ�Ϸ��ԣ��������롢��Flash��Χ�ڣ�
            if ((payload_len == 4U)
                && ((current_addr % W25QXX_SECTOR_SIZE) == 0U)
                && (current_addr < W25QXX_FlashSizeBytes)) {
                inited = 1U;
                UartFlashSendAckSuccess();
            } else {
                inited = 0U;
                uart_flash_fail_code = UART_FLASH_FAIL_INIT_PARAM;
                UartFlashSendAckFail();
            }
        } else if (cmd == UART_FLASH_CMD_DATA) {
            // �����������������д������
            if ((inited != 0U)
                && (payload_len == W25QXX_SECTOR_SIZE)
                && ((current_addr + W25QXX_SECTOR_SIZE) <= W25QXX_FlashSizeBytes)) {
                W25QXX_Erase_Sector(current_addr / W25QXX_SECTOR_SIZE); // ��������
                W25QXX_Write_NoCheck(rx_buf, current_addr, W25QXX_SECTOR_SIZE); // д������
                current_addr += W25QXX_SECTOR_SIZE; // ���µ�ַ
                UartFlashSendAckSuccess();
            } else {
                uart_flash_fail_code = UART_FLASH_FAIL_DATA_PARAM;
                uart_flash_fail_detail = payload_len;
                UartFlashSendAckFail();
            }
        } else if (cmd == UART_FLASH_CMD_END) {
            // ���������������д���ַ
            if ((inited != 0U) && (payload_len == 0U)) {
                UartFlashSendFinalAddr(current_addr);
            } else {
                uart_flash_fail_code = UART_FLASH_FAIL_END_PARAM;
                UartFlashSendAckFail();
            }
        } else {
            // δ֪����
            uart_flash_fail_code = UART_FLASH_FAIL_UNKNOWN_CMD;
            UartFlashSendAckFail();
        }
    }
}

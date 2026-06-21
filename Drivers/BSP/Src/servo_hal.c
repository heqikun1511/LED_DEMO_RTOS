#include "servo_hal.h"

#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

/* ---------- 超时计时器 (基于 FreeRTOS 的绝对 tick) ---------- */
static TickType_t countdown_expire = 0;

void SysTick_DelayMs(uint32_t ms)
{
    osDelay(ms);
}

void SysTick_CountdownBegin(uint32_t ms)
{
    countdown_expire = xTaskGetTickCount() + pdMS_TO_TICKS(ms);
}

uint8_t SysTick_CountdownIsTimeout(void)
{
    return (xTaskGetTickCount() >= countdown_expire) ? 1 : 0;
}

void SysTick_CountdownCancel(void)
{
    countdown_expire = 0;
}

/* ---------- 串口适配 ---------- */

void USART_InitServoUsart(Usart_DataTypeDef *usart,
                          UART_HandleTypeDef *huart,
                          RingBufferTypeDef *sendBuf,
                          RingBufferTypeDef *recvBuf)
{
    usart->huart   = huart;
    usart->sendBuf = sendBuf;
    usart->recvBuf = recvBuf;
}

void Usart_SendAll(Usart_DataTypeDef *usart)
{
    uint16_t len = RingBuffer_GetByteUsed(usart->sendBuf);
    if (len == 0) return;

    /* 从 ring buffer 中读出所有字节到临时缓冲区 */
    uint8_t tmp[128];
    uint16_t to_send = (len < sizeof(tmp)) ? len : sizeof(tmp);

    for (uint16_t i = 0; i < to_send; i++) {
        tmp[i] = RingBuffer_ReadByte(usart->sendBuf);
    }

    /* 通过 HAL 串口发送 (阻塞模式) */
    HAL_UART_Transmit(usart->huart, tmp, to_send, 100);
}

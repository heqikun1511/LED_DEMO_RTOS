#ifndef SERVO_HAL_H
#define SERVO_HAL_H

#include "main.h"
#include "usart.h"
#include "ring_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== Usart_DataTypeDef ==========
   适配 HARDWARE 层舵机库需要的串口数据结构体 */
typedef struct {
    RingBufferTypeDef  *sendBuf;   /* 发送环形缓冲 */
    RingBufferTypeDef  *recvBuf;   /* 接收环形缓冲 */
    UART_HandleTypeDef *huart;     /* HAL 串口句柄 */
} Usart_DataTypeDef;

/* ========== 定时器适配 ========== */
void     SysTick_DelayMs(uint32_t ms);
void     SysTick_CountdownBegin(uint32_t ms);
uint8_t  SysTick_CountdownIsTimeout(void);
void     SysTick_CountdownCancel(void);

/* ========== 串口适配 ========== */
/* 初始化舵机串口结构体 (传入 huart 句柄 + 收发缓冲区) */
void USART_InitServoUsart(Usart_DataTypeDef *usart,
                          UART_HandleTypeDef *huart,
                          RingBufferTypeDef *sendBuf,
                          RingBufferTypeDef *recvBuf);

/* 将 sendBuf 中所有数据通过串口发送出去 */
void Usart_SendAll(Usart_DataTypeDef *usart);

#ifdef __cplusplus
}
#endif

#endif /* SERVO_HAL_H */

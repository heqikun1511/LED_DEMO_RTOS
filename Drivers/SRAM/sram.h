#ifndef _SRAM_H
#define _SRAM_H

#include <stdint.h>
#include "stm32f1xx_hal.h"

extern SRAM_HandleTypeDef SRAM_Handler;

/* FSMC Bank3 (NE3) base address */
#define Bank1_SRAM3_ADDR ((uint32_t)(0x68000000))

void SRAM_Init(void);
void FSMC_SRAM_WriteBuffer(uint8_t *pBuffer, uint32_t WriteAddr, uint32_t n);
void FSMC_SRAM_ReadBuffer(uint8_t *pBuffer, uint32_t ReadAddr, uint32_t n);

#endif

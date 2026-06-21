/* 
 * sys_tick.h — 适配 HARDWARE 舵机库的定时器接口
 * 实际实现在 servo_hal.c 中
 */
#ifndef __SYS_TICK_H
#define __SYS_TICK_H

#include <stdint.h>

void SysTick_DelayMs(uint32_t ms);
void SysTick_CountdownBegin(uint32_t ms);
uint8_t SysTick_CountdownIsTimeout(void);
void SysTick_CountdownCancel(void);

#endif /* __SYS_TICK_H */

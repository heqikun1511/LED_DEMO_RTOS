#ifndef __BSP_CAN_H__
#define __BSP_CAN_H__

#include <stdint.h>

/* 赛车数据 */
struct car_racingdata
{
  int16_t speed_rpm;       /* 转速值 (rpm) */
  int16_t torque_promille; /* 转矩值 (‰) */
  int16_t controller_temp; /* 控制器温度 (0.1°C) */
  int16_t motor_temp;      /* 电机温度 (0.1°C) */
  uint16_t dc_bus_voltage; /* 直流母线电压 (0.1V) */
  int16_t dc_bus_current;  /* 直流母线电流 (0.1A) */
};

extern volatile struct car_racingdata g_racing_data;

/* CAN 过滤器初始化 */
void can_filter_init(void);

/* CAN 发送 (支持标准帧/扩展帧) */
uint8_t CAN_SendMsg(uint32_t id, uint8_t *data, uint8_t len);

/* CAN 接收（轮询方式） */
uint8_t CAN_ReceiveMsg(uint32_t id, uint8_t *data);

#endif /* __BSP_CAN_H__ */

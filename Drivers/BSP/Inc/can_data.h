#ifndef __CAN_DATA_H
#define __CAN_DATA_H

#include <stdint.h>

/* CAN 报文 ID */
#define CAN_ID_VCU2MCU 0x08C1EF21U /* VCU → MCU (扩展帧) */
#define CAN_ID_MCU2VCU 0x0CFFC7EFU /* MCU → VCU/Meter (扩展帧) */

/* VCU → MCU 报文解析 (起始位 1-based) */
typedef struct
{
  int16_t speed_rpm;       /* 转速值, 起始位=1, 3B, -10000~10000rpm, 分辨率=1 */
  int16_t torque_promille; /* 转矩值, 起始位=... 2B, 0~1000‰ */
  uint8_t control_mode;    /* 控制模式指令 */
  uint8_t gear_status;     /* 档位状态 */
  uint8_t main_contactor;  /* 主正接触器 */
  uint16_t dc_bus_voltage; /* 直流母线电压, 2B, 0.1V */
} vcu2mcu_data_t;

/* MCU → VCU/Meter 报文解析 */
typedef struct
{
  int16_t controller_temp; /* 控制器温度, 1B, -50~200°C, 分辨率 0.1°C */
  int16_t motor_temp;      /* 电机温度, 1B, -50~200°C, 分辨率 0.1°C */
  uint16_t dc_bus_voltage; /* 直流母线电压, 2B, 0~1000V, 分辨率 0.1V */
  int16_t dc_bus_current;  /* 直流母线电流, 2B, -1600A~1600A, 分辨率 0.1A */
  uint16_t ac_current_rms; /* 交流电流有效值 */
  uint8_t overcurrent_alarm;
  uint8_t bus_overvoltage;
  uint8_t control_power_fault;
  uint8_t bus_undervoltage;
  uint8_t power_limit_alarm;
  uint8_t ctrl_temp_alarm;
  uint8_t motor_temp_alarm;
  uint8_t self_check_status;
  uint8_t comprehensive_alarm;
} mcu2vcu_data_t;

/* 全局 CAN 数据实例 */
extern volatile vcu2mcu_data_t g_can_vcu2mcu;
extern volatile mcu2vcu_data_t g_can_mcu2vcu;

/* CAN 数据解析函数 */
void can_data_parse_vcu2mcu(const uint8_t *data, vcu2mcu_data_t *out);
void can_data_parse_mcu2vcu(const uint8_t *data, mcu2vcu_data_t *out);

#endif /* __CAN_DATA_H */

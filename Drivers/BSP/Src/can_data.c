#include "can_data.h"
#include <string.h>

/* 全局 CAN 数据实例 */
volatile vcu2mcu_data_t g_can_vcu2mcu = {0};
volatile mcu2vcu_data_t g_can_mcu2vcu = {0};

/*============================================================================
 * can_data_parse_vcu2mcu
 * 解析 VCU → MCU 报文 (ID=0x08C1EF21)
 *
 * 数据格式（8字节，起始位从1开始）:
 *   Byte[0..2] : 转速值        (24-bit signed, 分辨率 1, -10000~10000rpm)
 *   Byte[2..3] : 转矩值        (16-bit signed, 分辨率 1, 0~1000‰)
 *   Byte[4]    : 控制模式指令  (bit描述)
 *   Byte[5]    : 档位状态
 *   Byte[6]    : 主正接触器/预留
 *   Byte[7]    : 直流母线电压  (8-bit or 16-bit?)
 *
 *  注意: 根据实际 DBC 调整偏移和掩码
 *============================================================================*/
void can_data_parse_vcu2mcu(const uint8_t *data, vcu2mcu_data_t *out)
{
  if (!data || !out)
    return;

  /* 转速值: Byte[0]..Byte[2] (24-bit, 小端, 有符号) */
  int32_t raw_speed = (int32_t)(data[0] | ((uint16_t)data[1] << 8) | ((uint32_t)data[2] << 16));
  if (raw_speed & 0x800000)
    raw_speed |= 0xFF000000; /* 符号扩展 */
  out->speed_rpm = (int16_t)raw_speed;

  /* 转矩值: Byte[3]..Byte[4] (16-bit, 小端, 有符号) */
  int16_t raw_torque = (int16_t)(data[3] | ((uint16_t)data[4] << 8));
  out->torque_promille = raw_torque;

  /* 控制模式指令: Byte[5] */
  out->control_mode = data[5];

  /* 档位状态: Byte[6] 低4位 */
  out->gear_status = data[6] & 0x0F;

  /* 主正接触器: Byte[6] bit4 */
  out->main_contactor = (data[6] >> 4) & 0x01;

  /* 直流母线电压: Byte[7] (8-bit, 分辨率 1V 或需调整) */
  out->dc_bus_voltage = data[7] * 10; /* 假设分辨率 0.1V */
}

/*============================================================================
 * can_data_parse_mcu2vcu
 * 解析 MCU → VCU/Meter 报文 (ID=0x0CFFC7EF)
 *
 * 数据格式（8字节）:
 *   Byte[0]    : 控制器温度        (signed, 分辨率 0.1°C, -50~200°C)
 *   Byte[1]    : 电机温度          (signed, 分辨率 0.1°C, -50~200°C)
 *   Byte[2..3] : 直流母线电压      (16-bit, 分辨率 0.1V, 0~1000V)
 *   Byte[4..5] : 直流母线电流      (16-bit signed, 分辨率 0.1A, -1600~1600A)
 *   Byte[6]    : 报警/状态位
 *   Byte[7]    : 报警/状态位
 *
 *  注意: 根据实际 DBC 调整偏移和掩码
 *============================================================================*/
void can_data_parse_mcu2vcu(const uint8_t *data, mcu2vcu_data_t *out)
{
  if (!data || !out)
    return;

  /* 控制器温度: Byte[0], 有符号, 分辨率 0.1°C, 实际温度 = raw * 0.1 */
  out->controller_temp = (int8_t)data[0];

  /* 电机温度: Byte[1], 有符号, 分辨率 0.1°C */
  out->motor_temp = (int8_t)data[1];

  /* 直流母线电压: Byte[2..3], 16-bit, 小端, 分辨率 0.1V */
  out->dc_bus_voltage = (uint16_t)(data[2] | ((uint16_t)data[3] << 8));

  /* 直流母线电流: Byte[4..5], 16-bit signed, 小端, 分辨率 0.1A */
  int16_t raw_current = (int16_t)(data[4] | ((uint16_t)data[5] << 8));
  out->dc_bus_current = raw_current;

  /* 交流电流有效值: 暂未分配, 默认为0 */
  out->ac_current_rms = 0;

  /* Byte[6] 报警位 */
  out->overcurrent_alarm = (data[6] >> 0) & 0x01;
  out->bus_overvoltage = (data[6] >> 1) & 0x01;
  out->control_power_fault = (data[6] >> 2) & 0x01;
  out->bus_undervoltage = (data[6] >> 3) & 0x01;

  /* Byte[7] 报警位 */
  out->power_limit_alarm = (data[7] >> 0) & 0x01;
  out->ctrl_temp_alarm = (data[7] >> 1) & 0x01;
  out->motor_temp_alarm = (data[7] >> 2) & 0x01;
  out->self_check_status = (data[7] >> 3) & 0x01;
  out->comprehensive_alarm = (data[7] >> 4) & 0x01;
}

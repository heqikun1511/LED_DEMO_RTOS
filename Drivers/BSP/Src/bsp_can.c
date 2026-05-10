#include "main.h"
#include "bsp_can.h"
#include "can_data.h"
#include <string.h>
#include "can.h"

/*============================================================================
 * 赛车数据全局结构体
 *   speed_rpm        : 转速值 (rpm)
 *   torque_promille  : 转矩值 (‰)
 *   controller_temp  : 控制器温度 (0.1°C)
 *   motor_temp       : 电机温度 (0.1°C)
 *   dc_bus_voltage   : 直流母线电压 (0.1V)
 *   dc_bus_current   : 直流母线电流 (0.1A)
 *============================================================================*/


volatile struct car_racingdata g_racing_data = {0};

/*============================================================================
 * can_filter_init - 配置 CAN 过滤器，只接收两个目标 ID
 *   VCU→MCU : 0x08C1EF21 (扩展帧)
 *   MCU→VCU : 0x0CFFC7EF (扩展帧)
 *============================================================================*/
void can_filter_init(void)
{
  CAN_FilterTypeDef filter = {0};

  /*
   * STM32F1 CAN filter 工作在 32-bit mask 模式:
   *   FilterIdHigh : ID[28:13] (扩展帧高16位)
   *   FilterIdLow  : ID[12:0] << 3 + IDE + RTR  (扩展帧低16位)
   *
   * 这里使用两个 filter 分别接收两个 ID
   */

  /* --- Filter 0: 接收 VCU→MCU (0x08C1EF21) --- */
  filter.FilterBank = 0;
  filter.FilterMode = CAN_FILTERMODE_IDLIST; /* 列表模式(精确匹配) */
  filter.FilterScale = CAN_FILTERSCALE_32BIT;
  /* 扩展帧 ID 在寄存器中的布局:
   *   STDID[10:0] @ Bit[31:21], EXTID[17:13] @ Bit[20:16],
   *   EXTID[12:0] @ Bit[15:3], IDE @ Bit[2], RTR @ Bit[1]
   * 简化方法: 使用 CAN_FILTERSCALE_16BIT + 双 filter 分别匹配高低16位
   */
  filter.FilterIdHigh = (uint16_t)(CAN_ID_VCU2MCU >> 13); /* ID[28:13] */
  filter.FilterIdLow = (uint16_t)((CAN_ID_VCU2MCU << 3) | (0x02 << 2) | (0x00 << 1));
  /* ID[12:0] << 3 | IDE=1(扩展) */
  filter.FilterMaskIdHigh = 0xFFFF; /* mask 全1 = 精确匹配 */
  filter.FilterMaskIdLow = 0xFFFC;  /* 只匹配 ID+IDE, 忽略 RTR */
  filter.FilterFIFOAssignment = CAN_RX_FIFO0;
  filter.FilterActivation = ENABLE;
  filter.SlaveStartFilterBank = 0;

  if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK)
  {
    Error_Handler();
  }

  /* --- Filter 1: 接收 MCU→VCU (0x0CFFC7EF) → FIFO0 --- */
  filter.FilterBank = 1;
  filter.FilterIdHigh = (uint16_t)(CAN_ID_MCU2VCU >> 13);
  filter.FilterIdLow = (uint16_t)((CAN_ID_MCU2VCU << 3) | (0x02 << 2));
  filter.FilterFIFOAssignment = CAN_RX_FIFO0;

  if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK)
  {
    Error_Handler();
  }

  /* --- Filter 2: 同样两个 ID 镜像到 FIFO1 (双缓冲防丢帧) --- */
  filter.FilterBank = 2;
  filter.FilterIdHigh = (uint16_t)(CAN_ID_VCU2MCU >> 13);
  filter.FilterIdLow = (uint16_t)((CAN_ID_VCU2MCU << 3) | (0x02 << 2));
  filter.FilterFIFOAssignment = CAN_RX_FIFO1;
  if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK)
    Error_Handler();

  filter.FilterBank = 3;
  filter.FilterIdHigh = (uint16_t)(CAN_ID_MCU2VCU >> 13);
  filter.FilterIdLow = (uint16_t)((CAN_ID_MCU2VCU << 3) | (0x02 << 2));
  filter.FilterFIFOAssignment = CAN_RX_FIFO1;
  if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK)
    Error_Handler();

  /* 启动 CAN */
  if (HAL_CAN_Start(&hcan) != HAL_OK)
  {
    Error_Handler();
  }

  /* 使能 FIFO0 + FIFO1 中断 */
  if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
    Error_Handler();
  if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO1_MSG_PENDING) != HAL_OK)
    Error_Handler();
}

/*============================================================================
 * CAN_SendMsg - 发送 CAN 数据帧 (支持扩展帧)
 *============================================================================*/
uint8_t CAN_SendMsg(uint32_t id, uint8_t *data, uint8_t len)
{
  CAN_TxHeaderTypeDef txHeader;
  uint8_t txData[8];
  uint32_t txMailbox;
  uint32_t tickstart = HAL_GetTick();

  /* 自动判断标准帧还是扩展帧 */
  if (id <= 0x7FF)
  {
    txHeader.StdId = id;
    txHeader.ExtId = 0;
    txHeader.IDE = CAN_ID_STD;
  }
  else
  {
    txHeader.StdId = 0;
    txHeader.ExtId = id;
    txHeader.IDE = CAN_ID_EXT;
  }

  txHeader.RTR = CAN_RTR_DATA;
  txHeader.DLC = (len > 8) ? 8 : len;
  txHeader.TransmitGlobalTime = DISABLE;

  memcpy(txData, data, txHeader.DLC);

  if (HAL_CAN_AddTxMessage(&hcan, &txHeader, txData, &txMailbox) != HAL_OK)
  {
    return 1;
  }

  while (HAL_CAN_IsTxMessagePending(&hcan, txMailbox))
  {
    if ((HAL_GetTick() - tickstart) > 100)
      return 1;
  }
  return 0;
}

/*============================================================================
 * CAN_ReceiveMsg - 轮询方式接收 (仅标准帧)
 *============================================================================*/
uint8_t CAN_ReceiveMsg(uint32_t id, uint8_t *data)
{
  CAN_RxHeaderTypeDef rxHeader;
  uint8_t rxData[8];

  if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &rxHeader, rxData) == HAL_OK)
  {
    if (rxHeader.IDE == CAN_ID_STD && rxHeader.StdId == id)
    {
      memcpy(data, rxData, 8);
      return 0;
    }
    return 2;
  }
  return 1;
}

/*============================================================================
 * HAL_CAN_RxFifo0MsgPendingCallback - CAN 接收中断回调
 *   根据 ID 解析数据并更新全局结构体和赛车数据结构
 *============================================================================*/
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_ptr)
{
  CAN_RxHeaderTypeDef rxHeader;
  uint8_t rxData[8];

  if (HAL_CAN_GetRxMessage(hcan_ptr, CAN_RX_FIFO0, &rxHeader, rxData) != HAL_OK)
  {
    return;
  }

  if (rxHeader.IDE != CAN_ID_EXT)
    return;

  switch (rxHeader.ExtId)
  {
    case CAN_ID_VCU2MCU:
    {
      vcu2mcu_data_t parsed;
      can_data_parse_vcu2mcu(rxData, &parsed);
      /* 更新全局赛车数据 */
      g_racing_data.speed_rpm = parsed.speed_rpm;
      g_racing_data.torque_promille = parsed.torque_promille;
      g_racing_data.dc_bus_voltage = parsed.dc_bus_voltage;
      break;
    }

    case CAN_ID_MCU2VCU:
    {
      mcu2vcu_data_t parsed;
      can_data_parse_mcu2vcu(rxData, &parsed);
      /* 更新全局赛车数据 */
      g_racing_data.controller_temp = parsed.controller_temp;
      g_racing_data.motor_temp = parsed.motor_temp;
      g_racing_data.dc_bus_voltage = parsed.dc_bus_voltage;
      g_racing_data.dc_bus_current = parsed.dc_bus_current;
      break;
    }

    default:
      break;
  }
}

/*============================================================================
 * HAL_CAN_RxFifo1MsgPendingCallback - CAN RX FIFO1 中断回调 (双缓冲)
 *   与 FIFO0 同样的解析逻辑，防止高负载下丢帧
 *============================================================================*/
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan_ptr)
{
  CAN_RxHeaderTypeDef rxHeader;
  uint8_t rxData[8];

  if (HAL_CAN_GetRxMessage(hcan_ptr, CAN_RX_FIFO1, &rxHeader, rxData) != HAL_OK)
    return;

  if (rxHeader.IDE != CAN_ID_EXT)
    return;

  switch (rxHeader.ExtId)
  {
    case CAN_ID_VCU2MCU:
    {
      vcu2mcu_data_t parsed;
      can_data_parse_vcu2mcu(rxData, &parsed);
      g_racing_data.speed_rpm       = parsed.speed_rpm;
      g_racing_data.torque_promille = parsed.torque_promille;
      g_racing_data.dc_bus_voltage  = parsed.dc_bus_voltage;
      break;
    }
    case CAN_ID_MCU2VCU:
    {
      mcu2vcu_data_t parsed;
      can_data_parse_mcu2vcu(rxData, &parsed);
      g_racing_data.controller_temp = parsed.controller_temp;
      g_racing_data.motor_temp      = parsed.motor_temp;
      g_racing_data.dc_bus_voltage  = parsed.dc_bus_voltage;
      g_racing_data.dc_bus_current  = parsed.dc_bus_current;
      break;
    }
    default:
      break;
  }
}


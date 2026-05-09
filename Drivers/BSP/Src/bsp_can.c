#include "main.h"
#include "bsp_can.h"
#include <string.h>
#include "can.h"


void can_filter_init(void)
{
  CAN_FilterTypeDef filter = {0};

  filter.FilterBank = 0;
  filter.FilterMode = CAN_FILTERMODE_IDMASK;
  filter.FilterScale = CAN_FILTERSCALE_32BIT;
  filter.FilterIdHigh = 0x0000;
  filter.FilterIdLow = 0x0000;
  filter.FilterMaskIdHigh = 0x0000; /* 掩码全 0 = 接收所有 */
  filter.FilterMaskIdLow = 0x0000;
  filter.FilterFIFOAssignment = CAN_RX_FIFO0;
  filter.FilterActivation = ENABLE;
  filter.SlaveStartFilterBank = 0;

  if (HAL_CAN_ConfigFilter(&hcan, &filter) != HAL_OK)
  {
    Error_Handler();
  }

  /* 启动 CAN */
  if (HAL_CAN_Start(&hcan) != HAL_OK)
  {
    Error_Handler();
  }

  /* 使能 RX FIFO0 消息挂起中断 */
  if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK)
  {
    Error_Handler();
  }
}


uint8_t CAN_SendMsg(uint32_t id, uint8_t *data, uint8_t len)
{
  CAN_TxHeaderTypeDef txHeader;
  uint8_t txData[8];
  uint32_t txMailbox;
  uint32_t tickstart = HAL_GetTick();

  txHeader.StdId = id;
  txHeader.ExtId = 0;
  txHeader.IDE = CAN_ID_STD;
  txHeader.RTR = CAN_RTR_DATA;
  txHeader.DLC = (len > 8) ? 8 : len;
  txHeader.TransmitGlobalTime = DISABLE;

  memcpy(txData, data, txHeader.DLC);

  if (HAL_CAN_AddTxMessage(&hcan, &txHeader, txData, &txMailbox) != HAL_OK)
  {
    return 1;
  }

  /* 等待发送完成 */
  while (HAL_CAN_IsTxMessagePending(&hcan, txMailbox))
  {
    if ((HAL_GetTick() - tickstart) > 100)
    {
      return 1; /* 超时 */
    }
  }
  return 0;
}


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
    return 2; /* ID 不匹配 */
  }
  return 1; /* 无数据 */
}

void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan_ptr)
{
  CAN_RxHeaderTypeDef rxHeader;
  uint8_t rxData[8];
  uint8_t txData[8];
  uint32_t txMailbox;

  /* 读取接收到的消息 */
  if (HAL_CAN_GetRxMessage(hcan_ptr, CAN_RX_FIFO0, &rxHeader, rxData) != HAL_OK)
  {
    return;
  }

  /* 将收到的数据原样发回 */
  CAN_TxHeaderTypeDef txHeader;
  txHeader.StdId = rxHeader.StdId; /* 使用相同的 ID */
  txHeader.ExtId = 0;
  txHeader.IDE = CAN_ID_STD;
  txHeader.RTR = CAN_RTR_DATA;
  txHeader.DLC = rxHeader.DLC;
  txHeader.TransmitGlobalTime = DISABLE;

  memcpy(txData, rxData, txHeader.DLC);
  HAL_CAN_AddTxMessage(hcan_ptr, &txHeader, txData, &txMailbox);
}

#ifndef __BSP_CAN_H__
#define __BSP_CAN_H__

#include <stdint.h>

/* CAN handle 声明 */


/* CAN 初始化 */


/* CAN 过滤器初始化 */
void can_filter_init(void);

/* CAN 发送 */
uint8_t CAN_SendMsg(uint32_t id, uint8_t *data, uint8_t len);

/* CAN 接收（轮询方式） */
uint8_t CAN_ReceiveMsg(uint32_t id, uint8_t *data);

#endif /* __BSP_CAN_H__ */

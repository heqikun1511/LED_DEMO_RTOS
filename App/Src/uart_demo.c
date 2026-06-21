#include "uart_demo.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "cmsis_os.h"
#include "main.h"
#include "task.h"
#include "usart.h"

#include "lvgl.h"
#include "screens.h"
#include "ui.h"

/* UART 接收缓冲区（环形缓冲） */
static volatile uint8_t  uart_rx_byte = 0;
static volatile uint8_t  uart_rx_buf[UART_RX_BUF_SIZE];
static volatile uint16_t uart_rx_head = 0;
static volatile uint16_t uart_rx_tail = 0;

/* 当前接收的命令行 */
static char uart_cmd_line[UART_RX_BUF_SIZE];
static uint16_t uart_cmd_idx = 0;

/* 上一次显示的命令和响应 */
static char last_cmd[64] = "";
static char last_rsp[64] = "";

/* 从环形缓冲取一个字节，返回 0 表示无数据 */
static int uart_get_byte(void)
{
    if (uart_rx_head == uart_rx_tail)
    {
        return -1;
    }
    uint8_t b = uart_rx_buf[uart_rx_tail];
    uart_rx_tail = (uart_rx_tail + 1) % UART_RX_BUF_SIZE;
    return b;
}

/* 向环形缓冲存入一个字节（从中断调用） */
static void uart_put_byte(uint8_t b)
{
    uint16_t next = (uart_rx_head + 1) % UART_RX_BUF_SIZE;
    if (next != uart_rx_tail)
    {
        uart_rx_buf[uart_rx_head] = b;
        uart_rx_head = next;
    }
}

/* 发送字符串 */
static void uart_send(const char *str)
{
    if (str == NULL) return;
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), 100);
}

/* 发送响应并显示在 LCD 上 */
static void uart_respond(const char *cmd, const char *response)
{
    /* 通过串口发回 */
    uart_send(response);
    uart_send("\r\n");

    /* 保存到 LCD 显示缓存 */
    strncpy(last_cmd, cmd, sizeof(last_cmd) - 1);
    last_cmd[sizeof(last_cmd) - 1] = '\0';
    strncpy(last_rsp, response, sizeof(last_rsp) - 1);
    last_rsp[sizeof(last_rsp) - 1] = '\0';

    /* 更新 LVGL 标签（在任务上下文中安全调用） */
    lv_label_set_text_static(objects.cpu_value, last_cmd);
    lv_label_set_text_static(objects.fps_value, last_rsp);
}

/* 处理一行命令 */
static void process_command(const char *cmd)
{
    /* 去除末尾的 \r \n */
    char clean[UART_RX_BUF_SIZE];
    strncpy(clean, cmd, sizeof(clean) - 1);
    clean[sizeof(clean) - 1] = '\0';
    size_t len = strlen(clean);
    while (len > 0 && (clean[len - 1] == '\r' || clean[len - 1] == '\n'))
    {
        clean[--len] = '\0';
    }

    if (len == 0) return;

    /* 命令解析 */
    if (strcmp(clean, "LED_ON") == 0)
    {
        HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET);
        uart_respond(clean, "LED ON OK");
    }
    else if (strcmp(clean, "LED_OFF") == 0)
    {
        HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET);
        uart_respond(clean, "LED OFF OK");
    }
    else if (strcmp(clean, "STATUS") == 0)
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "STATUS: RUNNING");
        uart_respond(clean, buf);
    }
    else if (strncmp(clean, "ECHO ", 5) == 0)
    {
        uart_respond(clean, clean + 5);
    }
    else if (strcmp(clean, "HELP") == 0)
    {
        uart_respond(clean,
            "Commands: LED_ON, LED_OFF, STATUS, ECHO <text>, HELP");
    }
    else
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "UNKNOWN: %s", clean);
        uart_respond(clean, buf);
    }
}

/* ------------------------------------------------------------------ */
/*  HAL 回调：UART1 接收完成中断（每收到 1 字节触发）                  */
/* ------------------------------------------------------------------ */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1) return;

    /* 存入环形缓冲 */
    uart_put_byte(uart_rx_byte);

    /* 继续接收下一字节 */
    HAL_UART_Receive_IT(&huart1, (uint8_t *)&uart_rx_byte, 1);
}

/* ------------------------------------------------------------------ */
/*  FreeRTOS 任务：UART1 命令处理                                      */
/* ------------------------------------------------------------------ */
void uart_demo_task(void *argument)
{
    (void)argument;

    /* 发送启动提示 */
    uart_send("\r\n--- UART1 Demo Ready ---\r\n");
    uart_send("Type HELP for commands\r\n");

    /* 启动中断接收（每次 1 字节） */
    HAL_UART_Receive_IT(&huart1, (uint8_t *)&uart_rx_byte, 1);

    for (;;)
    {
        /* 从环形缓冲读取所有可用字节，组装命令行 */
        int ch;
        while ((ch = uart_get_byte()) >= 0)
        {
            uint8_t b = (uint8_t)ch;

            if (b == '\r' || b == '\n')
            {
                if (uart_cmd_idx > 0)
                {
                    uart_cmd_line[uart_cmd_idx] = '\0';
                    process_command(uart_cmd_line);
                    uart_cmd_idx = 0;
                }
            }
            else
            {
                if (uart_cmd_idx < sizeof(uart_cmd_line) - 1)
                {
                    uart_cmd_line[uart_cmd_idx++] = (char)b;
                }
            }
        }

        osDelay(10);
    }
}

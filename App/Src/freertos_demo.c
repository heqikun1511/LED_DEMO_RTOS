#include "freertos_demo.h"

#include <stdio.h>
#include <string.h>

#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os.h"

#include "main.h"
#include "usart.h"
#include "uart_servo_lite.h"
#include "servo_hal.h"

#include "lvgl.h"
#include "screens.h"
#include "ui.h"

/* ========== 舵机串口全局变量 ========== */
static Usart_DataTypeDef  servo_usart_obj;
static Usart_DataTypeDef *servoUsart = &servo_usart_obj;

/* 收发环形缓冲区内存 */
static uint8_t servo_send_buf_mem[128];
static uint8_t servo_recv_buf_mem[256];
static RingBufferTypeDef servo_send_ring;
static RingBufferTypeDef servo_recv_ring;

/* ========== 角度换算工具 ========== */
static float RawToRelativeDegree(uint16_t raw)
{
    if (raw > 4095) return -999.0f;
    return raw * 360.0f / 4095.0f - 180.0f;
}

static uint16_t RelativeDegreeToRaw(float relDeg)
{
    float absDeg = relDeg + 180.0f;
    if (absDeg < 0.0f) absDeg = 0.0f;
    if (absDeg > 360.0f) absDeg = 360.0f;
    return (uint16_t)(absDeg * 4095.0f / 360.0f);
}

/* 清空舵机接收缓冲 */
static void ClearServoRxBuf(void)
{
    RingBuffer_Reset(servoUsart->recvBuf);
}

/* ========== LCD 数据显示 ========== */
static void update_lcd_display(uint16_t pos, uint16_t volt,
                               int16_t curr, int8_t temp,
                               const char *direction)
{
    char buf[32];

    /* 角度 —— 用 label_speed (主表盘) */
    snprintf(buf, sizeof(buf), "%+3d", (int)RawToRelativeDegree(pos));
    lv_label_set_text(objects.label_speed, buf);

    /* 电压 —— 用 battery_value */
    snprintf(buf, sizeof(buf), "%dV", volt / 10);  /* 0.1V → V */
    lv_label_set_text(objects.battery_value, buf);

    /* 电流 —— 用 rpm_value */
    snprintf(buf, sizeof(buf), "%d mA", curr);
    lv_label_set_text(objects.rpm_value, buf);

    /* 温度 —— 用 cpu_value */
    snprintf(buf, sizeof(buf), "%d C", temp);
    lv_label_set_text(objects.cpu_value, buf);

    /* 方向/状态 —— 用 fps_value */
    if (direction) {
        lv_label_set_text(objects.fps_value, direction);
    }
}

/* ========== FreeRTOS 任务：舵机控制 ========== */
void freertos_servo_task(void *argument)
{
    (void)argument;

    uint8_t servoId = 0;
    JOHO_STATUS statusCode;

    /* ---- 初始化环形缓冲 ---- */
    RingBuffer_Init(&servo_send_ring, sizeof(servo_send_buf_mem), servo_send_buf_mem);
    RingBuffer_Init(&servo_recv_ring, sizeof(servo_recv_buf_mem), servo_recv_buf_mem);

    /* ---- 初始化舵机串口适配层 (使用 USART1) ---- */
    USART_InitServoUsart(servoUsart, &huart1, &servo_send_ring, &servo_recv_ring);

    /* ---- 启动提示（通过 USART1 输出） ---- */
    const char *welcome = "\r\n=== FreeRTOS Servo Demo Started ===\r\n";
    HAL_UART_Transmit(&huart1, (uint8_t *)welcome, strlen(welcome), 100);

    /* ---- 步骤 1: Ping 舵机, 自动检测 ID (轮询 1~3) ---- */
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "\r\n[Init] Scanning servo IDs 1~3...\r\n");
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, strlen(buf), 100);

        for (uint8_t tryId = 1; tryId <= 3; tryId++) {
            snprintf(buf, sizeof(buf), "  Pinging ID=%d... ", tryId);
            HAL_UART_Transmit(&huart1, (uint8_t *)buf, strlen(buf), 100);

            statusCode = US_Ping(servoUsart, tryId);
            if (statusCode == JOHO_STATUS_SUCCESS) {
                snprintf(buf, sizeof(buf), "OK!\r\n");
                HAL_UART_Transmit(&huart1, (uint8_t *)buf, strlen(buf), 100);
                servoId = tryId;
                break;
            }
            snprintf(buf, sizeof(buf), "err=%d\r\n", statusCode);
            HAL_UART_Transmit(&huart1, (uint8_t *)buf, strlen(buf), 100);
        }

        if (servoId == 0) {
            const char *msg = "[Init] No servo found! Forcing ID=1\r\n";
            HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), 100);
            servoId = 1;
        }
    }

    /* ---- 步骤 2: 使能扭矩 ---- */
    {
        char buf[64];
        snprintf(buf, sizeof(buf), "\r\n[Init] Torque enable for ID=%d...\r\n", servoId);
        HAL_UART_Transmit(&huart1, (uint8_t *)buf, strlen(buf), 100);
        SET_Torque(servoUsart, servoId, 1);
        vTaskDelay(pdMS_TO_TICKS(200));
    }

    /* ---- 步骤 3: 回到中心位置 (2048 = 0°) ---- */
    {
        const char *msg = "\r\n[Move] Center position (2048 = 0\303\260)...\r\n";
        HAL_UART_Transmit(&huart1, (uint8_t *)msg, strlen(msg), 100);

        ClearServoRxBuf();
        USL_SetServoAngle(servoUsart, servoId, 2048.0f, 1000);
        vTaskDelay(pdMS_TO_TICKS(2000));

        /* 读取实际角度验证 */
        uint16_t pos;
        statusCode = USL_GetServoStatus(servoUsart, servoId, &pos, NULL, NULL, NULL);
        if (statusCode == JOHO_STATUS_SUCCESS) {
            char buf[64];
            snprintf(buf, sizeof(buf), "[Verify] At %u (%.1f\303\260)\r\n",
                     pos, RawToRelativeDegree(pos));
            HAL_UART_Transmit(&huart1, (uint8_t *)buf, strlen(buf), 100);
            update_lcd_display(pos, 0, 0, 0, "CENTER");
        }
    }

    /* ---- 步骤 4: 循环左转20° ↔ 右转20° ---- */
    const uint16_t leftRaw  = RelativeDegreeToRaw(-20.0f);
    const uint16_t rightRaw = RelativeDegreeToRaw( 20.0f);
    uint8_t dir_toggle = 0;

    uint32_t tick = xTaskGetTickCount();

    for (;;) {
        uint16_t target;
        const char *dirName;

        if (dir_toggle == 0) {
            target  = leftRaw;
            dirName = "<< LEFT";
        } else {
            target  = rightRaw;
            dirName = "RIGHT >>";
        }
        dir_toggle ^= 1;

        /* 发送角度指令 */
        ClearServoRxBuf();
        USL_SetServoAngle(servoUsart, servoId, (float)target, 500);

        /* 等待舵机到达 */
        vTaskDelay(pdMS_TO_TICKS(1500));

        /* 读取舵机状态：角度 + 电压 + 电流 + 温度 */
        uint16_t pos, volt;
        int16_t  curr;
        int8_t   temp;

        ClearServoRxBuf();
        statusCode = USL_GetServoStatus(servoUsart, servoId,
                                        &pos, &volt, &curr, &temp);

        if (statusCode == JOHO_STATUS_SUCCESS) {
            /* 通过串口输出 */
            char buf[80];
            snprintf(buf, sizeof(buf),
                     "  %s | Pos=%4u(%+.1f\303\260) | Volt=%u.%uV"
                     " | Curr=%+dmA | Temp=%+d\303\260C\r\n",
                     dirName, pos, RawToRelativeDegree(pos),
                     volt / 10, volt % 10, curr, temp);
            HAL_UART_Transmit(&huart1, (uint8_t *)buf, strlen(buf), 100);

            /* 更新 LCD 显示 */
            update_lcd_display(pos, volt, curr, temp, dirName);
        } else {
            char buf[48];
            snprintf(buf, sizeof(buf), "  ReadStatus err=%d\r\n", statusCode);
            HAL_UART_Transmit(&huart1, (uint8_t *)buf, strlen(buf), 100);
        }

        /* 固定周期 */
        vTaskDelayUntil(&tick, pdMS_TO_TICKS(100));
    }
}

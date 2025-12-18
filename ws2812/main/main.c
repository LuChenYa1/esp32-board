/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "led_ws2812.h"

#define WS2812_GPIO_NUM GPIO_NUM_26
#define WS2812_LED_NUM 12

/*
 * RMT 是 ESP32 的一个专用外设，本质上是一个可编程的脉冲序列发生器/分析器。
 * 全称是 Remote Control Transceiver（远程控制收发器）。
 * 它最初设计用于处理红外遥控信号，但因其灵活性，现在被广泛用于各种精确时序的信号处理。
    1、内存块：包含要发送或接收的脉冲序列描述
    2、时钟分频：可以配置精确的时钟源
    3、有限状态机：独立于 CPU 运行，不受任务调度影响
    4、中断/DMA：数据传输完成后通过中断通知 CPU
 * 为什么不用纯软件延时来驱动 WS2812？
    与 STM32 不同，ESP32 是双核处理器，运行FreeRTOS实时操作系统：
    在 RTOS 环境中，纯软件延时容易被其他任务打断
    指令和数据缓存会导致时序不一致
    ESP32 主频较高（240MHz），简单的 nop 循环很难精确控制微秒/纳秒级时序
 */

/** HSV转RGB ，暂无用到
 * @param h:色调(0-360) s饱和度(0-100) v亮度(0-100)
 * @param rgb
 * @return 无
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i)
    {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

void app_main(void)
{
    ws2812_strip_handle_t ws2812_handle = NULL;
    int index = 0;
    ws2812_init(WS2812_GPIO_NUM, WS2812_LED_NUM, &ws2812_handle);

    while (1)
    {
        // 红色跑马灯
        for (index = 0; index < 4; index++)
        {
            uint32_t r = 230, g = 20, b = 20;
            ws2812_write(ws2812_handle, index, r, g, b);
            vTaskDelay(pdMS_TO_TICKS(80));
        }
        // 绿色跑马灯
        for (index = 4; index < 8; index++)
        {
            uint32_t r = 20, g = 230, b = 20;
            ws2812_write(ws2812_handle, index, r, g, b);
            vTaskDelay(pdMS_TO_TICKS(80));
        }
        // 蓝色跑马灯
        for (index = 8; index < 12; index++)
        {
            uint32_t r = 20, g = 20, b = 230;
            ws2812_write(ws2812_handle, index, r, g, b);
            vTaskDelay(pdMS_TO_TICKS(80));
        }
        // 蓝色跑马灯
        for (index = 0; index < 4; index++)
        {
            uint32_t r = 20, g = 20, b = 230;
            ws2812_write(ws2812_handle, index, r, g, b);
            vTaskDelay(pdMS_TO_TICKS(80));
        }
        // 红色跑马灯
        for (index = 4; index < 8; index++)
        {
            uint32_t r = 230, g = 20, b = 20;
            ws2812_write(ws2812_handle, index, r, g, b);
            vTaskDelay(pdMS_TO_TICKS(80));
        }
        // 绿色跑马灯
        for (index = 8; index < 12; index++)
        {
            uint32_t r = 20, g = 230, b = 20;
            ws2812_write(ws2812_handle, index, r, g, b);
            vTaskDelay(pdMS_TO_TICKS(80));
        }
    }
}

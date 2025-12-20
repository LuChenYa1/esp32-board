/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#pragma once

#include <stdint.h>
#include "driver/rmt_encoder.h"

#ifdef __cplusplus
extern "C" {
#endif


typedef struct Ws2812Strip_t *Ws2812StripHandle_t;

/** 初始化 WS2812 外设
 * @param xGpio 控制 WS2812 的管脚
 * @param iMaxLed 控制 WS2812 的个数
 * @param pxLedHandle 返回的控制句柄
 * @return ESP_OK or ESP_FAIL
*/
esp_err_t xWs2812Init(gpio_num_t xGpio, int iMaxLed, Ws2812StripHandle_t *pxHandle);

/** 反初始化 WS2812 外设
 * @param pxHandle 初始化的句柄
 * @return ESP_OK or ESP_FAIL
*/
esp_err_t xWs2812Deinit(Ws2812StripHandle_t xHandle);

/** 向某个 WS2812 写入 RGB 数据
 * @param xHandle 句柄
 * @param ulIndex 第几个 WS2812（0开始）
 * @param r,g,b RGB 数据
 * @return ESP_OK or ESP_FAIL
*/
esp_err_t xWs2812Write(Ws2812StripHandle_t xHandle, uint32_t ulIndex, uint32_t r, uint32_t g, uint32_t b);


#ifdef __cplusplus
}
#endif

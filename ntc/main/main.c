/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "ntc.h"

#define TAG     "main"

/* 
 * ADC2 和 wifi 蓝牙不能同时用，所以一般使用ADC1。
 * 不是所有IO口都能用作ADC，具体参考datasheet。
 * 单次转换模式，准确度更高，但需要反复执行转换函数。
 * ESP32设计的ADC参考电压为1100mV,只能测量0-1100mV，如果要测量更大范围的电压，需要设置衰减倍数
 */
void app_main(void)
{
    /* 初始化并创建 ADC 转换任务得到温度*/
    temp_ntc_init();
    while(1)
    {
        /* 另起一个函数接口用于返回温度值*/
        ESP_LOGI(TAG,"current temp:%.2f",get_temp());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

}

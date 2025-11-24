 /*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 * SPDX-FileCopyrightText: 2024-2026 LuChenYa <1934600272@qq.com>
 * 
 * Project: sr04
 * Version: 2.0.0
 * Repository: https://github.com/LuChenYa1/esp32-board
 * 
 * Version History:
 * v2.0.0 (2025-11-24)
 *   - 封装SR04驱动程序, 将其作为独立文件
 *   - 统一变量、函数等的命名规范为类型前缀加大驼峰
*    - 修改注释为多行注释风格
 * 
 * v1.0.0 (2024-4-7)
 *   - 初始未封装版本
 * 
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "sr04.h"

static const char *TAG = "example";

/* TRIG和ECHO对应的GPIO */
#define HC_SR04_TRIG_GPIO  GPIO_NUM_32
#define HC_SR04_ECHO_GPIO  GPIO_NUM_33

void vSR04Task(void *pvParameters)
{
    SR04_t xSR04Sensor;
    float fDistance_CM;
    esp_err_t xRet;

    ESP_LOGI(TAG, "Initializing HC-SR04 sensor");

    // 初始化SR04传感器
    xRet = xSR04Init(&xSR04Sensor, HC_SR04_TRIG_GPIO, HC_SR04_ECHO_GPIO);
    if (xRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SR04 sensor: %s", esp_err_to_name(xRet));
        return;
    }

    ESP_LOGI(TAG, "HC-SR04 sensor initialized successfully");

    while (1) {
        // 测量距离
        xRet = xSR04MeasureDistance(&xSR04Sensor, &fDistance_CM, 1000);
        
        if (xRet == ESP_OK) {
            ESP_LOGI(TAG, "Measured distance: %.2f cm", fDistance_CM);
        } else if (xRet == ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "Measurement timeout - no echo received");
        } else if (xRet == ESP_ERR_INVALID_SIZE) {
            ESP_LOGW(TAG, "Measurement out of range");
        } else {
            ESP_LOGE(TAG, "Measurement failed: %s", esp_err_to_name(xRet));
            }

        vTaskDelay(pdMS_TO_TICKS(500));
    }

    // 清理资源（在实际应用中可能需要调用）
    // sr04_deinit(&sr04_sensor);
}


void app_main(void)
{
    xTaskCreatePinnedToCore(vSR04Task, "SR04_task", 4096, NULL, 5, NULL, 0);
}

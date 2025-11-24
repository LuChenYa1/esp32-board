#ifndef SR04_H
#define SR04_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/mcpwm_cap.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    gpio_num_t xTrigGPIO;
    gpio_num_t xEchoGPIO;
    mcpwm_cap_timer_handle_t xCaptureTimer;
    mcpwm_cap_channel_handle_t xCaptureChannel;
    TaskHandle_t xTaskHandle;
} SR04_t;

/**
 * @brief 初始化HC-SR04超声波传感器
 * 
 * @param sr04 SR04传感器结构体指针
 * @param trig_gpio Trig引脚
 * @param echo_gpio Echo引脚
 * @return esp_err_t 错误代码
 */
esp_err_t xSR04Init(SR04_t *sr04, gpio_num_t xTrigGPIO, gpio_num_t xEchoGPIO);

/**
 * @brief 反初始化HC-SR04超声波传感器
 * 
 * @param sr04 SR04传感器结构体指针
 * @return esp_err_t 错误代码
 */
esp_err_t xSR04Deinit(SR04_t *sr04);

/**
 * @brief 测量距离
 * 
 * @param sr04 SR04传感器结构体指针
 * @param distance_cm 测量的距离值（厘米）
 * @param timeout_ms 超时时间（毫秒）
 * @return esp_err_t 错误代码
 */
esp_err_t xSR04MeasureDistance(SR04_t *sr04, float *distance_cm, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* SR04_H */
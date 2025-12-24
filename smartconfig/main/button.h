#ifndef _BUTTON_H_
#define _BUTTON_H_
#include "esp_err.h"

// 按键回调函数
typedef void (*button_press_cb)(void);

// 按键配置结构体
typedef struct
{
    int iGPIONum;             // gpio号
    int iActiveLevel;         // 按下的电平
    int iLongPressTime;      // 长按时间
    button_press_cb xShortCallback; // 短按回调函数
    button_press_cb xLongCallback;  // 长按回调函数
} ButtonConfig_t;

/** 设置按键事件
 * @param pxConfig   配置结构体
 * @return ESP_OK or ESP_FAIL
 */
esp_err_t xBtnEventSet(ButtonConfig_t *pxConfig);

#endif

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "wifi_smartconfig.h"
#include "nvs_flash.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "button.h"

/* 按键事件组 */ 
static EventGroupHandle_t xPressEvent;
#define SHORT_EV BIT0 // 短按
#define LONG_EV BIT1  // 长按
#define BTN_GPIO GPIO_NUM_39

/** 长按按键回调函数
 * @param 无
 * @return 无
 */
void vLongPressHandle(void)
{
    xEventGroupSetBits(xPressEvent, LONG_EV);
}

void app_main(void)
{
    nvs_flash_init();  // 初始化NVS
    vWifiInit(); // 初始化wifi
    xPressEvent = xEventGroupCreate();
    ButtonConfig_t xBtnConfig =
    {
        .iGPIONum = BTN_GPIO,   // gpio号
        .iActiveLevel = 0,      // 按下的电平
        .iLongPressTime = 3000, // 长按时间
        .xShortCallback = NULL,            // 短按回调函数
        .xLongCallback = vSmartConfigStart // 长按回调函数
    };
    xBtnEventSet(&xBtnConfig); // 添加按键响应事件处理
    EventBits_t xEventBits;
    while (1){
        xEventBits = xEventGroupWaitBits(xPressEvent, LONG_EV, pdTRUE, pdFALSE, portMAX_DELAY);
        if (xEventBits & LONG_EV){
            vSmartConfigStart(); // 检测到长按事件，启动smartconfig
        }
    }
}

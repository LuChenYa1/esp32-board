/**
 * @file button.c
 * @brief 按键处理模块
 * @author  (LuChenYa1@github.com)
 * @version 0.0.2
 * @date 2025-12-24
 * @copyright Copyright © 2021-2021 xqyjlj <LuChenYa1@github.com>
 * 
 * 已格式化
 * 已修改变量命名
 * 已修改注释方式
 * 中英文隔开
 * 花括号不换行
 */
#include "button.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_timer.h"
#include <stdio.h>
#include <string.h>
static const char *TAG = "button";

typedef enum
{
    BUTTON_RELEASE,         // 按键没有按下
    BUTTON_PRESS,           // 按键按下了，等待一点延时（消抖），然后触发短按回调事件，进入 BUTTON_HOLD
    BUTTON_HOLD,            // 按住状态，如果时间长度超过设定的超时计数，将触发长按回调函数，进入 BUTTON_LONG_PRESS_HOLD
    BUTTON_LONG_PRESS_HOLD, // 此状态等待电平消失，回到 BUTTON_RELEASE 状态
} ButtonState_e;

typedef struct Button
{
    ButtonConfig_t xBtnConfig; // 按键配置
    ButtonState_e eState;      // 当前状态
    int iPressCount;           // 按下计数
    struct Button *pxNext;     // 下一个按键参数
} ButtonDevice_t;

/* 按键处理列表 */ 
static ButtonDevice_t *pxBtnHead = NULL;

/* 消抖过滤时间 */ 
#define FILITER_TIMER 20

/* 定时器释放运行标志 */ 
static bool IsTimerRunning = false;

/* 定时器句柄 */ 
static esp_timer_handle_t xBtnTimerHandle;

static void prvBtnHandle(void *pvParam);

/** 设置按键事件
 * @pvParam pxConfig   配置结构体
 * @return ESP_OK or ESP_FAIL
 */
esp_err_t xBtnEventSet(ButtonConfig_t *pxConfig)
{
    gpio_config_t xGPIOConfig = {
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_INPUT,
        .pin_bit_mask = (1ull << pxConfig->iGPIONum),
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&xGPIOConfig));
    ButtonDevice_t *pxBtn = (ButtonDevice_t *)malloc(sizeof(ButtonDevice_t));
    if (!pxBtn){
        return ESP_FAIL;
    }
    memset(pxBtn, 0, sizeof(ButtonDevice_t));
    if (!pxBtnHead){
        pxBtnHead = pxBtn;
        ESP_LOGI(TAG, "first btn add");
    }else{
        ButtonDevice_t *pxBtnDev = pxBtnHead;
        while (pxBtnDev->pxNext != NULL){
            pxBtnDev = pxBtnDev->pxNext;
        }
        pxBtnDev->pxNext = pxBtn;
        ESP_LOGI(TAG, "btn add");
    }
    memcpy(&pxBtn->xBtnConfig, pxConfig, sizeof(ButtonConfig_t));
    /* 创建定时器 */
    if (false == IsTimerRunning){ // 防止重复创建定时器
        ESP_LOGI(TAG, "run button timer");
        /* static 是否多余？ */
        static int iTimerInterval = 5;
        esp_timer_create_args_t xBtnTimer;
        xBtnTimer.arg = (void *)iTimerInterval;
        xBtnTimer.callback = prvBtnHandle;
        xBtnTimer.dispatch_method = ESP_TIMER_TASK;
        xBtnTimer.name = "prvBtnHandle";
        esp_timer_create(&xBtnTimer, &xBtnTimerHandle);
        esp_timer_start_periodic(xBtnTimerHandle, 5000);
        IsTimerRunning = true;
    }

    return ESP_OK;
}

static void prvBtnHandle(void *pvParam)
{
    int iIncCount = (int)pvParam;
    ButtonDevice_t *pxBtnTarget = pxBtnHead;
    /* 遍历每一个按键设备节点 */
    for (; pxBtnTarget; pxBtnTarget = pxBtnTarget->pxNext){
        switch (pxBtnTarget->eState){
        case BUTTON_RELEASE: // 按键没有按下
            if (gpio_get_level(pxBtnTarget->xBtnConfig.iGPIONum) == pxBtnTarget->xBtnConfig.iActiveLevel){
                pxBtnTarget->iPressCount += iIncCount;
                pxBtnTarget->eState = BUTTON_PRESS;
            }
            break;
        case BUTTON_PRESS: // 按键按下了，等待一点延时（消抖），然后触发短按回调事件，进入 BUTTON_HOLD
            if (gpio_get_level(pxBtnTarget->xBtnConfig.iGPIONum) == pxBtnTarget->xBtnConfig.iActiveLevel){
                pxBtnTarget->iPressCount += iIncCount;
                if (pxBtnTarget->iPressCount >= FILITER_TIMER){ // 防止引脚电平抖动导致误判为按下状态
                    ESP_LOGI(TAG, "short press detect");
                    if (pxBtnTarget->xBtnConfig.xShortCallback)
                        pxBtnTarget->xBtnConfig.xShortCallback();
                    pxBtnTarget->eState = BUTTON_HOLD;
                }
            }
            else
                pxBtnTarget->eState = BUTTON_RELEASE;
            break;
        case BUTTON_HOLD: // 按住状态，如果时间长度超过设定的超时计数，将触发长按回调函数，进入 BUTTON_LONG_PRESS_HOLD
            if (gpio_get_level(pxBtnTarget->xBtnConfig.iGPIONum) == pxBtnTarget->xBtnConfig.iActiveLevel){
                pxBtnTarget->iPressCount += iIncCount;
                if (pxBtnTarget->iPressCount >= pxBtnTarget->xBtnConfig.iLongPressTime){
                    ESP_LOGI(TAG, "long press detect");
                    if (pxBtnTarget->xBtnConfig.xLongCallback)
                        pxBtnTarget->xBtnConfig.xLongCallback();
                    pxBtnTarget->eState = BUTTON_LONG_PRESS_HOLD;
                }
            }
            else
                pxBtnTarget->eState = BUTTON_RELEASE;
            break;
        case BUTTON_LONG_PRESS_HOLD: // 此状态等待电平消失，回到 BUTTON_RELEASE 状态
            if (gpio_get_level(pxBtnTarget->xBtnConfig.iGPIONum) != pxBtnTarget->xBtnConfig.iActiveLevel){
                pxBtnTarget->eState = BUTTON_RELEASE;
                pxBtnTarget->iPressCount = 0;
            }
            break;
        default:
            break;
        }
    }
}

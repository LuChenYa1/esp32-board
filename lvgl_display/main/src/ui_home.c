/*
 * 已完成变量命名修改
 * 已完成注释修改
 * 已完成格式化
 * 已完成中英文间距修改
*/
// 首先包含系统头文件
#include <stdio.h>
#include <stdint.h>
#include <string.h>
// 然后包含ESP-IDF头文件
#include "esp_log.h"
#include "esp_err.h"
// 再包含LVGL头文件
#include "lvgl.h"
// 最后包含自定义头文件
#include "ui_home.h"
#include "led_ws2812.h"
#include "dht11.h"

/* 图像声明放在函数外部 */ 
LV_IMG_DECLARE(temp_img)
LV_IMG_DECLARE(humidity_img)

#define WS2812_NUM 12

/* 全局变量声明 */ 
static lv_obj_t *pxTempImage;
static lv_obj_t *pxHumidityImage;

static lv_obj_t *pxTempLabel;
static lv_obj_t *pxHumidityLabel;

static lv_obj_t *pxLightSlider;

static lv_timer_t *pxDht11Timer;

Ws2812StripHandle_t xWs2812Handle;


/**
 * @brief 光线滑块事件回调函数，用于处理滑块数值变化事件并控制 LED 亮度
 * @param e 指向事件对象的指针，包含事件相关信息
 */
void vLightSliderEventCallback(lv_event_t *e)
{
    lv_event_code_t xCode = lv_event_get_code(e);
    switch (xCode){
    case LV_EVENT_VALUE_CHANGED:
        /* 获取触发事件的滑块对象及其当前值，并转换为 RGB 亮度值 */
        lv_obj_t *pxSliderObj = lv_event_get_target(e);
        int32_t lValue = lv_slider_get_value(pxSliderObj);
        /* 改用满量程 0-255 */ 
        uint32_t ulRgbValue = 255 * (lValue / 100.0);
        ESP_LOGI("SLIDER", "Value: %ld%%, RGB: %ld", lValue, ulRgbValue);
        /* 根据滑块值设置所有 WS2812 LED灯的亮度 */
        for (int iLedIndex = 0; iLedIndex < WS2812_NUM; iLedIndex++){
            xWs2812Write(xWs2812Handle, iLedIndex, ulRgbValue, ulRgbValue, ulRgbValue);
        }
        
        break;
    default:
        break;
    }
}

/**
 * @brief DHT11 传感器数据获取定时器回调函数
 *
 * 该函数作为LVGL定时器的回调函数，定期读取 DHT11 温湿度传感器的数据，
 * 并将温度和湿度值显示在对应的标签控件上。
 *
 * @param t 定时器句柄指针，指向触发此回调的定时器对象
 */
void vDht11TimerCallback(struct _lv_timer_t *t)
{
    int iTemp;
    int iHumidity;

    /* 尝试获取 DHT11 传感器的温湿度数据 */
    if (iDht11StartGet(&iTemp, &iHumidity)){
        char cDisplayBuffer[32];
        /* 格式化温度值并更新温度标签显示 */
        //ESP_LOGI("DHT11", "Temperature: %.1f C", (float)iTemp / 10.0);
        snprintf(cDisplayBuffer, sizeof(cDisplayBuffer), "%.1f", (float)iTemp / 10.0);
        lv_label_set_text(pxTempLabel, cDisplayBuffer);
        /* 格式化湿度值并更新湿度标签显示 */
        //ESP_LOGI("DHT11", "Humidity: %d%%", iHumidity);
        snprintf(cDisplayBuffer, sizeof(cDisplayBuffer), "%d%%", iHumidity);
        lv_label_set_text(pxHumidityLabel, cDisplayBuffer);
    }
}

void vUIHomeCreate(void)
{
    /* 设置背景色 */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    /* 创建调光用的进度条 */
    pxLightSlider = lv_slider_create(lv_scr_act());
    lv_obj_set_pos(pxLightSlider, 60, 200);
    lv_obj_set_size(pxLightSlider, 150, 15);
    lv_slider_set_range(pxLightSlider, 0, 100);
    lv_obj_add_event_cb(pxLightSlider, vLightSliderEventCallback, LV_EVENT_VALUE_CHANGED, NULL);
    /* 创建温度图片 */
    pxTempImage = lv_img_create(lv_scr_act());
    lv_img_set_src(pxTempImage, &temp_img);
    lv_obj_set_pos(pxTempImage, 40, 40);
    /** 创建湿度图片 */
    pxHumidityImage = lv_img_create(lv_scr_act());
    lv_img_set_src(pxHumidityImage, &humidity_img);
    lv_obj_set_pos(pxHumidityImage, 40, 110);
    /* 创建温度 label */
    pxTempLabel = lv_label_create(lv_scr_act());
    lv_obj_set_pos(pxTempLabel, 110, 40);
    lv_obj_set_style_text_font(pxTempLabel, &lv_font_montserrat_38, 0);
    /* 创建湿度 label */
    pxHumidityLabel = lv_label_create(lv_scr_act());
    lv_obj_set_pos(pxHumidityLabel, 110, 110);
    lv_obj_set_style_text_font(pxHumidityLabel, &lv_font_montserrat_38, 0);
    /* 创建定时器 */
    pxDht11Timer = lv_timer_create(vDht11TimerCallback, 2000, NULL);
    /* 初始化 WS2812 */
    /* 原定的 ws2812 RGB 引脚 为 GPIO 26, 但和 LCD 背光冲突，故改为 GPIO 32 */
    xWs2812Init(GPIO_NUM_32, WS2812_NUM, &xWs2812Handle);
    /* 初始化 DHT11 */
    vDht11Init(GPIO_NUM_25);
}

/*
 * 已完成变量命名修改
 * 已完成注释修改
 * 已完成格式化
 * 已完成中英文间距修改
*/
#include "esp_log.h"
#include "driver/gpio.h"
#include "lvgl.h"

static lv_obj_t *pxLedButton = NULL;
static lv_obj_t *pxLedLabel = NULL;

/**
 * @brief LED按钮事件回调函数
 * @param e 指向事件对象的指针
 * @return 无返回值
 *
 * 该函数处理 LVGL 库中按钮点击事件，用于控制 LED 灯的开关状态。
 * 当按钮被点击时，会切换 LED 的状态并在 GPIO 27引脚上输出相应的电平。
 */
void vLvLedButtonCallback(lv_event_t *e)
{
    /* 获取事件代码 */
    lv_event_code_t code = lv_event_get_code(e);
    /* 定义静态变量存储 LED 当前状态，初始为关闭状态 */
    static uint32_t ulLedState = 0;
    /* 根据不同的事件类型进行处理 */
    switch (code){
    case LV_EVENT_CLICKED:
        /* 点击事件处理：切换LED状态 */
        ulLedState = ulLedState ? 0 : 1;
        gpio_set_level(GPIO_NUM_27, ulLedState);
        break;
    default:
        break;
    }
}

void vUILedCreate(void)
{
    /* 设置背景色为黑色 */
    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_black(), 0);
    /* 创建按钮 */
    pxLedButton = lv_btn_create(lv_scr_act());
    lv_obj_align(pxLedButton, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(pxLedButton, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_set_size(pxLedButton, 80, 40);
    /* 创建标签 */
    pxLedLabel = lv_label_create(pxLedButton);
    lv_obj_align(pxLedLabel, LV_ALIGN_CENTER, 0, 0);
    lv_label_set_text(pxLedLabel, "LED");
    /* 设置标签字体 */
    lv_obj_set_style_text_font(pxLedLabel, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    /* 添加点击事件 */
    lv_obj_add_event_cb(pxLedButton, vLvLedButtonCallback, LV_EVENT_CLICKED, NULL);
}

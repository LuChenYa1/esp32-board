#include "stdio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "lv_port.h"
#include "lv_demos.h"
#include "st7789_driver.h"
#include "driver/gpio.h"
#include "ui_led.h"
#include "ui_home.h"

/* .c 文件中用 static 修饰函数，则该函数只能在本 .c 文件中调用 */
void app_main()
{
    xLvPortInit();             // 初始化lvgl
    vSt7789LcdBackLight(true); // 打开背光

    // lv_demo_widgets(); // 启动官方 demo
    // lv_demo_music();

#if 0
    vUILedCreate();
    gpio_config_t xLedGPIO = {
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .pin_bit_mask = (1ull << GPIO_NUM_27),
        .intr_type = GPIO_INTR_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&xLedGPIO);
    gpio_set_level(GPIO_NUM_27, 0);
#else
    vUIHomeCreate();
#endif

    while (1)
    {
        lv_task_handler();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

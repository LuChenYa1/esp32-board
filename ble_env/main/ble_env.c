#include <stdio.h>
#include "esp_err.h"
#include "ble_app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include "dht11.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define DHT11_GPIO      25              // DHT11引脚定义

#define LED_GPIO        27              // LED定义

#define TAG     "main"

void mytask1(void* param)
{
    int tempx10 = 0;
    int humidity = 0;
    while(1)
    {
        if(DHT11_StartGet(&tempx10,&humidity))
        {
            ESP_LOGI(TAG,"temperature:%.1f,humidity:%i%%",(float)tempx10/10.0,humidity);
            ble_set_temp_value(tempx10&0xffff);
            ble_set_humidity_value(humidity&0xffff);
        }
        vTaskDelay(pdMS_TO_TICKS(1500));
    }
}

void app_main(void)
{
    //初始化nvs
    esp_err_t ret_val = ESP_OK;
    ret_val = nvs_flash_init();
    if (ret_val == ESP_ERR_NVS_NO_FREE_PAGES || ret_val == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret_val = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret_val );

    //初始化蓝牙
    ble_cfg_net_init();

    //初始化DHT11
    DHT11_Init(DHT11_GPIO);

    //初始化GPIO
    gpio_config_t led_cfg = 
    {
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = (1ull<<LED_GPIO),
    };
    gpio_config(&led_cfg);

    //新建任务
    xTaskCreatePinnedToCore(mytask1,"mytask",4096,NULL,3,NULL,1);
}

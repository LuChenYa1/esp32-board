#include <stdio.h>
#include "esp_err.h"
#include "ble_app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

void mytask1(void* param)
{
    uint16_t count1 = 1;
    uint16_t count2 = 100;
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(3000));
        ble_set_ch1_value(count1++);
        vTaskDelay(pdMS_TO_TICKS(700));
        ble_set_ch2_value(count2++);
    }
}

void app_main(void)
{
    ESP_ERROR_CHECK(nvs_flash_init());
    ble_cfg_net_init();
    xTaskCreatePinnedToCore(mytask1,"mytask",4096,NULL,3,NULL,1);
}

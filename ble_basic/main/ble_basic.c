#include <stdio.h>
#include "ble_app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void mytask1(void* param)
{
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void app_main(void)
{
    ble_cfg_net_init();
    xTaskCreatePinnedToCore(mytask1,"mytask",4096,NULL,3,NULL,1);
}

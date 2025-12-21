#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "main";

/* NVS 命名空间 */
#define NVS_BOB_NAMESPACE "Bob"   // namespace 最长15字节
#define NVS_JOHN_NAMESPACE "John" // namespace 最长15字节

#define NVS_AGE_KEY "age" // 年龄键名
#define NVS_SEX_KEY "sex" // 性别键名

void app_main(void)
{
    /* 初始化 NVS  */ 
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        /* NVS 出现错误，执行擦除 */ 
        ESP_ERROR_CHECK(nvs_flash_erase());
        /* 重新尝试初始化 */ 
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    char cReadBuffer[64];
    size_t len = 0;

    /* 以字节方式写入到 NVS 中 */ 
    xWriteNvsStr(NVS_BOB_NAMESPACE, NVS_SEX_KEY, "female");
    xWriteNvsStr(NVS_JOHN_NAMESPACE, NVS_SEX_KEY, "male");

    /* 读取 NVS_BOB_NAMESPACE 命名空间中的 SEX 键值 */ 
    len = xReadNvsStr(NVS_BOB_NAMESPACE, NVS_SEX_KEY, cReadBuffer, 64);
    if (len)
        ESP_LOGI(TAG, "Read BOB SEX:%s", cReadBuffer);
    else
        ESP_LOGI(TAG, "Read BOB SEX fail, please perform nvs_erase_key and try again");

    /* 读取 NVS_JOHN_NAMESPACE 命名空间中的 SEX 键值 */ 
    len = xReadNvsStr(NVS_JOHN_NAMESPACE, NVS_SEX_KEY, cReadBuffer, 64);
    if (len)
        ESP_LOGI(TAG, "Read JOHN SEX:%s", cReadBuffer);
    else
        ESP_LOGI(TAG, "Read JOHN SEX fail,please perform nvs_erase_key and try again");

    uint8_t ucBlobBuffer[32];
    ucBlobBuffer[0] = 19;
    /* 以二进制( 数字 )方式写入年龄键值 */ 
    xWriteNvsBlob(NVS_BOB_NAMESPACE, NVS_AGE_KEY, ucBlobBuffer, 1);
    ucBlobBuffer[0] = 23;
    xWriteNvsBlob(NVS_JOHN_NAMESPACE, NVS_AGE_KEY, ucBlobBuffer, 1);

    /* 以二进制方式读取 */ 
    len = xReadNvsBlob(NVS_BOB_NAMESPACE, NVS_AGE_KEY, ucBlobBuffer, 32);
    if (len)
        ESP_LOGI(TAG, "Read BOB age:%d", ucBlobBuffer[0]);
    else
        ESP_LOGI(TAG, "Read BOB age fail, please perform nvs_erase_key and try again");

    /* 以二进制方式读取 */ 
    len = xReadNvsBlob(NVS_JOHN_NAMESPACE, NVS_AGE_KEY, ucBlobBuffer, 32);
    if (len)
        ESP_LOGI(TAG, "Read JOHN age:%d", ucBlobBuffer[0]);
    else
        ESP_LOGI(TAG, "Read JOHN age fail, please perform nvs_erase_key and try again");

    while (1){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

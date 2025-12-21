#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_partition.h"
#include <string.h>

static const char *TAG = "main";

#define USER_PARTITION_TYPE 0x40    // 自定义的分区类型
#define USER_PARTITION_SUBTYPE 0x01 // 自定义的分区子类型

/* 分区指针 */ 
static const esp_partition_t *pxPartitionRes = NULL; // 承载用户自定义分区的首地址

/* 读取缓存 */ 
static char cEspBuffer[1024];

void app_main(void)
{
    /* 找到符合要求的第一个分区，返回分区指针，后续用到这个指针进行各种操作 */ 
    pxPartitionRes = esp_partition_find_first(USER_PARTITION_TYPE, USER_PARTITION_SUBTYPE, "user");
    if (pxPartitionRes == NULL){
        ESP_LOGI(TAG, "Can't find partition,return");
        return;
    }
    /* 擦除 */ 
    ESP_ERROR_CHECK(esp_partition_erase_range(pxPartitionRes, 0x0, 0x1000));
    /* 测试字符串 */ 
    const char *test_str = "this is for test string";
    /* 从分区偏移位置0x0写入字符串 */ 
    ESP_ERROR_CHECK(esp_partition_write(pxPartitionRes, 0x00, test_str, strlen(test_str)));
    /* 从分区偏移位置0x0读取字符串 */ 
    ESP_ERROR_CHECK(esp_partition_read(pxPartitionRes, 0x00, cEspBuffer, strlen(test_str)));
    ESP_LOGI(TAG, "Read partition str:%s", cEspBuffer);
    while (1){
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

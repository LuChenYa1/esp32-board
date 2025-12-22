#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "simple_wifi_sta.h"


void app_main(void)
{
    /* NVS 初始化（WIFI 底层驱动有用到 NVS，所以这里要初始化） */ 
    nvs_flash_init();
    /* wifi STA 工作模式初始化 */   
    xWifiSTAInit();
    while (1){
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

// // debug.c - 只用于扫描，不连接
// // hotspot_test.c
// #include <stdio.h>
// #include <string.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "nvs_flash.h"
// #include "esp_wifi.h"
// #include "esp_event.h"
// #include "esp_log.h"
// #include "esp_netif.h"


// // 尝试连接手机热点
// void connect_to_hotspot(void)
// {
//     wifi_config_t wifi_config = {
//         .sta = {
//             .ssid = "testwifi",          // 你的手机热点名称
//             .password = "12345678",      // 你的热点密码
//             .scan_method = WIFI_ALL_CHANNEL_SCAN,
//             .sort_method = WIFI_CONNECT_AP_BY_SIGNAL,
//             .threshold.rssi = -127,
//             .threshold.authmode = WIFI_AUTH_WPA2_PSK,
//         },
//     };
    
//     // 打印配置信息
//     printf("\n尝试连接热点:\n");
//     printf("SSID: %s\n", wifi_config.sta.ssid);
//     printf("密码: %s\n", wifi_config.sta.password);
    
//     // 尝试不同信道
//     for (int channel = 1; channel <= 13; channel++) {
//         printf("尝试信道 %d...\n", channel);
        
//         wifi_config.sta.channel = channel;
//         esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
//         esp_wifi_connect();
        
//         vTaskDelay(pdMS_TO_TICKS(3000)); // 等待3秒
        
//         wifi_ap_record_t ap_info;
//         if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
//             printf("成功连接到热点！\n");
//             printf("SSID: %s, RSSI: %d dBm, 信道: %d\n", 
//                    ap_info.ssid, ap_info.rssi, ap_info.primary);
//             return;
//         }
//     }
//     printf("连接失败，热点未找到\n");
// }

// void app_main(void)
// {
//     // 初始化NVS
//     ESP_ERROR_CHECK(nvs_flash_init());
    
//     // 初始化网络接口
//     ESP_ERROR_CHECK(esp_netif_init());
//     ESP_ERROR_CHECK(esp_event_loop_create_default());
//     esp_netif_create_default_wifi_sta();
    
//     // 初始化WiFi
//     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
//     ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    
//     // 设置STA模式
//     ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
//     ESP_ERROR_CHECK(esp_wifi_start());
    
//     // 等待WiFi启动
//     vTaskDelay(pdMS_TO_TICKS(3000));
    
//     while(1) {
//         printf("\n\n=== 开始扫描 ===\n");
        
//         // 扫描所有信道
//         wifi_scan_config_t scan_conf = {
//             .ssid = NULL,
//             .bssid = NULL,
//             .channel = 0,  // 0表示扫描所有信道
//             .show_hidden = true
//         };
        
//         // 开始扫描
//         esp_err_t ret = esp_wifi_scan_start(&scan_conf, true);
//         if (ret != ESP_OK) {
//             printf("扫描失败: %s\n", esp_err_to_name(ret));
//             vTaskDelay(pdMS_TO_TICKS(5000));
//             continue;
//         }
        
//         // 获取结果
//         uint16_t ap_count = 0;
//         esp_wifi_scan_get_ap_num(&ap_count);
//         printf("找到 %d 个网络:\n", ap_count);
        
//         if (ap_count > 0) {
//             wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * ap_count);
//             if (ap_records) {
//                 esp_wifi_scan_get_ap_records(&ap_count, ap_records);
                
//                 for (int i = 0; i < ap_count; i++) {
//                     printf("%2d. %-32s (信号: %3d dBm, 信道: %2d)\n", 
//                            i+1, ap_records[i].ssid, ap_records[i].rssi, ap_records[i].primary);
//                 }
//                 free(ap_records);
//             }
//         }
        
//         printf("=== 扫描结束 ===\n");
        
//         // 尝试连接手机热点
//         connect_to_hotspot();
        
//         vTaskDelay(pdMS_TO_TICKS(10000)); // 10秒后再次尝试
//     }
// }

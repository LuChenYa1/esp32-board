/**
 * 格式化
 * 修改变量命名
 * 修改注释方式
 * 隔开中英文
 * 大花括号不换行
 */
#include "simple_wifi_sta.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"

/* 需要把这两个修改成你家 WIFI，测试是否连接成功 */ 
#define DEFAULT_WIFI_SSID "testwifi"
#define DEFAULT_WIFI_PASSWORD "12345678"

static const char *TAG = "wifi";

// 定义一个事件组，用于通知 main 函数 WIFI 连接成功
#define WIFI_CONNECT_BIT BIT0
extern EventGroupHandle_t xWifiEventGroup;
static void prvReconnectTask(void *arg)
{
    // 延迟2秒
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_wifi_connect();
    vTaskDelete(NULL);
}


/** 事件回调函数
 * @param arg         用户传递的参数
 * @param xEventBase  事件类别
 * @param lEventId    事件 ID
 * @param pvEventData 事件携带的数据
 * @return 无
 */
static void prvEventHandler(void *arg, esp_event_base_t xEventBase, int32_t lEventId, void *pvEventData)
{
    if (xEventBase == WIFI_EVENT){
        switch (lEventId){
        case WIFI_EVENT_STA_START: // WIFI 以 STA 模式启动后触发此事件
            esp_wifi_connect();    // 启动 WIFI 连接
            break;
        case WIFI_EVENT_STA_CONNECTED: // WIFI 连上路由器后，触发此事件
            ESP_LOGI(TAG, "connected to AP");
            break;
        case WIFI_EVENT_STA_DISCONNECTED: // WIFI 从路由器断开连接后触发此事件
            wifi_event_sta_disconnected_t *disconnect = (wifi_event_sta_disconnected_t *)pvEventData;
            ESP_LOGI(TAG, "Disconnected from AP, reason: %d", disconnect->reason);
            /* 常见断开原因代码 */
            switch (disconnect->reason) {
            case WIFI_REASON_NO_AP_FOUND:          /* 201 */
                /* 检查是否为2.4GHz 频段, 如果不是, 需要修改频段为2.4GHz 后重启手机 */
                ESP_LOGE(TAG, "未找到AP: SSID可能错误或AP隐藏");
                break;
            case WIFI_REASON_AUTH_FAIL:            /* 202 */
                ESP_LOGE(TAG, "认证失败: 密码错误");
                break;
            case WIFI_REASON_ASSOC_FAIL:           /* 203 */
                ESP_LOGE(TAG, "关联失败");
                break;
            case WIFI_REASON_HANDSHAKE_TIMEOUT:    /* 204 */
                ESP_LOGE(TAG, "握手超时");
                break;
            default:
                ESP_LOGW(TAG, "其他断开原因: %d", disconnect->reason);
                break;
            }
            /* 创建一个只执行一次的任务来延迟重连，避免阻塞事件任务 */ 
            xTaskCreate(prvReconnectTask, "reconnect", 2048, NULL, 5, NULL);
            ESP_LOGI(TAG, "connect to the AP fail,retry now");
            break;
        default:
            break;
        }
    }
    if (xEventBase == IP_EVENT){ // IP 相关事件
        switch (lEventId){
        case IP_EVENT_STA_GOT_IP: // 只有获取到路由器分配的 IP，才认为是连上了路由器
            ESP_LOGI(TAG, "get ip address");
            /* 设置事件组，通知 main 函数 WIFI 已连接成功 */
            xEventGroupSetBits(xWifiEventGroup, WIFI_CONNECT_BIT);
            break;
        }
    }
}


/* WIFI STA初始化 */ 
esp_err_t xWifiSTAInit(void)
{
    ESP_ERROR_CHECK(esp_netif_init());                // 用于初始化 tcpip 协议栈
    ESP_ERROR_CHECK(esp_event_loop_create_default()); // 创建一个默认系统事件调度循环，之后可以注册回调函数来处理系统的一些事件
    esp_netif_create_default_wifi_sta();              // 使用默认配置创建 STA 对象
    /* 初始化 WIFI */ 
    wifi_init_config_t xConfig = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&xConfig));
    /* 注册事件 */ 
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &prvEventHandler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &prvEventHandler, NULL));
    /* WIFI 配置 */ 
    wifi_config_t xWifiConfig = {
        .sta = {
            .ssid = DEFAULT_WIFI_SSID,                // WIFI 的 SSID
            .password = DEFAULT_WIFI_PASSWORD,        // WIFI 密码
            .threshold.authmode = WIFI_AUTH_WPA2_PSK, // 加密方式

            .pmf_cfg ={
                .capable = true,
                .required = false
            },
        },
    };
    /* 启动 WIFI */ 
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));               // 设置工作模式为 STA
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &xWifiConfig)); // 设置 wifi 配置
    ESP_ERROR_CHECK(esp_wifi_start());                               // 启动 WIFI

    ESP_LOGI(TAG, "wifi_init_sta finished.");
    return ESP_OK;
}

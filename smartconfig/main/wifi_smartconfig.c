/**
 * 格式化文档
 * 修改注释方式 
 * 中英文隔开
 * 修改变量命名
 * 花括号不换行
 */
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_eap_client.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_smartconfig.h"

/* 一个事件组，用于表示 */ 
static EventGroupHandle_t xWifiEventGroup;

/* 连接成功事件 */ 
static const int CONNECTED_BIT = BIT0;

/* smartconfig 完成事件 */ 
static const int ESPTOUCH_DONE_BIT = BIT1;

static const char *TAG = "smartconfig_example";

#define NVS_WIFI_NAMESPACE_NAME "DEV_WIFI"
#define NVS_SSID_KEY "ssid"
#define NVS_PASSWORD_KEY "password"

/* 缓存一份 ssid */ 
static char cSsidValue[33] = {0};

/* 缓存一份 password */ 
static char cPasswordValue[65] = {0};

/* 用一个标志来表示是否处于 smartconfig 中 */ 
static bool IsSmartconfig = false;

/** 从 NVS 中读取 ssid
 * @param ssid 读到的 ssid
 * @param iMaxLength 外部存储 ssid 数组的最大值
 * @return 读取到的字节数
 */
static size_t prvReadNvsSsid(char *ssid, int iMaxLength)
{
    nvs_handle_t xNvsHandle;
    esp_err_t xRetVal = ESP_FAIL;
    size_t xRequiredSize = 0;
    ESP_ERROR_CHECK(nvs_open(NVS_WIFI_NAMESPACE_NAME, NVS_READWRITE, &xNvsHandle));
    xRetVal = nvs_get_str(xNvsHandle, NVS_SSID_KEY, NULL, &xRequiredSize);
    if (xRetVal == ESP_OK && xRequiredSize <= iMaxLength){
        nvs_get_str(xNvsHandle, NVS_SSID_KEY, ssid, &xRequiredSize);
    }else{
        xRequiredSize = 0;
    }
    nvs_close(xNvsHandle);
    return xRequiredSize;
}

/** 写入 ssid 到 NVS 中
 * @param ssid 需写入的 ssid
 * @return ESP_OK or ESP_FAIL
 */
static esp_err_t prvWriteNvsSsid(char *ssid)
{
    nvs_handle_t xNvsHandle;
    esp_err_t ret;
    ESP_ERROR_CHECK(nvs_open(NVS_WIFI_NAMESPACE_NAME, NVS_READWRITE, &xNvsHandle));
    ret = nvs_set_str(xNvsHandle, NVS_SSID_KEY, ssid);
    nvs_commit(xNvsHandle);
    nvs_close(xNvsHandle);
    return ret;
}

/** 从 NVS 中读取 password
 * @param ssid 读到的 password
 * @param iMaxLength 外部存储 password 数组的最大值
 * @return 读取到的字节数
 */
static size_t prvReadNvsPassword(char *pwd, int iMaxLength)
{
    nvs_handle_t xNvsHandle;
    esp_err_t xRetVal = ESP_FAIL;
    size_t xRequiredSize = 0;
    ESP_ERROR_CHECK(nvs_open(NVS_WIFI_NAMESPACE_NAME, NVS_READWRITE, &xNvsHandle));
    xRetVal = nvs_get_str(xNvsHandle, NVS_PASSWORD_KEY, NULL, &xRequiredSize);
    if (xRetVal == ESP_OK && xRequiredSize <= iMaxLength){
        nvs_get_str(xNvsHandle, NVS_SSID_KEY, pwd, &xRequiredSize);
    }else{
        xRequiredSize = 0;
    }
    nvs_close(xNvsHandle);
    return xRequiredSize;
}

/** 写入 password 到 NVS 中
 * @param pwd 需写入的 password
 * @return ESP_OK or ESP_FAIL
 */
static esp_err_t prvWriteNvsPassword(char *pwd)
{
    nvs_handle_t xNvsHandle;
    esp_err_t ret;
    ESP_ERROR_CHECK(nvs_open(NVS_WIFI_NAMESPACE_NAME, NVS_READWRITE, &xNvsHandle));
    ret = nvs_set_str(xNvsHandle, NVS_PASSWORD_KEY, pwd);
    nvs_commit(xNvsHandle);
    nvs_close(xNvsHandle);
    return ret;
}


/** 各种网络事件的回调函数
 * @param arg 自定义参数
 * @param xEventBase 事件类型
 * @param lEventID 事件标识 ID，不同的事件类型都有不同的实际标识 ID
 * @param pvEventData 事件携带的数据
 */
static void event_handler(void *arg, esp_event_base_t xEventBase, int32_t lEventID, void *pvEventData)
{
    if (xEventBase == WIFI_EVENT && lEventID == WIFI_EVENT_STA_START){
        if (cSsidValue[0] != 0){
            esp_wifi_connect();
        }
    }else if (xEventBase == WIFI_EVENT && lEventID == WIFI_EVENT_STA_DISCONNECTED){
        /* WiFi 断开连接后，再次发起连接 */
        if (!IsSmartconfig){
            esp_wifi_connect();
        }
        /* 清除连接标志位 */
        xEventGroupClearBits(xWifiEventGroup, CONNECTED_BIT);
    }else if (xEventBase == IP_EVENT && lEventID == IP_EVENT_STA_GOT_IP){
        /* 获取到 IP，置位连接事件标志位 */
        xEventGroupSetBits(xWifiEventGroup, CONNECTED_BIT);
    }else if (xEventBase == SC_EVENT && lEventID == SC_EVENT_SCAN_DONE){
        /* smartconfig 扫描完成 */
        ESP_LOGI(TAG, "Scan done");
    }else if (xEventBase == SC_EVENT && lEventID == SC_EVENT_FOUND_CHANNEL){
        /* smartconfig 找到对应的通道 */
        ESP_LOGI(TAG, "Found channel");
    }else if (xEventBase == SC_EVENT && lEventID == SC_EVENT_GOT_SSID_PSWD){
        /* smartconfig 获取到 ssid 和密码 */
        ESP_LOGI(TAG, "Got SSID and password");
        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)pvEventData;
        wifi_config_t xWifiConfig;
        /* 初始化 xWifiConfig */
        memset(&xWifiConfig, 0, sizeof(wifi_config_t));
        /* 安全复制 SSID 到 xWifiConfig - 确保不会溢出 */
        size_t ssid_len = strnlen((char *)evt->ssid, sizeof(evt->ssid));
        if (ssid_len >= sizeof(xWifiConfig.sta.ssid)) {
            ssid_len = sizeof(xWifiConfig.sta.ssid) - 1;
        }
        memcpy(xWifiConfig.sta.ssid, evt->ssid, ssid_len);
        xWifiConfig.sta.ssid[ssid_len] = '\0';
        /* 安全复制密码到 xWifiConfig - 确保不会溢出 */
        size_t password_len = strnlen((char *)evt->password, sizeof(evt->password));
        if (password_len >= sizeof(xWifiConfig.sta.password)) {
            password_len = sizeof(xWifiConfig.sta.password) - 1;
        }
        memcpy(xWifiConfig.sta.password, evt->password, password_len);
        xWifiConfig.sta.password[password_len] = '\0';
        /* 设置 BSSID */
        xWifiConfig.sta.bssid_set = evt->bssid_set;
        if (xWifiConfig.sta.bssid_set == true){
            memcpy(xWifiConfig.sta.bssid, evt->bssid, sizeof(xWifiConfig.sta.bssid));
        }
        /* 直接使用 evt->ssid和evt->password 打印日志 */
        ESP_LOGI(TAG, "SSID:%.*s", (int)ssid_len, evt->ssid);
        ESP_LOGI(TAG, "PASSWORD:%.*s", (int)password_len, evt->password);
        /* 直接赋值给全局变量，不再需要中间变量 */
        memset(cSsidValue, 0, sizeof(cSsidValue));
        memset(cPasswordValue, 0, sizeof(cPasswordValue));
        memcpy(cSsidValue, evt->ssid, ssid_len);
        memcpy(cPasswordValue, evt->password, password_len);
        cSsidValue[ssid_len] = '\0';
        cPasswordValue[password_len] = '\0';
        /* 重新连接 WiFi */
        esp_err_t err = esp_wifi_disconnect();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to disconnect: %s", esp_err_to_name(err));
        }
        err = esp_wifi_set_config(WIFI_IF_STA, &xWifiConfig);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set config: %s", esp_err_to_name(err));
        } else {
            esp_wifi_connect();
        }
    }
    else if (xEventBase == SC_EVENT && lEventID == SC_EVENT_SEND_ACK_DONE)
    {
        /* smartconfig 已发起回应 */
        xEventGroupSetBits(xWifiEventGroup, ESPTOUCH_DONE_BIT);
    }
}

void vWifiInit(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    xWifiEventGroup = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t xConfig = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&xConfig));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    /* 从 NVS 中读取 ssid */ 
    prvReadNvsSsid(cSsidValue, 32);
    /* 从 NVS 中读取 password */ 
    prvReadNvsPassword(cPasswordValue, 64);
    if (cSsidValue[0] != 0){ // 通过 ssid 第一个字节是否是0，判断是否读取成功，然后设置 wifi_config_t
        wifi_config_t xWifiConfig = {
            .sta ={
                .threshold.authmode = WIFI_AUTH_WPA2_PSK,
                .pmf_cfg ={
                    .capable = true,
                    .required = false
                },
            },
        };
        snprintf((char *)xWifiConfig.sta.ssid, 32, "%s", cSsidValue);
        snprintf((char *)xWifiConfig.sta.password, 64, "%s", cPasswordValue);
        /* 原代码少了这一行 */
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &xWifiConfig));
    }
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void prvSmartConfigExampleTask(void *pvParam);

/** 启动 smartconfig
 * @param 无
 * @return 无
 */
void vSmartConfigStart(void)
{
    if (!IsSmartconfig){
        IsSmartconfig = true;
        esp_wifi_disconnect();
        xTaskCreate(prvSmartConfigExampleTask, "prvSmartConfigExampleTask", 4096, NULL, 3, NULL);
    }
}

/** smartconfig 处理任务
 * @param 无
 * @return 无
 */
static void prvSmartConfigExampleTask(void *pvParam)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2)); // 设定 SmartConfig 版本
    smartconfig_start_config_t xConfig = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&xConfig)); // 启动 SmartConfig
    while (1){
        uxBits = xEventGroupWaitBits(xWifiEventGroup, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if (uxBits & CONNECTED_BIT){
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if (uxBits & ESPTOUCH_DONE_BIT){ // 收到 smartconfig 配网完成通知
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();               // 停止 smartconfig 配网
            prvWriteNvsSsid(cSsidValue);         // 将 ssid 写入 NVS
            prvWriteNvsPassword(cPasswordValue); // 将 password 写入 NVS
            IsSmartconfig = false;
            vTaskDelete(NULL); // 退出任务
        }
    }
}

// static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
// {
//     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
//     {
//         if (cSsidValue[0] != 0)
//             esp_wifi_connect();
//     }
//     else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
//     {
//         /* WIFI 断开连接后，再次发起连接 */ 
//         if (!IsSmartconfig)
//             esp_wifi_connect();
//         /* 清除连接标志位 */ 
//         xEventGroupClearBits(xWifiEventGroup, CONNECTED_BIT);
//     }
//     else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
//     {
//         /* 获取到 IP，置位连接事件标志位 */ 
//         xEventGroupSetBits(xWifiEventGroup, CONNECTED_BIT);
//     }
//     else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE)
//     {
//         /* smartconfig 扫描完成 */ 
//         ESP_LOGI(TAG, "Scan done");
//     }
//     else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL)
//     {
//         /* smartconfig 找到对应的通道 */ 
//         ESP_LOGI(TAG, "Found channel");
//     }
//     else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
//     {
//         /* smartconfig 获取到SSID和密码 */ 
//         ESP_LOGI(TAG, "Got SSID and password");

//         smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
//         wifi_config_t xWifiConfig;
//         uint8_t ssid[33] = {0};
//         uint8_t password[65] = {0};
//         /* 从 pvEventData 中提取 SSID 和密码 */ 
//         bzero(&xWifiConfig, sizeof(wifi_config_t));
//         memcpy(xWifiConfig.sta.ssid, evt->ssid, sizeof(xWifiConfig.sta.ssid));
//         memcpy(xWifiConfig.sta.password, evt->password, sizeof(xWifiConfig.sta.password));
//         xWifiConfig.sta.bssid_set = evt->bssid_set;
//         if (xWifiConfig.sta.bssid_set == true)
//         {
//             memcpy(xWifiConfig.sta.bssid, evt->bssid, sizeof(xWifiConfig.sta.bssid));
//         }
//         memcpy(ssid, evt->ssid, sizeof(evt->ssid));
//         memcpy(password, evt->password, sizeof(evt->password));
//         ESP_LOGI(TAG, "SSID:%s", ssid);
//         ESP_LOGI(TAG, "PASSWORD:%s", password);
//         snprintf(cSsidValue, 33, "%s", (char *)ssid);
//         snprintf(cPasswordValue, 65, "%s", (char *)password);
//         /* 重新连接 WIFI */ 
//         ESP_ERROR_CHECK(esp_wifi_disconnect());
//         ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &xWifiConfig));
//         esp_wifi_connect();
//     }
//     else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE)
//     {
//         /* smartconfig 已发起回应 */ 
//         xEventGroupSetBits(xWifiEventGroup, ESPTOUCH_DONE_BIT);
//     }
// }

/**
 * 格式化
 * 修改变量命名
 * 修改注释方式
 * 隔开中英文
 * 大花括号不换行
 */
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "simple_wifi_sta.h"

static const char *TAG = "main";

#define MQTT_ADDRESS "mqtt://broker-cn.emqx.io" // MQTT 连接地址
#define MQTT_PORT 1883                          // MQTT 连接端口号
#define MQTT_CLIENT "mqttx_luchenya"            // Client ID（设备唯一，大家最好自行改一下）
#define MQTT_USERNAME "luchenya"                // MQTT 用户名
#define MQTT_PASSWORD "Jxnu07297021@"           // MQTT 密码

#define MQTT_PUBLIC_TOPIC "/test/topic1"    // 测试用的,推送消息主题
#define MQTT_SUBSCRIBE_TOPIC "/test/topic2" // 测试用的,需要订阅的主题

/* 定义一个事件组，用于通知 main 函数 WIFI 连接成功 */ 
#define WIFI_CONNECT_BIT BIT0
EventGroupHandle_t xWifiEventGroup = NULL;

/* MQTT 客户端操作句柄 */ 
static esp_mqtt_client_handle_t xMQTTClientHandle = NULL;

/* MQTT 连接标志 */ 
static bool bIsMQTTConnected = false;

/**
 * mqtt 连接事件处理函数
 * @param event 事件参数
 * @return 无
 */
static void prvMQTTEventHandler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    /* your_context_t *context = event->context; */ 

    switch ((esp_mqtt_event_id_t)event_id){
    case MQTT_EVENT_CONNECTED: // 连接成功
        ESP_LOGI(TAG, "mqtt connected");
        bIsMQTTConnected = true;
        /* 连接成功后，订阅测试主题 */ 
        esp_mqtt_client_subscribe_single(xMQTTClientHandle, MQTT_SUBSCRIBE_TOPIC, 1); // qos:1
        break;
    case MQTT_EVENT_DISCONNECTED: // 连接断开
        ESP_LOGI(TAG, "mqtt disconnected");
        bIsMQTTConnected = false;
        break;
    case MQTT_EVENT_SUBSCRIBED: // 订阅主题后, 收到消息 ACK
        ESP_LOGI(TAG, " mqtt subscribed ack, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED: // 解订阅后, 收到消息 ACK
        break;
    case MQTT_EVENT_PUBLISHED: // 发布主题后, 收到消息 ACK
        ESP_LOGI(TAG, "mqtt publish ack, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic); // 收到 Pub 消息直接打印出来
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        break;
    }
}

/** 启动 mqtt 连接
 * @param 无
 * @return 无
 */
void vMQTTStart(void)
{
    esp_mqtt_client_config_t xMQTTConfig = {0};
    xMQTTConfig.broker.address.uri = MQTT_ADDRESS;
    xMQTTConfig.broker.address.port = MQTT_PORT;
    /* Client ID */ 
    xMQTTConfig.credentials.client_id = MQTT_CLIENT;
    /* 用户名 */ 
    xMQTTConfig.credentials.username = MQTT_USERNAME;
    /* 密码 */ 
    xMQTTConfig.credentials.authentication.password = MQTT_PASSWORD;
    ESP_LOGI(TAG, "mqtt connect->clientId:%s,username:%s,password:%s", xMQTTConfig.credentials.client_id,
             xMQTTConfig.credentials.username, xMQTTConfig.credentials.authentication.password);
    /* 设置 mqtt 配置，返回 mqtt 操作句柄 */ 
    xMQTTClientHandle = esp_mqtt_client_init(&xMQTTConfig);
    /* 注册 mqtt 事件回调函数 */ 
    esp_mqtt_client_register_event(xMQTTClientHandle, ESP_EVENT_ANY_ID, prvMQTTEventHandler, xMQTTClientHandle);
    /* 启动 mqtt 连接 */ 
    esp_mqtt_client_start(xMQTTClientHandle);
}


void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND){
        /* NVS出现错误，执行擦除 */ 
        ESP_ERROR_CHECK(nvs_flash_erase());
        /* 重新尝试初始化 */ 
        ESP_ERROR_CHECK(nvs_flash_init());
    }
    xWifiEventGroup = xEventGroupCreate();
    EventBits_t xEventBits = 0;

    /* 初始化 WIFI，传入回调函数，用于通知连接成功事件 */ 
    xWifiSTAInit();
    /* 一直监听 WIFI 连接事件，直到 WiFi 连接成功后，才启动 MQTT 连接 */
    xEventBits = xEventGroupWaitBits(xWifiEventGroup, WIFI_CONNECT_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
    if (xEventBits & WIFI_CONNECT_BIT){
        vMQTTStart();
    }
    static char cMQTTPubBuffer[64];
    while (1){
        static int iCount = 0;
        /* 延时2秒发布一条消息到 /test/topic1 主题 */ 
        if (bIsMQTTConnected){
            snprintf(cMQTTPubBuffer, 64, "{\"iCount\":\"%d\"}", iCount);
            esp_mqtt_client_publish(xMQTTClientHandle, MQTT_PUBLIC_TOPIC,
                                    cMQTTPubBuffer, strlen(cMQTTPubBuffer), 1, 0); // qos:1
            iCount++;
        }
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    return;
}

#include "uart.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/queue.h"

#define TAG "uart_module"

/* UART 内部实例结构体 */ 
typedef struct {
    uart_port_t xUARTNum;
    QueueHandle_t xEventQueue;
    TaskHandle_t xTaskHandle;
    uint8_t* pcBuffer;
    size_t xBufferSize;
} UARTInstance_Internal_t;

/* UART 默认配置参数 */ 
static const UARTConfigParams_t xDefaultUARTConfig = {
    .xUARTNum = UART_NUM_2,
    .ulBaudRate = 115200,
    .xDataBits = UART_DATA_8_BITS,
    .xStopBits = UART_STOP_BITS_1,
    .xParity = UART_PARITY_DISABLE,
    .xFlowCtrl = UART_HW_FLOWCTRL_DISABLE,
    .xTxPin = GPIO_NUM_32,
    .xRxPin = GPIO_NUM_33,
    .xCtsPin = UART_PIN_NO_CHANGE,
    .xRtsPin = UART_PIN_NO_CHANGE,
    .xRxBufferSize = 1024,
    .xTxBufferSize = 1024,
    .xQueueSize = 20,
    .ulTaskStackSize = 4096,
    .xTaskPriority = 3,
    .xCoreId = 1
};

/* UART 事件处理任务（ 接收数据 ） */ 
static void prvUARTEventTask(void *pvParameters)
{
    UARTInstance_Internal_t *pxInstance = (UARTInstance_Internal_t*)pvParameters;
    uart_event_t xEvent;
    
    while (1) {
        /* 接收串口事件信息 */ 
        if (xQueueReceive(pxInstance->xEventQueue, (void *)&xEvent, (TickType_t)portMAX_DELAY)) {
            ESP_LOGI(TAG, "uart[%d] event:", pxInstance->xUARTNum);
            
            switch (xEvent.type) {
                /* 接收到数据*/
                case UART_DATA:
                    ESP_LOGI(TAG, "[UART DATA]: %d", xEvent.size);
                    if (pxInstance->pcBuffer && xEvent.size <= pxInstance->xBufferSize) {
                        uart_read_bytes(pxInstance->xUARTNum, pxInstance->pcBuffer, xEvent.size, portMAX_DELAY);
                        ESP_LOGI(TAG, "[DATA EVT]:");
                        uart_write_bytes(pxInstance->xUARTNum, pxInstance->pcBuffer, xEvent.size);
                    }
                    break;
                /* UART硬件FIFO溢出 */    
                case UART_FIFO_OVF:
                    ESP_LOGW(TAG, "hw fifo overflow");
                    /* 清空UART输入缓冲区，丢弃所有待处理数据 */
                    uart_flush_input(pxInstance->xUARTNum);
                    /* 重置UART事件队列，清除队列中的所有事件 */
                    xQueueReset(pxInstance->xEventQueue);
                    break;
                /* UART 环形缓冲区满了 */     
                case UART_BUFFER_FULL:
                    ESP_LOGW(TAG, "ring buffer full");
                    /* 
                     * 该分支处理UART接收缓冲区满的情况，这是由于数据接收速度超过了数据处理速度导致的
                     * 当发生缓冲区满的情况时，建议考虑增加缓冲区大小以适应数据流量。
                     * 当前示例中采用的解决方案是直接清空接收缓冲区并重置队列，这样可以快速恢复数据接收功能，但会导致未处理的数据丢失。
                     */                    
                    uart_flush_input(pxInstance->xUARTNum);
                    xQueueReset(pxInstance->xEventQueue);
                    break;
                /* 接收发生中断*/ 
                case UART_BREAK:
                    ESP_LOGW(TAG, "uart rx break");
                    break;
                /* UART 奇偶校验错误 */    
                case UART_PARITY_ERR:
                    ESP_LOGW(TAG, "uart xParity error");
                    break;
                /* UART 帧错误 */    
                case UART_FRAME_ERR:
                    ESP_LOGW(TAG, "uart frame error");
                    break;
                /* 其他事件类型 */    
                default:
                    ESP_LOGI(TAG, "uart event type: %d", xEvent.type);
                    break;
            }
        }
    }
    vTaskDelete(NULL);
}

/* UART 模块初始化 */
UARTInstance_t* pxUARTModuleInit(const UARTConfigParams_t* pxConfig)
{
    /* 如果外部传入的配置参数为空，则使用默认配置参数*/
    if (!pxConfig) {
        pxConfig = &xDefaultUARTConfig;
    }
    
    /* 创建内部实例结构体（ 只在驱动文件中创建和使用 ） */
    UARTInstance_Internal_t* pxInstance = calloc(1, sizeof(UARTInstance_Internal_t));
    if (!pxInstance) {
        ESP_LOGE(TAG, "Failed to allocate memory for UART pxInstance");
        return NULL;
    }
    /* 初始化内部实例结构体 */
    pxInstance->xUARTNum = pxConfig->xUARTNum;
    pxInstance->xBufferSize = pxConfig->xRxBufferSize;
    pxInstance->pcBuffer = malloc(pxInstance->xBufferSize);
    if (!pxInstance->pcBuffer) {
        ESP_LOGE(TAG, "Failed to allocate buffer memory");
        free(pxInstance);
        return NULL;
    }
    
    /* 1、安装 UART 驱动 */ 
    esp_err_t err = uart_driver_install(
        pxInstance->xUARTNum, 
        pxConfig->xRxBufferSize, 
        pxConfig->xTxBufferSize, 
        pxConfig->xQueueSize, 
        &pxInstance->xEventQueue, // 传入空的队列句柄，函数内部会创建实例队列（ 接收串口事件信息 ）并绑定该句柄
        0
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(err));
        free(pxInstance->pcBuffer);
        free(pxInstance);
        return NULL;
    }
    
    /* 2、配置 UART 参数*/ 
    uart_config_t xUARTConfig = {
        .baud_rate = pxConfig->ulBaudRate,
        .data_bits = pxConfig->xDataBits,
        .parity = pxConfig->xParity,
        .stop_bits = pxConfig->xStopBits,
        .flow_ctrl = pxConfig->xFlowCtrl,
    };
    err = uart_param_config(pxInstance->xUARTNum, &xUARTConfig);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART parameters: %s", esp_err_to_name(err));
        uart_driver_delete(pxInstance->xUARTNum);
        free(pxInstance->pcBuffer);
        free(pxInstance);
        return NULL;
    }
    
    /* 3、设置引脚 */ 
    err = uart_set_pin(
        pxInstance->xUARTNum, 
        pxConfig->xTxPin, 
        pxConfig->xRxPin, 
        pxConfig->xCtsPin, 
        pxConfig->xRtsPin
    );
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(err));
        uart_driver_delete(pxInstance->xUARTNum);
        free(pxInstance->pcBuffer);
        free(pxInstance);
        return NULL;
    }
    
    /* 4、创建事件处理任务 */ 
    char task_name[16];
    snprintf(task_name, sizeof(task_name), "uart_%d", pxInstance->xUARTNum);
    
    BaseType_t result = xTaskCreatePinnedToCore(
        prvUARTEventTask,
        task_name,
        pxConfig->ulTaskStackSize,
        pxInstance,
        pxConfig->xTaskPriority,
        &pxInstance->xTaskHandle, // 创建任务时传入空的任务句柄，用于绑定该任务
        pxConfig->xCoreId
    );
    if (result != pdPASS) {
        ESP_LOGE(TAG, "Failed to create UART task");
        uart_driver_delete(pxInstance->xUARTNum);
        free(pxInstance->pcBuffer);
        free(pxInstance);
        return NULL;
    }
    
    /* 5、返回外部可见的实例结构体（ 相对于内部实例结构体少了缓冲区指针和缓冲区大小 ） */ 
    UARTInstance_t* pxExternalInstance = malloc(sizeof(UARTInstance_t));
    if (!pxExternalInstance) {
        ESP_LOGE(TAG, "Failed to allocate memory for external pxInstance");
        vTaskDelete(pxInstance->xTaskHandle);
        uart_driver_delete(pxInstance->xUARTNum);
        free(pxInstance->pcBuffer);
        free(pxInstance);
        return NULL;
    }
    /* 原本为空的队列句柄和任务句柄在执行上面代码后被绑定了实例队列和实例任务，可用于后续任务删除等，所以需要返回给外部 */
    pxExternalInstance->xUARTNum = pxInstance->xUARTNum;
    pxExternalInstance->xEventQueue = pxInstance->xEventQueue;
    pxExternalInstance->xTaskHandle = pxInstance->xTaskHandle;
    
    return pxExternalInstance;
}

/* 反初始化串口 */
void vUARTModuleDeinit(UARTInstance_t* pxInstance)
{
    if (!pxInstance) {
        return;
    }
    
    /* 删除任务 */ 
    if (pxInstance->xTaskHandle) {
        vTaskDelete(pxInstance->xTaskHandle);
    }
    
    /* 删除 UART 驱动 */  
    uart_driver_delete(pxInstance->xUARTNum);
    
    free(pxInstance);
}

/* 发送数据 */
int iUARTModuleSendData(UARTInstance_t* pxInstance, const void* pvData, size_t xLen)// timeout 没用到
{
    if (!pxInstance || !pvData || xLen == 0) {
        return -1;
    }
    
    int sent = uart_write_bytes(pxInstance->xUARTNum, pvData, xLen);
    return sent;
}

/* 接收数据 */
int iUARTModuleReceiveData(UARTInstance_t* pxInstance, void* pvBuffer, size_t xLen, TickType_t xTimeout)
{
    if (!pxInstance || !pvBuffer || xLen == 0) {
        return -1;
    }
    
    int received = uart_read_bytes(pxInstance->xUARTNum, pvBuffer, xLen, xTimeout);
    return received;
}

/* 获取实例的队列句柄 */
QueueHandle_t xUARTModuleGetQueue(UARTInstance_t* pxInstance)
{
    if (!pxInstance) {
        return NULL;
    }
    
    return pxInstance->xEventQueue;
}
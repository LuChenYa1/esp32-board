 #include "esp_log.h"
#include "uart.h"

void app_main(void)
{
    // 自定义配置（可选）
    UARTConfigParams_t my_uart_config = {
        .xUARTNum = UART_NUM_2,
        .ulBaudRate = 115200,
        .xDataBits = UART_DATA_8_BITS,
        .xStopBits = UART_STOP_BITS_1,
        .xParity = UART_PARITY_DISABLE,
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
    
    // 初始化 UART 模块
    UARTInstance_t* uart_instance = pxUARTModuleInit(&my_uart_config);
    if (!uart_instance) {
        ESP_LOGE("main", "Failed to initialize UART module");
        return;
    }
    
    // 现在可以在项目其他部分使用这个UART实例
    // ...
    
    // 使用完毕后释放资源
    //vUARTModuleDeinit(uart_instance);
}


/* 单文件例程 */
// #include <stdio.h>
// #include "esp_log.h"
// #include "driver/uart.h"
// #include "driver/gpio.h"
// #define TAG     "uart"

// #define    USER_UART_NUM     UART_NUM_2 // 串口2
// #define    UART_BUFFER_SIZE  1024       // 缓冲区大小

// static uint8_t ucUART_Buffer[1024];     // 缓冲区

// static QueueHandle_t xUART_Queue;       // 队列句柄

// /* 串口事件处理任务 */
// static void prvUARTEventTask(void *pvParameters)
// {
//     uart_event_t xEvent;
//     size_t xBufferSize;
//     while (1)
//     {
//         // 接收串口事件信息
//         if (xQueueReceive(xUART_Queue, (void *)&xEvent, (TickType_t)portMAX_DELAY)) 
//         {
//             ESP_LOGI(TAG, "uart[%d] event:", USER_UART_NUM);
//             switch (xEvent.type) {
//                 /* 接收到数据*/
//                 case UART_DATA:
//                     ESP_LOGI(TAG, "[UART DATA]: %d", xEvent.size);
//                     uart_read_bytes(USER_UART_NUM, ucUART_Buffer, xEvent.size, portMAX_DELAY);
//                     ESP_LOGI(TAG, "[DATA EVT]:");
//                     uart_write_bytes(USER_UART_NUM, ucUART_Buffer, xEvent.size);
//                     break;
                
//                 /* UART硬件FIFO溢出 */
//                 case UART_FIFO_OVF: 
//                     ESP_LOGI(TAG, "hw fifo overflow");
//                     /* 清空UART输入缓冲区，丢弃所有待处理数据 */
//                     uart_flush_input(USER_UART_NUM);
//                     /* 重置UART事件队列，清除队列中的所有事件 */
//                     xQueueReset(xUART_Queue);
//                     break;
//                 /* UART 环形缓冲区满了 */ 
//                 case UART_BUFFER_FULL:
//                     ESP_LOGI(TAG, "ring buffer full");
//                     /* 
//                      * 该分支处理UART接收缓冲区满的情况，这是由于数据接收速度超过了数据处理速度导致的
//                      * 当发生缓冲区满的情况时，建议考虑增加缓冲区大小以适应数据流量。
//                      * 当前示例中采用的解决方案是直接清空接收缓冲区并重置队列，这样可以快速恢复数据接收功能，但会导致未处理的数据丢失。
//                      */
//                     uart_flush_input(USER_UART_NUM);
//                     xQueueReset(xUART_Queue);
//                     break;
//                 // 接收发生中断
//                 case UART_BREAK:
//                     ESP_LOGI(TAG, "uart rx break");
//                     break;
//                 // uart奇偶校验错误
//                 case UART_PARITY_ERR:
//                     ESP_LOGI(TAG, "uart xParity error");
//                     break;
//                 // UART帧错误
//                 case UART_FRAME_ERR:
//                     ESP_LOGI(TAG, "uart frame error");
//                     break;
//                 default:
//                     ESP_LOGI(TAG, "uart event type: %d", xEvent.type);
//                     break;
//             }
//         }
//     }
//     vTaskDelete(NULL);
// }

// void app_main(void)
// {
//     /* 串口初始化 */
//     uart_config_t uart_cfg={
//         .ulBaudRate=115200,                                               //波特率115200
//         .xDataBits=UART_DATA_8_BITS,                                     //8位数据位
//         .flow_ctrl=UART_HW_FLOWCTRL_DISABLE,                             //无硬件流控制
//         .xParity=UART_PARITY_DISABLE,                                     //无校验位
//         .xStopBits=UART_STOP_BITS_1                                      //1位停止位
//     };
//     uart_driver_install(USER_UART_NUM, 1024, 1024, 20, &xUART_Queue, 0); //安装串口驱动，使用队列储存串口事件
//     uart_param_config(USER_UART_NUM, &uart_cfg);
//     /* 指定串口tx、rx引脚, 后两个UART_PIN_NO_CHANGE参数表示不改变CTS和RTS引脚的默认配置 */
//     uart_set_pin(USER_UART_NUM, GPIO_NUM_32, GPIO_NUM_33, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE); //设置引脚,tx为32,rx为33
    
//     /* 创建串口事件处理任务 */
//     xTaskCreatePinnedToCore(prvUARTEventTask, "uart", 4096, NULL, 3, NULL, 1);
//     #if 0
//     while(1)
//     {
//         int rec = uart_read_bytes(USER_UART_NUM,uart_buffer,sizeof(uart_buffer),pdMS_TO_TICKS(100));
//         if(rec)
//         {
//             uart_write_bytes(USER_UART_NUM,uart_buffer,rec);
//         }
//     }
//     #endif
// }  

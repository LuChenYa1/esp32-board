#ifndef UART_MODULE_H
#define UART_MODULE_H

#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"

/* UART 配置结构体 */ 
typedef struct {
    uart_port_t xUARTNum;            // UART端口号
    uint32_t ulBaudRate;             // 波特率
    uart_word_length_t xDataBits;    // 数据位
    uart_stop_bits_t xStopBits;      // 停止位
    uart_parity_t xParity;           // 校验位
    uart_hw_flowcontrol_t xFlowCtrl; // 硬件流控制
    gpio_num_t xTxPin;               // TX引脚
    gpio_num_t xRxPin;               // RX引脚
    gpio_num_t xCtsPin;              // CTS引脚 (可选)
    gpio_num_t xRtsPin;              // RTS引脚 (可选)
    size_t xRxBufferSize;            // 接收缓冲区大小
    size_t xTxBufferSize;            // 发送缓冲区大小
    size_t xQueueSize;               // 队列大小
    uint32_t ulTaskStackSize;        // 任务栈大小
    UBaseType_t xTaskPriority;       // 任务优先级
    BaseType_t xCoreId;              // 运行核心ID
} UARTConfigParams_t;

/* UART 实例句柄（外部可见） */ 
typedef struct {
    uart_port_t xUARTNum;
    QueueHandle_t xEventQueue;
    TaskHandle_t xTaskHandle;
} UARTInstance_t;

/**
 * @brief 初始化 UART 模块（创建了任务）
 * @param config 配置参数
 * @return uart_instance_t* UART实例指针，失败返回NULL
 */
UARTInstance_t* pxUARTModuleInit(const UARTConfigParams_t* pxConfig);

/**
 * @brief 反初始化 UART 模块
 * @param instance UART 实例指针
 */
void vUARTModuleDeinit(UARTInstance_t* pxInstance);

/**
 * @brief 发送数据
 * @param instance UART实例
 * @param data 要发送的数据
 * @param len 数据长度
 * @param timeout 超时时间
 * @return 实际发送的字节数
 */
int iUARTModuleSendData(UARTInstance_t* pxInstance, const void* pvData, size_t xLen);

/**
 * @brief 接收数据
 * @param instance UART实例
 * @param buffer 接收缓冲区
 * @param len 要接收的最大长度
 * @param timeout 超时时间
 * @return 实际接收的字节数
 */
int iUARTModuleReceiveData(UARTInstance_t* pxInstance, void* pvBuffer, size_t xLen, TickType_t xTimeout);

/**
 * @brief 获取UART事件队列
 * @param instance UART实例
 * @return 队列句柄
 */
QueueHandle_t xUARTModuleGetQueue(UARTInstance_t* pxInstance);

#endif // UART_MODULE_H
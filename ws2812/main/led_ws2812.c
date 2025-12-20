/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * 已完成变量命名修改
 * 已完成注释修改
 * 已完成格式化
 * 已完成中英文间距修改
 */

#include "esp_check.h"
#include "led_ws2812.h"
#include "driver/rmt_tx.h"

static const char *TAG = "pxLedEncoder";

#define LED_STRIP_RESOLUTION_HZ 10000000 // 10MHz 分辨率, 也就是1tick = 0.1us，也就是可以控制的最小时间单元，低于0.1us的脉冲无法产生

/*
 * RMT 是 ESP32 的一个专用外设，本质上是一个可编程的脉冲序列发生器/分析器。
 * 全称是 Remote Control Transceiver（远程控制收发器）。
 * 它最初设计用于处理红外遥控信号，但因其灵活性，现在被广泛用于各种精确时序的信号处理。
    1、内存块：包含要发送或接收的脉冲序列描述
    2、时钟分频：可以配置精确的时钟源
    3、有限状态机：独立于 CPU 运行，不受任务调度影响
    4、中断/DMA：数据传输完成后通过中断通知 CPU
 * 为什么不用纯软件延时来驱动 WS2812？
    与 STM32 不同，ESP32 是双核处理器，运行FreeRTOS实时操作系统：
    在 RTOS 环境中，纯软件延时容易被其他任务打断
    指令和数据缓存会导致时序不一致
    ESP32 主频较高（240MHz），简单的 nop 循环很难精确控制微秒/纳秒级时序
 */

/* WS2812驱动的描述符 */
struct Ws2812Strip_t
{
    rmt_channel_handle_t xLedChan;    // rmt 通道
    rmt_encoder_handle_t xLedEncoder; // rmt 编码器
    uint8_t *pcLedBuffer;             // rgb 数据
    int iLedNum;                      // led 个数
};

/* 自定义编码器 */
typedef struct
{
    rmt_encoder_t xBase;           // 编码器，里面包含三个需要用户实现的回调函数，encode, del, ret
    rmt_encoder_t *pxBytesEncoder; // 字节编码器，调用 rmt_new_bytes_encoder 函数后创建
    rmt_encoder_t *pxCopyEncoder;  // 拷贝编码器，调用 rmt_new_copy_encoder 函数后创建
    int iState;                    // 状态控制
    rmt_symbol_word_t xResetCode;  // 结束位的时序
} RmtLedStripEncoder_t;

/* 发送 WS2812 数据的函数调用顺序如下
 * 1、调用 rmt_transmit，需传入 RMT 通道、发送的数据、编码器参数
 * 2、调用编码器的 encode 函数，在本例程中就是调用 rmt_encode_led_strip 函数
 * 3、调用由 rmt_new_bytes_encoder 创建的字节编码器编码函数 pxBytesEncoder->encode，将用户数据编码成 rmt_symbol_word_t RMT符号
 * 4、调用由 rmt_new_copy_encoder 创建的拷贝编码器编码函数 pxCopyEncoder->encode，将复位信号安装既定的电平时间进行编码
 * 5、rmt_encode_led_strip 函数返回，在底层将信号发送出去（本质上是操作 IO 管脚高低电平）
 */

/** 编码回调函数
 * @param pxEncoder 编码器
 * @param xChannel RMT通道
 * @param pvPrimaryData 待编码用户数据
 * @param xDataSize 待编码用户数据长度
 * @param pxRetState 编码状态
 * @return RMT符号个数
 */
static size_t prvRmtEncodeLedStrip(rmt_encoder_t *pxEncoder, rmt_channel_handle_t xChannel, const void *pvPrimaryData, size_t xDataSize, rmt_encode_state_t *pxRetState)
{
    /*
    __containerof 宏的作用:
    通过结构的成员来访问这个结构的地址
    在这个函数中，传入参数 pxEncoder 是 RmtLedStripEncoder_t 结构体中的base成员
    __containerof 宏通过 pxEncoder 的地址，根据 RmtLedStripEncoder_t 的内存排布找到 RmtLedStripEncoder_t* 的首地址
    */
    RmtLedStripEncoder_t *pxLedEncoder = __containerof(pxEncoder, RmtLedStripEncoder_t, xBase);
    rmt_encoder_handle_t xBytesEncoder = pxLedEncoder->pxBytesEncoder; // 取出字节编码器
    rmt_encoder_handle_t xCopyEncoder = pxLedEncoder->pxCopyEncoder;   // 取出拷贝编码器
    rmt_encode_state_t xSessionState = RMT_ENCODING_RESET;
    rmt_encode_state_t xState = RMT_ENCODING_RESET;
    size_t xEncodedSymbols = 0;
    switch (pxLedEncoder->iState){
    /* pxLedEncoder->iState 是自定义的状态，这里只有两种值，0是发送RGB数据，1是发送复位码 */
    case 0: // send RGB data
        xEncodedSymbols += xBytesEncoder->encode(xBytesEncoder, xChannel, pvPrimaryData, xDataSize, &xSessionState);
        if (xSessionState & RMT_ENCODING_COMPLETE){ // 字节编码完成
            pxLedEncoder->iState = 1; // switch to next state when current encoding session finished
        }
        if (xSessionState & RMT_ENCODING_MEM_FULL){ // 缓存不足，本次退出
            xState |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space for encoding artifacts
        }
    /* fall-through */
    case 1: // send reset code
        xEncodedSymbols += xCopyEncoder->encode(xCopyEncoder, xChannel, &pxLedEncoder->xResetCode, sizeof(pxLedEncoder->xResetCode), &xSessionState);
        if (xSessionState & RMT_ENCODING_COMPLETE){
            pxLedEncoder->iState = RMT_ENCODING_RESET; // back to the initial encoding session
            xState |= RMT_ENCODING_COMPLETE;
        }
        if (xSessionState & RMT_ENCODING_MEM_FULL){
            xState |= RMT_ENCODING_MEM_FULL;
            goto out; // yield if there's no free space for encoding artifacts
        }
    }
out:
    *pxRetState = xState;
    return xEncodedSymbols;
}

static esp_err_t prvRmtDelLedStripEncoder(rmt_encoder_t *encoder)
{
    RmtLedStripEncoder_t *pxLedEncoder = __containerof(encoder, RmtLedStripEncoder_t, xBase);
    rmt_del_encoder(pxLedEncoder->pxBytesEncoder);
    rmt_del_encoder(pxLedEncoder->pxCopyEncoder);
    free(pxLedEncoder);
    return ESP_OK;
}

static esp_err_t prvRmtLedStripEncoderReset(rmt_encoder_t *pxEncoder)
{
    RmtLedStripEncoder_t *pxLedEncoder = __containerof(pxEncoder, RmtLedStripEncoder_t, xBase);
    rmt_encoder_reset(pxLedEncoder->pxBytesEncoder);
    rmt_encoder_reset(pxLedEncoder->pxCopyEncoder);
    pxLedEncoder->iState = RMT_ENCODING_RESET;
    return ESP_OK;
}

/** 创建一个基于 WS2812 时序的编码器
 * @param pxRetEncoder 返回的编码器，这个编码器在使用 rmt_transmit 函数传输时会用到
 * @return ESP_OK or ESP_FAIL
 */
esp_err_t xRmtNewLedStripEncoder(rmt_encoder_handle_t *pxRetEncoder)
{
    esp_err_t ret = ESP_OK;

    /* 创建一个自定义的编码器结构体，用于控制发送编码的流程 */
    RmtLedStripEncoder_t *pxLedEncoder = NULL;
    pxLedEncoder = calloc(1, sizeof(RmtLedStripEncoder_t));
    ESP_GOTO_ON_FALSE(pxLedEncoder, ESP_ERR_NO_MEM, err, TAG, "no mem for led strip encoder");
    pxLedEncoder->xBase.encode = prvRmtEncodeLedStrip;      // 这个函数会在 rmt 发送数据的时候被调用，我们可以在这个函数增加额外代码进行控制
    pxLedEncoder->xBase.del = prvRmtDelLedStripEncoder;     // 这个函数在卸载 rmt 时被调用
    pxLedEncoder->xBase.reset = prvRmtLedStripEncoderReset; // 这个函数在复位 rmt 时被调用

    /* 新建一个编码器配置(0,1位持续时间，参考芯片手册) */
    rmt_bytes_encoder_config_t xBytesEncoderConfig = {
        .bit0 = {
            .level0 = 1,
            .duration0 = 0.3 * LED_STRIP_RESOLUTION_HZ / 1000000, // T0H = 0.3us
            .level1 = 0,
            .duration1 = 0.9 * LED_STRIP_RESOLUTION_HZ / 1000000, // T0L = 0.9us
        },
        .bit1 = {
            .level0 = 1,
            .duration0 = 0.9 * LED_STRIP_RESOLUTION_HZ / 1000000, // T1H = 0.9us
            .level1 = 0,
            .duration1 = 0.3 * LED_STRIP_RESOLUTION_HZ / 1000000, // T1L = 0.3us
        },
        .flags.msb_first = 1 // 高位先传输
    };
    /* 传入编码器配置，获得数据编码器操作句柄 */
    rmt_new_bytes_encoder(&xBytesEncoderConfig, &pxLedEncoder->pxBytesEncoder);
    /* 新建一个拷贝编码器配置，拷贝编码器一般用于传输恒定的字符数据，比如说结束位 */
    rmt_copy_encoder_config_t copy_encoder_config = {};
    rmt_new_copy_encoder(&copy_encoder_config, &pxLedEncoder->pxCopyEncoder);
    /* 设定结束位时序 */
    uint32_t ulResetTicks = LED_STRIP_RESOLUTION_HZ / 1000000 * 50 / 2; // 分辨率/1M=每个 ticks 所需的 us ，然后乘以50就得出50 us 所需的 ticks
    pxLedEncoder->xResetCode = (rmt_symbol_word_t){
        .level0 = 0,
        .duration0 = ulResetTicks,
        .level1 = 0,
        .duration1 = ulResetTicks,
    };
    /* 返回编码器 */
    *pxRetEncoder = &pxLedEncoder->xBase;
    return ESP_OK;
err:
    if (pxLedEncoder){
        if (pxLedEncoder->pxBytesEncoder){
            rmt_del_encoder(pxLedEncoder->pxBytesEncoder);
        }
        if (pxLedEncoder->pxCopyEncoder){
            rmt_del_encoder(pxLedEncoder->pxCopyEncoder);
        }
        free(pxLedEncoder);
    }
    return ret;
}

/** 初始化 WS2812 外设
 * @param xGpio 控制 WS2812 的管脚
 * @param iMaxLed 控制 WS2812 的个数
 * @param pxLedHandle 返回的控制句柄
 * @return ESP_OK or ESP_FAIL
 */
esp_err_t xWs2812Init(gpio_num_t xGpio, int iMaxLed, Ws2812StripHandle_t *pxHandle)
{
    struct Ws2812Strip_t *pxLedHandle = NULL;
    /* 新增一个 WS2812 驱动描述 */
    pxLedHandle = calloc(1, sizeof(struct Ws2812Strip_t));
    assert(pxLedHandle);
    /* 按照 led 个数来分配 RGB 缓存数据 */
    pxLedHandle->pcLedBuffer = calloc(1, iMaxLed * 3);
    assert(pxLedHandle->pcLedBuffer);
    /* 设置 LED 个数 */
    pxLedHandle->iLedNum = iMaxLed;
    /* 定义一个 RMT 发送通道配置 */
    rmt_tx_channel_config_t xTxChannelConfig = {
        .clk_src = RMT_CLK_SRC_DEFAULT,           // 默认时钟源
        .gpio_num = xGpio,                        // GPIO 管脚
        .mem_block_symbols = 64,                  // 内存块大小，即 64 * 4 = 256 字节
        .resolution_hz = LED_STRIP_RESOLUTION_HZ, // RMT通道的分辨率 10000000 hz=0.1 us，也就是可以控制的最小时间单元
        .trans_queue_depth = 4,                   // 底层后台发送的队列深度
    };
    /* 创建一个 RMT 发送通道 */
    ESP_ERROR_CHECK(rmt_new_tx_channel(&xTxChannelConfig, &pxLedHandle->xLedChan));
    /* 创建自定义编码器（重点函数），所谓编码，就是发射红外时加入我们的时序控制 */
    ESP_ERROR_CHECK(xRmtNewLedStripEncoder(&pxLedHandle->xLedEncoder));
    /* 使能RMT通道 */
    ESP_ERROR_CHECK(rmt_enable(pxLedHandle->xLedChan));
    /* 返回 WS2812 操作句柄 */
    *pxHandle = pxLedHandle;

    return ESP_OK;
}

/** 反初始化 WS2812 外设
 * @param pxHandle 初始化的句柄
 * @return ESP_OK or ESP_FAIL
 */
esp_err_t xWs2812Deinit(Ws2812StripHandle_t xHandle)
{
    if (!xHandle)
        return ESP_OK;
    rmt_del_encoder(xHandle->xLedEncoder);
    if (xHandle->pcLedBuffer)
        free(xHandle->pcLedBuffer);
    free(xHandle);
    return ESP_OK;
}

/** 向某个 WS2812 写入 RGB 数据
 * @param xHandle 句柄
 * @param ulIndex 第几个 WS2812（0开始）
 * @param r,g,b RGB 数据
 * @return ESP_OK or ESP_FAIL
 */
esp_err_t xWs2812Write(Ws2812StripHandle_t xHandle, uint32_t ulIndex, uint32_t r, uint32_t g, uint32_t b)
{
    if (!xHandle || ulIndex >= xHandle->iLedNum)
        return ESP_FAIL;
    // 关键：等待上一次传输完成
    rmt_tx_wait_all_done(xHandle->xLedChan, 50); // 等待最多50ms

    rmt_transmit_config_t xTxConfig = {
        .loop_count = 0, // 不循环发送
    };
    
    uint32_t start = ulIndex * 3;
    xHandle->pcLedBuffer[start + 0] = g & 0xff; // 注意，WS2812 的数据顺序时 GRB
    xHandle->pcLedBuffer[start + 1] = r & 0xff;
    xHandle->pcLedBuffer[start + 2] = b & 0xff;

    return rmt_transmit(xHandle->xLedChan, xHandle->xLedEncoder, xHandle->pcLedBuffer, xHandle->iLedNum * 3, &xTxConfig);
}

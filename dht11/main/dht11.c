#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <soc/rmt_reg.h>
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp32/rom/ets_sys.h"

#define TAG "DHT11"

uint8_t ucDht11PIN = -1;

/* RMT 接收通道句柄 */ 
static rmt_channel_handle_t xRxChannelHandle = NULL;

/* 数据接收队列 */ 
static QueueHandle_t xRxReceiveQueue = NULL;

/* 将 RMT 读取到的脉冲数据处理为温度和湿度 */ 
static int prvParseItems(rmt_symbol_word_t *pxItem, int iItemNum, int *piHumidity, int *piTempX10);

/* 接收完成回调函数 */ 
static bool IRAM_ATTR prvExampleRmtRxDoneCallback(rmt_channel_handle_t channel, const rmt_rx_done_event_data_t *edata, void *user_data)
{
	BaseType_t high_task_wakeup = pdFALSE;
	QueueHandle_t rx_receive_queue = (QueueHandle_t)user_data;
	/* send the received RMT symbols to the parser task */ 
	xQueueSendFromISR(rx_receive_queue, edata, &high_task_wakeup);
	return high_task_wakeup == pdTRUE;
}

/** DHT11初始化
 * @param xDht11Pin GPIO引脚
 * @return 无
 */
void vDht11Init(uint8_t xDht11Pin)
{
	ucDht11PIN = xDht11Pin;

	rmt_rx_channel_config_t rx_chan_config = {
		.clk_src = RMT_CLK_SRC_APB,	  // 选择时钟源
		.resolution_hz = 1000 * 1000, // 1 MHz 滴答分辨率，即 1 滴答 = 1 µs
		.mem_block_symbols = 64,	  // 内存块大小，即 64 * 4 = 256 字节
		.gpio_num = xDht11Pin,		  // GPIO 编号
		.flags.invert_in = false,	  // 不反转输入信号
		.flags.with_dma = false,	  // 不需要 DMA 后端(ESP32S3才有)
	};
	/* 创建rmt接收通道 */ 
	ESP_ERROR_CHECK(rmt_new_rx_channel(&rx_chan_config, &xRxChannelHandle));

	/* 新建接收数据队列 */ 
	xRxReceiveQueue = xQueueCreate(20, sizeof(rmt_rx_done_event_data_t));
	assert(xRxReceiveQueue);

	/* 注册接收完成回调函数 */ 
	ESP_LOGI(TAG, "register RX done callback");
	rmt_rx_event_callbacks_t xCallbackStruct = {
		.on_recv_done = prvExampleRmtRxDoneCallback,
	};
	ESP_ERROR_CHECK(rmt_rx_register_event_callbacks(xRxChannelHandle, &xCallbackStruct, xRxReceiveQueue));

	/* 使能RMT接收通道 */ 
	ESP_ERROR_CHECK(rmt_enable(xRxChannelHandle));
}

/* 将RMT读取到的脉冲数据处理为温度和湿度(rmt_symbol_word_t称为RMT符号) */ 
static int prvParseItems(rmt_symbol_word_t *pxItem, int iItemNum, int *piHumidity, int *piTempX10)
{
	int i = 0;
	unsigned int rh = 0, uiTemp = 0, uiCheckSum = 0;
	/* 检查是否有足够的脉冲数 */ 
	if (iItemNum < 41)
	{ 
		ESP_LOGI(TAG, "iItemNum < 41  %d", iItemNum);
		return 0;
	}
	else if (iItemNum > 41)
		pxItem++; // 跳过开始信号脉冲
	/* 提取湿度数据 */ 
	for (i = 0; i < 16; i++, pxItem++) {
		uint16_t uiDuration = 0;
		if (pxItem->level0){
			uiDuration = pxItem->duration0;
		}else{
			uiDuration = pxItem->duration1;
		}
		rh = (rh << 1) + (uiDuration < 35 ? 0 : 1);
	}
	/* 提取温度数据 */ 
	for (i = 0; i < 16; i++, pxItem++) 
	{
		uint16_t uiDuration = 0;
		if (pxItem->level0)
			uiDuration = pxItem->duration0;
		else
			uiDuration = pxItem->duration1;
		uiTemp = (uiTemp << 1) + (uiDuration < 35 ? 0 : 1);
	}

	for (i = 0; i < 8; i++, pxItem++)
	{ 
		/* 提取校验数据 */ 
		uint16_t uiDuration = 0;
		if (pxItem->level0)
			uiDuration = pxItem->duration0;
		else
			uiDuration = pxItem->duration1;
		uiCheckSum = (uiCheckSum << 1) + (uiDuration < 35 ? 0 : 1);
	}
	/* 检查校验 */ 
	if ((((uiTemp >> 8) + uiTemp + (rh >> 8) + rh) & 0xFF) != uiCheckSum)
	{
		ESP_LOGI(TAG, "Checksum failure %4X %4X %2X\n", uiTemp, rh, uiCheckSum);
		return 0;
	}
	/* 返回数据 */ 
	rh = rh >> 8;
	uiTemp = (uiTemp >> 8) * 10 + (uiTemp & 0xFF);
	/* 判断数据合法性 */ 
	if (rh <= 100)
		*piHumidity = rh;
	if (uiTemp <= 600)
		*piTempX10 = uiTemp;
	return 1;
}

/** 获取DHT11数据
 * @param piTempX10 温度值
 * @return 无
 */
int iDht11StartGet(int *piTempX10, int *piHumidity)
{
	/* 发送20ms开始信号脉冲启动DHT11单总线 */ 
	gpio_set_direction(ucDht11PIN, GPIO_MODE_OUTPUT);
	gpio_set_level(ucDht11PIN, 1);
	ets_delay_us(1000);
	gpio_set_level(ucDht11PIN, 0);
	ets_delay_us(20000);
	/* 拉高20us */ 
	gpio_set_level(ucDht11PIN, 1);
	ets_delay_us(20);
	/* 信号线设置为输入准备接收数据 */ 
	gpio_set_direction(ucDht11PIN, GPIO_MODE_INPUT);
	gpio_set_pull_mode(ucDht11PIN, GPIO_PULLUP_ONLY);

	/* 启动RMT接收器以获取数据 */ 
	rmt_receive_config_t xReceiveConfig = {
		.signal_range_min_ns = 100,			// 最小脉冲宽度(0.1us),信号长度小于这个值，视为干扰
		.signal_range_max_ns = 1000 * 1000, // 最大脉冲宽度(1000us)，信号长度大于这个值，视为结束信号
	};

	static rmt_symbol_word_t xRawSymbols[128]; // 接收缓存
	static rmt_rx_done_event_data_t xRxData;   // 实际接收到的数据
	ESP_ERROR_CHECK(rmt_receive(xRxChannelHandle, xRawSymbols, sizeof(xRawSymbols), &xReceiveConfig));

	/* wait for RX done signal */ 
	if (xQueueReceive(xRxReceiveQueue, &xRxData, pdMS_TO_TICKS(1000)) == pdTRUE)
	{
		/* parse the receive symbols and print the result */ 
		return prvParseItems(xRxData.received_symbols, xRxData.num_symbols, piHumidity, piTempX10);
	}
	return 0;
}

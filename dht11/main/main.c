#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <nvs_flash.h>
#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <soc/rmt_reg.h>
#include "driver/gpio.h"
#include <esp_log.h>
#include <freertos/queue.h>
#include "esp32/rom/ets_sys.h"
#include "dht11.h"

#define DHT11_GPIO 25 // DHT11引脚定义
const static char *TAG = "DHT11_Demo";

// 温度 湿度变量
int iTemp = 0, iHum = 0;

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

// 主函数
void app_main(void)
{
	/*
	 * 非易失性存储（NVS）系统通常用于存储：
		WiFi SSID 和密码
		设备配置参数
		校准数据
		运行计数器
		OTA 固件信息
	 * 这些数据在设备断电或重启后也不会丢失
	 */
	ESP_ERROR_CHECK(nvs_flash_init());
	vTaskDelay(100 / portTICK_PERIOD_MS);

	/* 输出系统版本信息*/
	ESP_LOGI(TAG, "[APP] APP Is Start!~\r\n");
	ESP_LOGI(TAG, "[APP] IDF Version is %d.%d.%d", ESP_IDF_VERSION_MAJOR, ESP_IDF_VERSION_MINOR, ESP_IDF_VERSION_PATCH);
	ESP_LOGI(TAG, "[APP] Free memory: %lu bytes", esp_get_free_heap_size());
	ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

	vDht11Init(DHT11_GPIO);
	while (1)
	{
		if (iDht11StartGet(&iTemp, &iHum))
		{
			ESP_LOGI(TAG, "temp->%i.%i C     hum->%i%%", iTemp / 10, iTemp % 10, iHum);
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

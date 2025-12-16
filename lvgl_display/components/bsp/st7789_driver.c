#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "st7789_driver.h"
#include "lvgl/lvgl.h"

#define LCD_SPI_HOST    SPI2_HOST

static const char* TAG = "st7789";

/* lcd 操作句柄 */ 
static esp_lcd_panel_io_handle_t xLcdIOHandle = NULL;

/* 刷新完成回调函数 */ 
static pvLcdFlushDoneCallback xFlushDoneCallback = NULL;

/* 背光 GPIO */ 
static gpio_num_t   xBLGPIO = -1;

/**
 * @brief LCD面板IO刷新完成通知回调函数
 * 
 * 当LCD面板IO操作完成时，此函数会被调用以通知上层应用。
 * 如果设置了刷新完成回调函数指针xFlushDoneCallback，则会调用该回调函数。
 * 
 * @param panel_io LCD面板IO句柄
 * @param edata LCD面板IO事件数据指针
 * @param user_ctx 用户上下文数据指针
 * @return bool 返回false表示不阻止事件传播
 */
static bool bNotifyFlushReady(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    /* 如果刷新完成回调函数已设置，则调用该回调函数 */
    if(xFlushDoneCallback)
        xFlushDoneCallback(user_ctx);
    return false;
}


/** st7789初始化
 * @param St7789Config_t  接口参数
 * @return 成功或失败
*/
esp_err_t xSt7789DriverHwInit(St7789Config_t* pxConfig)
{
    /* 1、初始化 SPI */
    spi_bus_config_t xBusConfig = {
        .sclk_io_num = pxConfig->xClk,        // SCLK引脚
        .mosi_io_num = pxConfig->xMOSI,       // MOSI引脚
        .miso_io_num = -1,                    // -1 表示不使用MISO引脚
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .flags = SPICOMMON_BUSFLAG_MASTER ,   // SPI主模式
        .max_transfer_sz = pxConfig->uiWidth * 40 * sizeof(uint16_t),  // DMA单次传输最大字节，最大32768
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &xBusConfig, SPI_DMA_CH_AUTO));

    xFlushDoneCallback = pxConfig->pvDoneCallback;  // 设置刷新完成回调函数

    /* 2、初始化背光 GPIO（ 输出 ） */ 
    xBLGPIO = pxConfig->xBL;                        // 设置背光GPIO
    gpio_config_t xBLGPIOConfig = 
    {
        .pull_up_en = GPIO_PULLUP_DISABLE,          // 禁止上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,      // 禁止下拉
        .mode = GPIO_MODE_OUTPUT,                   // 输出模式
        .intr_type = GPIO_INTR_DISABLE,             // 禁止中断
        .pin_bit_mask = (1<<pxConfig->xBL)          // GPIO脚
    };
    gpio_config(&xBLGPIOConfig);

    /* 3、初始化复位脚（ 输出 ） */
    if(pxConfig->xRst > 0)
    {
        gpio_config_t xResetGPIOConfig = 
        {
            .pull_up_en = GPIO_PULLUP_DISABLE,      // 禁止上拉
            .pull_down_en = GPIO_PULLDOWN_DISABLE,  // 禁止下拉
            .mode = GPIO_MODE_OUTPUT,               // 输出模式
            .intr_type = GPIO_INTR_DISABLE,         // 禁止中断
            .pin_bit_mask = (1<<pxConfig->xRst)     // GPIO 脚
        };
        gpio_config(&xResetGPIOConfig);
    }

    /* 4、创建基于 spi 的 lcd 操作句柄 */
    esp_lcd_panel_io_spi_config_t xIOConfig = {
        .dc_gpio_num = pxConfig->xDC,             // DC 引脚
        .cs_gpio_num = pxConfig->xCS,             // CS 引脚
        .pclk_hz = pxConfig->ulSPIFreq,           // SPI 时钟频率
        .lcd_cmd_bits = 8,                        // 命令长度
        .lcd_param_bits = 8,                      // 参数长度
        .spi_mode = 0,                            // 使用SPI0模式
        .trans_queue_depth = 10,                  // 表示可以缓存的spi传输事务队列深度
        .on_color_trans_done = bNotifyFlushReady, // 刷新完成回调函数
        .user_ctx = pxConfig->pvCallbackParam,    // 回调函数参数
        .flags = {                                // 以下为 SPI 时序的相关参数，需根据 LCD 驱动 IC 的数据手册以及硬件的配置确定
            .sio_mode = 0,                        // 通过一根数据线（MOSI）读写数据，0: Interface I 型，1: Interface II 型
        },
    };
    // Attach the LCD to the SPI bus
    ESP_LOGI(TAG,"create esp_lcd_new_panel");
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &xIOConfig, &xLcdIOHandle));
    
    /* 5、硬件复位 */
    if(pxConfig->xRst > 0)
    {
        gpio_set_level(pxConfig->xRst, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level(pxConfig->xRst, 1);
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    /* 6、向LCD写入初始化命令*/
    esp_lcd_panel_io_tx_param(xLcdIOHandle, LCD_CMD_SWRESET,NULL, 0); // 软件复位
    vTaskDelay(pdMS_TO_TICKS(150));
    esp_lcd_panel_io_tx_param(xLcdIOHandle, LCD_CMD_SLPOUT,NULL, 0);  // 退出休眠模式
    vTaskDelay(pdMS_TO_TICKS(200));
    esp_lcd_panel_io_tx_param(xLcdIOHandle, LCD_CMD_COLMOD,(uint8_t[]) {0x55,}, 1); //选择RGB数据格式，0x55:RGB565,0x66:RGB666
    esp_lcd_panel_io_tx_param(xLcdIOHandle, 0xb0, (uint8_t[]) {0x00, 0xF0}, 2);

    esp_lcd_panel_io_tx_param(xLcdIOHandle, LCD_CMD_INVON,NULL, 0);   // 颜色翻转
    esp_lcd_panel_io_tx_param(xLcdIOHandle, LCD_CMD_NORON,NULL, 0);   // 普通显示模式

    uint8_t ucSpinType = 0;    // 设置旋转角度
    switch(pxConfig->ucSpin)
    {
        case 0:
            ucSpinType = 0x00; // 不旋转
            break;
        case 1:
            ucSpinType = 0x60; // 顺时针90
            break;
        case 2:
            ucSpinType = 0xC0; // 180
            break;
        case 3:
            ucSpinType = 0xA0; // 顺时针270,（逆时针90）
            break;
        default:break;
    }
    esp_lcd_panel_io_tx_param(xLcdIOHandle, LCD_CMD_MADCTL, (uint8_t[]) {ucSpinType,}, 1); 
    vTaskDelay(pdMS_TO_TICKS(150));
    esp_lcd_panel_io_tx_param(xLcdIOHandle, LCD_CMD_DISPON, NULL, 0); // 开启显示
    vTaskDelay(pdMS_TO_TICKS(300));
    return ESP_OK;
}

/** st7789 写入显示数据
 * @param x1,x2,y1,y2:显示区域
 * @return 无
*/
void vSt7789Flush(int x1, int x2, int y1, int y2, void *pvData)
{
    /* 检查显示区域是否有效（宽度和高度必须为正数），如果区域无效，调用完成回调函数并直接返回*/
    if(x2 <= x1 || y2 <= y1)
    {
        if(xFlushDoneCallback)
            xFlushDoneCallback(NULL);
        return;
    }
    /* 设置列地址范围 */
    esp_lcd_panel_io_tx_param(xLcdIOHandle, LCD_CMD_CASET, (uint8_t[]) {
        (x1 >> 8) & 0xFF,
        x1 & 0xFF,
        ((x2 - 1) >> 8) & 0xFF,
        (x2 - 1) & 0xFF,
    }, 4);
    /* 设置行地址范围 */
    esp_lcd_panel_io_tx_param(xLcdIOHandle, LCD_CMD_RASET, (uint8_t[]) {
        (y1 >> 8) & 0xFF,
        y1 & 0xFF,
        ((y2 - 1) >> 8) & 0xFF,
        (y2 - 1) & 0xFF,
    }, 4);
    /* 写入显示数据 */ 
    size_t xLength = (x2 - x1) * (y2 - y1) * 2; // 计算需要传输的数据长度：宽度 × 高度 × 2字节, 每个像素占2字节
    esp_lcd_panel_io_tx_color(xLcdIOHandle, LCD_CMD_RAMWR, pvData, xLength);
    return ;
}

/** 控制背光
 * @param bEnable 是否使能背光
 * @return 无
*/
void vSt7789LcdBackLight(bool bEnable)
{
    if(bEnable){
        gpio_set_level(xBLGPIO,1);
    }else{
        gpio_set_level(xBLGPIO,0);
    }
}

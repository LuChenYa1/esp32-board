#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/gpio.h"
#include "lv_port.h"
#include "lvgl.h"
#include "st7789_driver.h"
#include "cst816t_driver.h"

/* 
 * 移植步骤
   1、创建并初始化 LVGL 显示驱动
   2、创建并初始化 LVGL 触摸驱动
   3、初始化 st7789 硬件接口
   4、初始化 cst816 硬件接口
   5、提供一个定时器给 LVGL 使用
 */

static lv_disp_drv_t xDisplayDriver;
static const char *TAG = "lv_port";

#define LCD_WIDTH   280
#define LCD_HEIGHT  240

/**
 * @brief LVGL 定时器时钟
 *
 * @param pvParam 无用
 */
static void prvLvTickIncCallback(void *pvData)
{
    uint32_t ulTickIncPeriodMS = *((uint32_t *) pvData);
    lv_tick_inc(ulTickIncPeriodMS);
}

/**
 * @brief 通知 LVGL 写入数据完毕
 */
static void prvLvPortFlushReady(void* param)
{
    lv_disp_flush_ready(&xDisplayDriver);

    /* portYIELD_FROM_ISR (true) or not (false). */
}

/**
 * @brief 写入显示数据
 *
 * @param xDisplayDriver  对应的显示器
 * @param area      显示区域
 * @param color_p   显示数据
 */
static void prvDisplayFlush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    (void) disp_drv;
    vSt7789Flush(area->x1 + 20, area->x2 + 1 + 20, area->y1,area->y2 + 1, color_p);
}

/**
 * @brief 注册 LVGL 显示驱动
 *
 */
static void prvLvPortDisplayInit(void)
{
    static lv_disp_draw_buf_t xDrawBufferDsc;
    size_t xDispBufHeight = 40;

    /* 必须从内部RAM分配显存，这样刷新速度快 */
    lv_color_t *pxDispBuffer1 = heap_caps_malloc(LCD_WIDTH * xDispBufHeight * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    lv_color_t *pxDispBuffer2 = heap_caps_malloc(LCD_WIDTH * xDispBufHeight * sizeof(lv_color_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    ESP_LOGI(TAG, "Try allocate two %u * %u display buffer, size:%u Byte", LCD_WIDTH, xDispBufHeight, LCD_WIDTH * xDispBufHeight * sizeof(lv_color_t) * 2);
    if (NULL == pxDispBuffer1 || NULL == pxDispBuffer2) {
        ESP_LOGE(TAG, "No memory for LVGL display buffer");
        esp_system_abort("Memory allocation failed");
    }

    /* 初始化显示缓存 */
    lv_disp_draw_buf_init(&xDrawBufferDsc, pxDispBuffer1, pxDispBuffer2, LCD_WIDTH * xDispBufHeight);

    /* 初始化显示驱动 */
    lv_disp_drv_init(&xDisplayDriver);

    /* 设置水平和垂直宽度 */
    xDisplayDriver.hor_res = LCD_WIDTH;       //水平宽度
    xDisplayDriver.ver_res = LCD_HEIGHT;      //垂直宽度

    /* 设置刷新数据函数 */
    xDisplayDriver.flush_cb = prvDisplayFlush;

    /* 设置显示缓存 */
    xDisplayDriver.draw_buf = &xDrawBufferDsc;

    /* 注册显示驱动 */
    lv_disp_drv_register(&xDisplayDriver);
}


/**
 * @brief 获取触摸坐标
 *
 * @param xIndevDriver  触摸驱动
 * @param pvData      数据
 * @return 无
 */
void IRAM_ATTR vIndevRead(struct _lv_indev_drv_t * indev_drv, lv_indev_data_t * pxData)
{
    int16_t x, y;
    int state;
    vCst816tRead(&x, &y, &state);
    pxData->point.x = y; // 旋转了90度
    if(x == 0)
        x = 1;
    pxData->point.y = LCD_HEIGHT - x;
    pxData->state = state;
}

/**
 * @brief 注册 LVGL 输入驱动
 *
 * @return esp_err_t
 */
static esp_err_t prvLvPortIndevInit(void)
{
    static lv_indev_drv_t xIndevDriver;
    lv_indev_drv_init(&xIndevDriver);
    xIndevDriver.type = LV_INDEV_TYPE_POINTER;
    xIndevDriver.read_cb = vIndevRead;
    lv_indev_drv_register(&xIndevDriver);
    return ESP_OK;
}

/**
 * @brief 创建LVGL定时器
 *
 * @return esp_err_t
 */
static esp_err_t prvLvPortTickInit(void)
{
    static uint32_t ulTickIncPeriodMS = 5;
    const esp_timer_create_args_t xPeriodicTimerArgs = {
        .callback = prvLvTickIncCallback,
        .name = "",
        .arg = &ulTickIncPeriodMS,
        .dispatch_method = ESP_TIMER_TASK,
        .skip_unhandled_events = true,
    };

    esp_timer_handle_t xPeriodicTimer;
    ESP_ERROR_CHECK(esp_timer_create(&xPeriodicTimerArgs, &xPeriodicTimer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(xPeriodicTimer, ulTickIncPeriodMS * 1000));

    return ESP_OK;
}

/**
 * @brief LCD 显示接口初始化
 *
 * @return NULL
 */
static void prvLcdInit(void)
{
    St7789Config_t xSt7789Config;
    xSt7789Config.xMOSI = GPIO_NUM_19;
    xSt7789Config.xClk = GPIO_NUM_18;
    xSt7789Config.xCS = GPIO_NUM_5;
    xSt7789Config.xDC = GPIO_NUM_17;
    xSt7789Config.xRst = GPIO_NUM_21;
    xSt7789Config.xBL = GPIO_NUM_26;
    xSt7789Config.ulSPIFreq = 40*1000*1000;             // SPI 时钟频率
    xSt7789Config.uiWidth = LCD_WIDTH;                  // 屏宽
    xSt7789Config.uiHeight = LCD_HEIGHT;                // 屏高
    xSt7789Config.ucSpin = 1;                           // 顺时针旋转90度
    xSt7789Config.pvDoneCallback = prvLvPortFlushReady; // 数据写入完成回调函数
    xSt7789Config.pvCallbackParam = &xDisplayDriver;          // 回调函数参数

    xSt7789DriverHwInit(&xSt7789Config);
}

/**
 * @brief LCD触摸接口初始化
 *
 * @return NULL
 */
static void prvCst816tInit(void)
{
    Cst816tConfig_t xCst816tConfig;
    xCst816tConfig.xSDA = GPIO_NUM_23;
    xCst816tConfig.xSCL = GPIO_NUM_22;
    xCst816tConfig.uiXLimit = LCD_HEIGHT; // 由于屏幕显示旋转了90°，X和Y触摸需要调转
    xCst816tConfig.uiYLimit = LCD_WIDTH;
    //xCst816tConfig.uiXLimit = LCD_WIDTH;
    //xCst816tConfig.uiYLimit = LCD_HEIGHT;
    xCst816tConfig.ulFreq = 200 * 1000;
    
    xCst816tInit(&xCst816tConfig);
}

/**
 * @brief LVGL 端口初始化
 *
 * @return esp_err_t
 */
esp_err_t xLvPortInit(void)
{
    /* 初始化 LVGL 库 */
    lv_init();

    /* 1、lcd 显示接口（st7789）初始化 */
    prvLcdInit();

    /* 2、注册显示驱动 */
    prvLvPortDisplayInit();

    /* 3、lcd 触摸接口（cst816t）初始化 */
    prvCst816tInit();

    /* 4、注册触摸驱动 */
    prvLvPortIndevInit();

    /* 5、初始化 LVGL 定时器 */
    prvLvPortTickInit();

    return ESP_OK;
}

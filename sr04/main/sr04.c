/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "sr04.h"
#include "esp_log.h"
#include "esp_private/esp_clk.h"

static const char *TAG = "SR04";

/**
 * @brief 生成Trig引脚脉冲以启动测量
 */
static void prvSR04TrigOutput(gpio_num_t xTrigGPIO)
{
    gpio_set_level(xTrigGPIO, 1); // 设置高电平
    esp_rom_delay_us(10);
    gpio_set_level(xTrigGPIO, 0); // 设置低电平
}

/**
 * @brief 回声捕获回调函数
 */
static bool prvSR04EchoCallback(mcpwm_cap_channel_handle_t xCaptureChannel, 
                              const mcpwm_capture_event_data_t *xEventData, 
                              void *pvUserData)
{
    static uint32_t uxCaptureBeginValue = 0;
    static uint32_t uxCaptureEndValue = 0;
    SR04_t *pxSR04 = (SR04_t *)pvUserData;
    BaseType_t xHighTaskWakeup = pdFALSE;

    // 判断边沿，上升沿记录捕获的计数值
    if (xEventData->cap_edge == MCPWM_CAP_EDGE_POS) {
        // 存储正边沿检测到的时间戳
        uxCaptureBeginValue = xEventData->cap_value;
        uxCaptureEndValue = uxCaptureBeginValue;
    } else {
        // 如果是下降沿，也记录捕获的计数值
        uxCaptureEndValue = xEventData->cap_value;

        // 两个计数值的差值，就是脉宽长度
        uint32_t uxTofTicks = uxCaptureEndValue - uxCaptureBeginValue;

        // 通知测距任务计数差值
        if (pxSR04->xTaskHandle) {
            xTaskNotifyFromISR(pxSR04->xTaskHandle, uxTofTicks, eSetValueWithOverwrite, &xHighTaskWakeup);
        }
    }

    return xHighTaskWakeup == pdTRUE;
}

/**
 * @brief 初始化SR04传感器
 */
esp_err_t xSR04Init(SR04_t *pxSR04, gpio_num_t xTrigGPIO, gpio_num_t xEchoGPIO)
{
    esp_err_t xRet = ESP_OK;
    
    if (pxSR04 == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    // 初始化结构体
    pxSR04->xTrigGPIO = xTrigGPIO;
    pxSR04->xEchoGPIO = xEchoGPIO;
    pxSR04->xCaptureTimer = NULL;
    pxSR04->xCaptureChannel = NULL;
    pxSR04->xTaskHandle = xTaskGetCurrentTaskHandle();

    ESP_LOGI(TAG, "Install capture timer");

    /* 新建一个捕获定时器，使用默认的时钟源 */
    mcpwm_capture_timer_config_t xCaptureTimerConfig = {
        .clk_src = MCPWM_CAPTURE_CLK_SRC_DEFAULT,
        .group_id = 0,
    };
    xRet = mcpwm_new_capture_timer(&xCaptureTimerConfig, &pxSR04->xCaptureTimer);
    if (xRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create capture timer");
        return xRet;
    }

    /* 新建一个捕获通道，把捕获定时器与捕获通道绑定起来，采取双边捕获的策略 */
    ESP_LOGI(TAG, "Install capture channel");
    mcpwm_capture_channel_config_t xCaptureChannelConfig = {
        .gpio_num = pxSR04->xEchoGPIO,
        .prescale = 1,
        // capture on both edge
        .flags.neg_edge = true,
        .flags.pos_edge = true,
        // pull up internally
        .flags.pull_up = true,
    };
    xRet = mcpwm_new_capture_channel(pxSR04->xCaptureTimer, &xCaptureChannelConfig, &pxSR04->xCaptureChannel);
    if (xRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create capture channel");
        goto error;
    }

    /* 新建一个捕获事件回调函数，当捕获到相关的边沿，则调用这个回调函数 */
    ESP_LOGI(TAG, "Register capture callback");
    mcpwm_capture_event_callbacks_t xCallbackStruct = {
        .on_cap = prvSR04EchoCallback,
    };
    xRet = mcpwm_capture_channel_register_event_callbacks(pxSR04->xCaptureChannel, &xCallbackStruct, pxSR04);
    if (xRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register capture callback");
        goto error;
    }

    /* 使能并开启捕获定时器 */
    ESP_LOGI(TAG, "Enable and start capture timer");
    xRet = mcpwm_capture_timer_enable(pxSR04->xCaptureTimer);
    if (xRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable capture timer");
        goto error;
    }
    
    xRet = mcpwm_capture_timer_start(pxSR04->xCaptureTimer);
    if (xRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start capture timer");
        goto error;
    }

    /* 使能捕获通道 */
    ESP_LOGI(TAG, "Enable capture channel");
    xRet = mcpwm_capture_channel_enable(pxSR04->xCaptureChannel);
    if (xRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable capture channel");
        goto error;
    }

    /* 初始化Trig引脚，设置为输出 */
    ESP_LOGI(TAG, "Configure Trig pin");
    gpio_config_t xGPIOConfig = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << pxSR04->xTrigGPIO,
    };
    xRet = gpio_config(&xGPIOConfig);
    if (xRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure Trig GPIO");
        goto error;
    }
    xRet = gpio_set_level(pxSR04->xTrigGPIO, 0);
    if (xRet != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set Trig GPIO level");
        goto error;
    }

    ESP_LOGI(TAG, "SR04 initialized successfully");
    return ESP_OK;

error:
    /* 一旦报错，进行反初始化，清除所有设置 */
    xSR04Deinit(pxSR04);
    return xRet;
}

/**
 * @brief 反初始化SR04传感器
 */
esp_err_t xSR04Deinit(SR04_t *sr04)
{
    if (sr04 == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    if (sr04->xCaptureChannel) {
        mcpwm_capture_channel_disable(sr04->xCaptureChannel);
        mcpwm_del_capture_channel(sr04->xCaptureChannel);
        sr04->xCaptureChannel = NULL;
    }

    if (sr04->xCaptureTimer) {
        mcpwm_capture_timer_disable(sr04->xCaptureTimer);
        mcpwm_capture_timer_stop(sr04->xCaptureTimer);
        mcpwm_del_capture_timer(sr04->xCaptureTimer);
        sr04->xCaptureTimer = NULL;
    }

    // 重置Trig引脚
    if (sr04->xTrigGPIO >= 0) {
        gpio_reset_pin(sr04->xTrigGPIO);
    }

    sr04->xTaskHandle = NULL;
    
    ESP_LOGI(TAG, "SR04 deinitialized");
    return ESP_OK;
}

/**
 * @brief 测量距离
 */
esp_err_t xSR04MeasureDistance(SR04_t *xSR04, float *fDistance_CM, uint32_t uxTimeout_MS)
{
    if (xSR04 == NULL || fDistance_CM == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint32_t uxTofTicks;
    
    /* 产生一个Trig脉冲，启动一次测距 */
    prvSR04TrigOutput(xSR04->xTrigGPIO);

    /* 等待捕获完成信号 */
    if (xTaskNotifyWait(0x00, ULONG_MAX, &uxTofTicks, pdMS_TO_TICKS(uxTimeout_MS)) == pdTRUE) {
        // 计算脉宽时间长度
        /*
            tof_ticks:计数
            esp_clk_apb_freq():计数时钟频率
            1/esp_clk_apb_freq():计数时钟周期，也就是1个tof_ticks时间是多少秒
            1000000/esp_clk_apb_freq():一个tof_ticks时间是多少us
            因此tof_ticks * (1000000.0 / esp_clk_apb_freq())就是总的脉宽时长
        */
        float fPulseWidth_US = uxTofTicks * (1000000.0 / esp_clk_apb_freq());
        
        if (fPulseWidth_US > 35000) {
            // 脉宽太长，超出了SR04的计算范围
            ESP_LOGD(TAG, "Pulse width too long: %.2f us", fPulseWidth_US);
            return ESP_ERR_INVALID_SIZE;
        }
        
        // 计算距离：D = T / 58 cm
        /*
            计算公式如下：
            距离D=音速V*T/2;(T是上述的脉宽时间，因为是来回时间，所以要除以2才是单程时间)
            D(单位cm)=340m/s*Tus/2;(Tus是以微秒为单位的时间)
            D=34000cm/s*(10^-6)Ts/2;(1us=(10^-6)s)
            D=17000cm*(10^-6)*T
            D=0.017cm*T
            D=T/58cm
            所以下面这个公式/58是这样来的
        */
        *fDistance_CM = fPulseWidth_US / 58.0f;
        
        ESP_LOGD(TAG, "Measured distance: %.2f cm, pulse width: %.2f us", *fDistance_CM, fPulseWidth_US);
        return ESP_OK;
    } else {
        ESP_LOGD(TAG, "Measurement timeout");
        return ESP_ERR_TIMEOUT;
    }
}
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/rmt_rx.h>
#include <driver/rmt_tx.h>
#include <soc/rmt_reg.h>
#include "driver/gpio.h" 
#include <esp_log.h>
#include <freertos/queue.h>
#include <freertos/event_groups.h>
#include "esp32/rom/ets_sys.h"
#include "driver/ledc.h"

/* 定义 LED 的 GPIO 口 */
#define LED_GPIO  GPIO_NUM_27

#define TAG     "LEDC"

/* LED 闪烁运行任务 */
void led_run_task(void* param)
{
    int iGPIOLevel = 0;
    while(1)
    {
        vTaskDelay(pdMS_TO_TICKS(500));
        gpio_set_level(LED_GPIO,iGPIOLevel);
        iGPIOLevel = iGPIOLevel?0:1;
    }
}

/* LED 闪烁初始化 */
void led_flash_init(void)
{
    gpio_config_t led_gpio_cfg = {
        .pin_bit_mask = (1<<LED_GPIO),          //指定GPIO
        .mode = GPIO_MODE_OUTPUT,               //设置为输出模式
        .pull_up_en = GPIO_PULLUP_DISABLE,      //禁止上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE,  //禁止下拉
        .intr_type = GPIO_INTR_DISABLE,         //禁止中断
    };
    gpio_config(&led_gpio_cfg);

    xTaskCreatePinnedToCore(led_run_task, "led", 2048, NULL, 3, NULL, 1);
}


#define LEDC_TIMER              LEDC_TIMER_0            //定时器0
#define LEDC_MODE               LEDC_LOW_SPEED_MODE     //低速模式
#define LEDC_OUTPUT_IO          (LED_GPIO)              //选择GPIO端口
#define LEDC_CHANNEL            LEDC_CHANNEL_0          //PWM通道
#define LEDC_DUTY_RES           LEDC_TIMER_13_BIT       //分辨率
#define LEDC_DUTY               (4095)                  //渐变到目标占空比值，这里设置为2^12-1,最大可设置为2^13-1
#define LEDC_FREQUENCY          (5000)                  //PWM周期

//用于通知渐变完成
static EventGroupHandle_t   s_ledc_ev = NULL;

//关灯完成事件标志( 事件组第0位 )
#define LEDC_OFF_EV  (1UL<<0)

//开灯完成事件标志( 事件组第1位 )
#define LEDC_ON_EV   (1UL<<1)

/* 渐变完成回调函数 
 * IRAM_ATTR 放在中断服务函数前面，表示该函数放在内部RAM中执行，提升执行效率
 */
bool IRAM_ATTR ledc_finish_cb(const ledc_cb_param_t *param, void *user_arg) 
{
    BaseType_t xHigherPriorityTaskWoken;
    if(param->duty){
        /* 完成占空比从 0 到 LEDC_DUTY 的渐变*/
        xEventGroupSetBitsFromISR(s_ledc_ev,LEDC_ON_EV,&xHigherPriorityTaskWoken);
    }else{
        /* 完成占空比从 LEDC_DUTY 到 0 的渐变*/
        xEventGroupSetBitsFromISR(s_ledc_ev,LEDC_OFF_EV,&xHigherPriorityTaskWoken);
    }

    if(xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
    /* 
     * 这个返回值是一个布尔值，表示是否有更高优先级的任务被唤醒。
     * 如果返回值为pdTRUE，则意味着在ISR中唤醒了一个更高优先级的任务，
     * 因此可能需要执行一次上下文切换（通常通过portYIELD_FROM_ISR())
    */
    return xHigherPriorityTaskWoken;
}

/* ledc 渐变任务 */
void ledc_breath_task(void* param)
{
    /* 该变量不需要初始化，因为它总能被 xEventGroupWaitBits 函数设置一个确定的值*/
    EventBits_t ev;
    while(1){
        /* 
         * 如果成功等到事件，返回事件组中被设置的位掩码
         * 如果超时未等到事件, 返回事件组当前的值（0）
         * FreeRTOS保证函数总是返回一个确定的事件位掩码
         */
        ev = xEventGroupWaitBits(s_ledc_ev,LEDC_ON_EV|LEDC_OFF_EV,pdTRUE,pdFALSE,pdMS_TO_TICKS(5000));
        if(ev){
            if(ev & LEDC_OFF_EV){      // 检测到灯关闭，设置LEDC开灯渐变
                ledc_set_fade_with_time(LEDC_MODE,LEDC_CHANNEL,LEDC_DUTY,2000);
                ledc_fade_start(LEDC_MODE,LEDC_CHANNEL,LEDC_FADE_NO_WAIT);
            }else if(ev & LEDC_ON_EV){ // 检测到灯打开，设置LEDC关灯渐变
                ledc_set_fade_with_time(LEDC_MODE,LEDC_CHANNEL,0,2000);
                ledc_fade_start(LEDC_MODE,LEDC_CHANNEL,LEDC_FADE_NO_WAIT);
            }
            /* 再次设置回调函数
             * 每个回调函数在注册后只会执行一次
             * 当渐变完成触发回调后，系统会自动注销该回调函数
             * 如果要再次接收渐变完成事件，必须重新注册回调函数
            */
            ledc_cbs_t cbs = {.fade_cb=ledc_finish_cb,};
            ledc_cb_register(LEDC_MODE,LEDC_CHANNEL,&cbs,NULL);
        }
    }
}

/* LED 呼吸灯初始化(借助 ledc 实现, 无需对 GPIO 引脚初始化) */
void led_breath_init(void)
{
    /* 1、初始化一个定时器 */
    ledc_timer_config_t xLedcTimer = {
        .speed_mode       = LEDC_MODE,      // 低速模式
        .timer_num        = LEDC_TIMER,     // 定时器ID
        .duty_resolution  = LEDC_DUTY_RES,  // 占空比分辨率，这里是13位，2^13-1
        .freq_hz          = LEDC_FREQUENCY, // PWM频率,这里是5KHZ
        .clk_cfg          = LEDC_AUTO_CLK   // 自动选择时钟源
    };
    /* 
     * 对大多数返回 esp_err_t 的 ESP-IDF 函数（特别是初始化函数）的返回值都使用 ESP_ERROR_CHECK() 进行包装。
     * 这能确保在开发阶段快速发现并定位配置错误或硬件问题
     */
    ESP_ERROR_CHECK(ledc_timer_config(&xLedcTimer));

    /* 2、初始化 ledc 通道 */
    ledc_channel_config_t xLedcChannel = {
        .speed_mode     = LEDC_MODE,        // 低速模式
        .channel        = LEDC_CHANNEL,     // PWM 通道0-7
        .timer_sel      = LEDC_TIMER,       // 关联定时器，也就是上面初始化好的那个定时器
        .gpio_num       = LEDC_OUTPUT_IO,   // 设置输出PWM方波的GPIO管脚
        .intr_type      = LEDC_INTR_DISABLE,// 不使能中断
        .duty           = 0,                // 设置默认占空比为0
        .hpoint         = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&xLedcChannel));

    /* 3、(可选)安装硬件渐变服务 */
    ledc_fade_func_install(0);

    /* 4、(可选)配置 ledc 渐变 */
    ledc_set_fade_with_time(LEDC_MODE,LEDC_CHANNEL,LEDC_DUTY,2000);// 占空比从0渐变到 LEDC_DUTY，时间2000ms

    /* 5、启动渐变 */
    ledc_fade_start(LEDC_MODE,LEDC_CHANNEL,LEDC_FADE_NO_WAIT);

    /* 6、设置渐变完成回调函数 */
    ledc_cbs_t cbs = {.fade_cb=ledc_finish_cb,};
    ledc_cb_register(LEDC_MODE,LEDC_CHANNEL,&cbs,NULL);

    /* 7、创建一个事件组，用于通知任务渐变完成 */
    s_ledc_ev = xEventGroupCreate();

    xTaskCreatePinnedToCore(ledc_breath_task,"ledc",2048,NULL,3,NULL,1);
}


// 主函数
void app_main(void)
{
	//led_flash_init();     //简单led闪烁
    led_breath_init();      //呼吸灯
}
